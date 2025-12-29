package com.example.smart_light_app.mesh

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.os.ParcelUuid
import android.util.Log
import no.nordicsemi.android.support.v18.scanner.*
import java.util.UUID

/**
 * Data class for unprovisioned device info
 */
data class UnprovisionedDevice(
    val device: BluetoothDevice,
    val name: String?,
    val rssi: Int,
    val serviceData: ByteArray?,
    val deviceUuid: UUID?
) {
    // Extract MAC suffix from name (GATEWAY_XXXX -> XXXX)
    val macSuffix: String?
        get() = name?.let {
            if (it.startsWith(MeshConstants.GATEWAY_NAME_PREFIX)) {
                it.removePrefix(MeshConstants.GATEWAY_NAME_PREFIX).uppercase()
            } else null
        }
    
    fun toMap(): Map<String, Any?> = mapOf(
        "address" to device.address,
        "name" to name,
        "rssi" to rssi,
        "macSuffix" to macSuffix,
        "uuid" to deviceUuid?.toString()
    )
}

/**
 * BLE Scanner for discovering unprovisioned mesh devices
 */
class BleScanner(
    private val onDeviceFound: (UnprovisionedDevice) -> Unit,
    private val onScanFailed: (Int) -> Unit
) {
    companion object {
        private const val TAG = "BleScanner"
        
        // Mesh Provisioning Service UUID
        val MESH_PROVISIONING_UUID: ParcelUuid = ParcelUuid.fromString("00001827-0000-1000-8000-00805F9B34FB")
        
        // Mesh Proxy Service UUID (for already provisioned devices)
        val MESH_PROXY_UUID: ParcelUuid = ParcelUuid.fromString("00001828-0000-1000-8000-00805F9B34FB")
    }
    
    private val scanner: BluetoothLeScannerCompat = BluetoothLeScannerCompat.getScanner()
    private var isScanning = false
    
    private val discoveredDevices = mutableMapOf<String, UnprovisionedDevice>()
    
    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            processScanResult(result)
        }
        
        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            results.forEach { processScanResult(it) }
        }
        
        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Scan failed with error: $errorCode")
            isScanning = false
            onScanFailed(errorCode)
        }
    }
    
    private fun processScanResult(result: ScanResult) {
        val device = result.device
        val scanRecord = result.scanRecord ?: return
        
        // Get device name
        val name = scanRecord.deviceName ?: device.name
        
        // Skip devices without name (too noisy in logs)
        if (name.isNullOrEmpty()) return
        
        // Log scan results with "GATEWAY" in name
        if (name.contains("GATEWAY", ignoreCase = true)) {
            Log.d(TAG, "Found potential Gateway: name=$name, address=${device.address}, rssi=${result.rssi}")
        }
        
        // Get service data for Mesh Provisioning Service
        val serviceData = scanRecord.getServiceData(MESH_PROVISIONING_UUID)
        
        // Accept device if:
        // 1. Has mesh provisioning service data, OR
        // 2. Has mesh service UUID, OR
        // 3. Name matches GATEWAY_XXXX pattern (our specific use case)
        val isGatewayByName = name.startsWith(MeshConstants.GATEWAY_NAME_PREFIX, ignoreCase = true)
        val hasMeshServiceData = serviceData != null
        val hasMeshServiceUuid = scanRecord.serviceUuids?.any { 
            it.uuid == MESH_PROVISIONING_UUID.uuid || it.uuid == MESH_PROXY_UUID.uuid 
        } == true
        
        if (!hasMeshServiceData && !hasMeshServiceUuid && !isGatewayByName) {
            return // Not a mesh device we're interested in
        }
        
        if (hasMeshServiceData || hasMeshServiceUuid || isGatewayByName) {
            Log.d(TAG, "  Device qualifies: hasMeshServiceData=$hasMeshServiceData, hasMeshServiceUuid=$hasMeshServiceUuid, isGatewayByName=$isGatewayByName")
            Log.d(TAG, "  Service data: ${serviceData?.size ?: 0} bytes")
        }
        
        // Parse device UUID from service data if available (first 16 bytes)
        val deviceUuid = if (serviceData != null && serviceData.size >= 16) {
            val uuidBytes = serviceData.copyOfRange(0, 16)
            bytesToUuid(uuidBytes)
        } else {
            // Generate a UUID from device address if no service data
            UUID.nameUUIDFromBytes(device.address.toByteArray())
        }
        
        // Create unprovisioned device info
        val unprovisionedDevice = UnprovisionedDevice(
            device = device,
            name = name,
            rssi = result.rssi,
            serviceData = serviceData,
            deviceUuid = deviceUuid
        )
        
        // Check if already discovered
        val key = device.address
        if (!discoveredDevices.containsKey(key)) {
            Log.d(TAG, "Found unprovisioned device: $name (${device.address}), RSSI: ${result.rssi}")
            discoveredDevices[key] = unprovisionedDevice
            onDeviceFound(unprovisionedDevice)
        }
    }
    
    /**
     * Start scanning for unprovisioned mesh devices
     */
    fun startScan() {
        if (isScanning) {
            Log.w(TAG, "Already scanning")
            return
        }
        
        Log.d(TAG, "Starting BLE scan for unprovisioned devices...")
        
        // Clear previous results
        discoveredDevices.clear()
        
        // Build scan settings
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .setUseHardwareBatchingIfSupported(false)
            .build()
        
        // Build scan filter for Mesh Provisioning Service UUID
        val filter = ScanFilter.Builder()
            .setServiceUuid(MESH_PROVISIONING_UUID)
            .build()
        
        try {
            scanner.startScan(listOf(filter), settings, scanCallback)
            isScanning = true
            Log.d(TAG, "BLE scan started")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start scan: ${e.message}")
            onScanFailed(-1)
        }
    }
    
    /**
     * Start scanning without filter (find all BLE devices)
     */
    fun startScanNoFilter() {
        if (isScanning) {
            Log.w(TAG, "Already scanning")
            return
        }
        
        Log.d(TAG, "Starting BLE scan (no filter)...")
        
        discoveredDevices.clear()
        
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .build()
        
        try {
            scanner.startScan(null, settings, scanCallback)
            isScanning = true
            Log.d(TAG, "BLE scan started (no filter)")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start scan: ${e.message}")
            onScanFailed(-1)
        }
    }
    
    /**
     * Stop scanning
     */
    fun stopScan() {
        if (!isScanning) {
            return
        }
        
        Log.d(TAG, "Stopping BLE scan...")
        
        try {
            scanner.stopScan(scanCallback)
            isScanning = false
            Log.d(TAG, "BLE scan stopped")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to stop scan: ${e.message}")
        }
    }
    
    /**
     * Check if currently scanning
     */
    fun isScanning(): Boolean = isScanning
    
    /**
     * Get all discovered devices
     */
    fun getDiscoveredDevices(): List<UnprovisionedDevice> = discoveredDevices.values.toList()
    
    /**
     * Find device by MAC suffix (from GATEWAY_XXXX name pattern)
     */
    fun findDeviceByMacSuffix(suffix: String): UnprovisionedDevice? {
        val targetSuffix = suffix.uppercase()
        return discoveredDevices.values.find { device ->
            device.macSuffix?.uppercase() == targetSuffix
        }
    }
    
    /**
     * Find device by name prefix
     */
    fun findDeviceByNamePrefix(prefix: String): UnprovisionedDevice? {
        return discoveredDevices.values.find { device ->
            device.name?.startsWith(prefix, ignoreCase = true) == true
        }
    }
    
    /**
     * Convert bytes to UUID
     */
    private fun bytesToUuid(bytes: ByteArray): UUID? {
        if (bytes.size < 16) return null
        
        var msb: Long = 0
        var lsb: Long = 0
        
        for (i in 0..7) {
            msb = (msb shl 8) or (bytes[i].toLong() and 0xFF)
        }
        for (i in 8..15) {
            lsb = (lsb shl 8) or (bytes[i].toLong() and 0xFF)
        }
        
        return UUID(msb, lsb)
    }
    
    /**
     * Convert bytes to hex string
     */
    private fun bytesToHex(bytes: ByteArray): String {
        return bytes.joinToString("") { "%02X".format(it) }
    }
}
