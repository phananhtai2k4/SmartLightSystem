package com.example.smart_light_app.mesh

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import android.content.Context
import android.util.Log
import no.nordicsemi.android.ble.BleManager
import no.nordicsemi.android.ble.WriteRequest
import no.nordicsemi.android.ble.callback.DataReceivedCallback
import no.nordicsemi.android.ble.callback.DataSentCallback
import no.nordicsemi.android.ble.data.Data
import java.util.*

/**
 * BleMeshManager - Handles BLE GATT connection for Mesh Provisioning/Proxy
 * Uses Nordic BLE Library for reliable GATT operations
 */
class BleMeshManager(context: Context) : BleManager(context) {
    
    companion object {
        private const val TAG = "BleMeshManager"
        
        // Mesh Provisioning Service UUID
        val MESH_PROVISIONING_SERVICE_UUID: UUID = UUID.fromString("00001827-0000-1000-8000-00805F9B34FB")
        val MESH_PROVISIONING_DATA_IN_UUID: UUID = UUID.fromString("00002ADB-0000-1000-8000-00805F9B34FB")
        val MESH_PROVISIONING_DATA_OUT_UUID: UUID = UUID.fromString("00002ADC-0000-1000-8000-00805F9B34FB")
        
        // Mesh Proxy Service UUID
        val MESH_PROXY_SERVICE_UUID: UUID = UUID.fromString("00001828-0000-1000-8000-00805F9B34FB")
        val MESH_PROXY_DATA_IN_UUID: UUID = UUID.fromString("00002ADD-0000-1000-8000-00805F9B34FB")
        val MESH_PROXY_DATA_OUT_UUID: UUID = UUID.fromString("00002ADE-0000-1000-8000-00805F9B34FB")
    }
    
    // Characteristics
    private var dataInCharacteristic: BluetoothGattCharacteristic? = null
    private var dataOutCharacteristic: BluetoothGattCharacteristic? = null
    
    // State
    private var isProvisioningService = false
    private var isServiceChanging = false  // Track service change state
    var mtuSize: Int = 23
        private set
    
    // Queue for PDUs during service change
    private val pendingPduQueue = mutableListOf<ByteArray>()
    
    // Callbacks
    var onDataReceived: ((ByteArray) -> Unit)? = null
    var onDataSent: ((ByteArray) -> Unit)? = null
    var onMeshServiceDiscovered: ((Boolean) -> Unit)? = null
    
    /**
     * Check if connected to provisioning service (vs proxy service)
     */
    fun isProvisioningService(): Boolean = isProvisioningService
    
    /**
     * BleMeshManagerGattCallback handles GATT events
     */
    private inner class BleMeshManagerGattCallback : BleManagerGattCallback() {
        
        override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
            Log.d(TAG, "isRequiredServiceSupported called")
            
            // Try to find Mesh Provisioning Service first
            var service: BluetoothGattService? = gatt.getService(MESH_PROVISIONING_SERVICE_UUID)
            if (service != null) {
                Log.d(TAG, "Found Mesh Provisioning Service")
                isProvisioningService = true
                dataInCharacteristic = service.getCharacteristic(MESH_PROVISIONING_DATA_IN_UUID)
                dataOutCharacteristic = service.getCharacteristic(MESH_PROVISIONING_DATA_OUT_UUID)
            } else {
                // Try Mesh Proxy Service
                service = gatt.getService(MESH_PROXY_SERVICE_UUID)
                if (service != null) {
                    Log.d(TAG, "Found Mesh Proxy Service")
                    isProvisioningService = false
                    dataInCharacteristic = service.getCharacteristic(MESH_PROXY_DATA_IN_UUID)
                    dataOutCharacteristic = service.getCharacteristic(MESH_PROXY_DATA_OUT_UUID)
                }
            }
            
            val hasRequiredCharacteristics = dataInCharacteristic != null && dataOutCharacteristic != null
            Log.d(TAG, "Required characteristics found: $hasRequiredCharacteristics")
            
            return hasRequiredCharacteristics
        }
        
        override fun onServicesInvalidated() {
            Log.d(TAG, "onServicesInvalidated - services changing, will queue PDUs")
            isServiceChanging = true
            dataInCharacteristic = null
            dataOutCharacteristic = null
        }
        
        override fun initialize() {
            Log.d(TAG, "initialize() - setting up notifications and MTU")
            isServiceChanging = false  // Service change complete
            
            // Request high MTU for mesh PDUs
            requestMtu(517)
                .with { device, mtu ->
                    Log.d(TAG, "MTU changed to: $mtu")
                    mtuSize = mtu
                }
                .fail { device, status ->
                    Log.e(TAG, "MTU request failed with status: $status")
                }
                .enqueue()
            
            // Enable notifications on data out characteristic
            setNotificationCallback(dataOutCharacteristic)
                .with(dataReceivedCallback)
            
            enableNotifications(dataOutCharacteristic)
                .done { device ->
                    Log.d(TAG, "Notifications enabled successfully")
                    onMeshServiceDiscovered?.invoke(isProvisioningService)
                    
                    // Send any queued PDUs after service rediscovery
                    sendQueuedPdus()
                }
                .fail { device, status ->
                    Log.e(TAG, "Failed to enable notifications: $status")
                }
                .enqueue()
        }
    }
    
    /**
     * Data received callback
     */
    private val dataReceivedCallback = DataReceivedCallback { device, data ->
        val bytes = data.value
        if (bytes != null && bytes.isNotEmpty()) {
            Log.d(TAG, "Data received: ${bytes.size} bytes")
            onDataReceived?.invoke(bytes)
        }
    }
    
    /**
     * Data sent callback
     */
    private val dataSentCallback = DataSentCallback { device, data ->
        val bytes = data.value
        if (bytes != null) {
            Log.d(TAG, "Data sent: ${bytes.size} bytes")
            onDataSent?.invoke(bytes)
        }
    }
    
    override fun getGattCallback(): BleManagerGattCallback {
        return BleMeshManagerGattCallback()
    }
    
    /**
     * Send PDU to device
     * Uses .split() to automatically chunk large PDUs based on MTU
     */
    fun sendPdu(pdu: ByteArray) {
        Log.d(TAG, "======== SEND PDU VIA BLE ========")
        Log.d(TAG, "PDU size: ${pdu.size} bytes, MTU: $mtuSize")
        Log.d(TAG, "PDU hex: ${pdu.joinToString("") { b -> "%02X".format(b) }}")
        
        val characteristic = dataInCharacteristic
        if (characteristic == null) {
            if (isServiceChanging) {
                Log.w(TAG, "Service changing, queuing PDU for later (${pdu.size} bytes)")
                synchronized(pendingPduQueue) {
                    pendingPduQueue.add(pdu.copyOf())
                }
                Log.d(TAG, "Queued PDU, queue size: ${pendingPduQueue.size}")
            } else {
                Log.e(TAG, "Cannot send PDU - dataInCharacteristic is null and not in service change!")
            }
            return
        }
        
        Log.d(TAG, "Writing to characteristic: ${characteristic.uuid}")
        
        // Use .split() to automatically chunk large PDUs based on MTU
        // This is essential for PDUs larger than MTU-3 (like Public Key which is 68 bytes)
        writeCharacteristic(
            characteristic,
            pdu,
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        )
            .split()  // Split PDU into MTU-sized chunks automatically
            .with(dataSentCallback)
            .fail { device, status ->
                Log.e(TAG, "Failed to send PDU: status=$status")
            }
            .done { device ->
                Log.d(TAG, "PDU write completed successfully")
            }
            .enqueue()
    }
    
    /**
     * Send any queued PDUs after service rediscovery
     */
    private fun sendQueuedPdus() {
        val pdusToSend: List<ByteArray>
        synchronized(pendingPduQueue) {
            if (pendingPduQueue.isEmpty()) {
                Log.d(TAG, "No queued PDUs to send")
                return
            }
            pdusToSend = pendingPduQueue.toList()
            pendingPduQueue.clear()
        }
        
        Log.d(TAG, "======== SENDING ${pdusToSend.size} QUEUED PDUs ========")
        for ((index, pdu) in pdusToSend.withIndex()) {
            Log.d(TAG, "Sending queued PDU ${index + 1}/${pdusToSend.size}: ${pdu.size} bytes")
            sendPdu(pdu)
        }
    }
    
    /**
     * Get max PDU size based on MTU
     * MTU includes 3 bytes overhead, so max payload is MTU - 3
     */
    fun getMaxPduSize(): Int {
        return mtuSize - 3
    }
    
    override fun log(priority: Int, message: String) {
        Log.println(priority, TAG, message)
    }
}
