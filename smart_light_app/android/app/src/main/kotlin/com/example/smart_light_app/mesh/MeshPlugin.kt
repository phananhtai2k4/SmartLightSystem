package com.example.smart_light_app.mesh

import android.Manifest
import android.app.Activity
import android.bluetooth.*
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.PluginRegistry
import kotlinx.coroutines.*
import java.util.*

/**
 * MeshPlugin - Flutter Platform Channel bridge for BLE Mesh operations
 * Uses Nordic BLE Library (BleMeshManager) for reliable GATT operations
 */
class MeshPlugin : FlutterPlugin, MethodChannel.MethodCallHandler, 
    ActivityAware, PluginRegistry.RequestPermissionsResultListener {
    
    companion object {
        private const val TAG = "MeshPlugin"
        private const val METHOD_CHANNEL = "smart_light/mesh"
        private const val EVENT_CHANNEL_SCAN = "smart_light/mesh/scan"
        private const val EVENT_CHANNEL_PROVISION = "smart_light/mesh/provision"
        
        private const val PERMISSION_REQUEST_CODE = 1001
    }
    
    private var context: Context? = null
    private var activity: Activity? = null
    private var methodChannel: MethodChannel? = null
    private var scanEventChannel: EventChannel? = null
    private var provisionEventChannel: EventChannel? = null
    
    private var scanEventSink: EventChannel.EventSink? = null
    private var provisionEventSink: EventChannel.EventSink? = null
    
    private var meshManager: NrfMeshManager? = null
    private var bleScanner: BleScanner? = null
    private var bleMeshManager: BleMeshManager? = null
    
    private var currentDevice: BluetoothDevice? = null
    private var pendingServiceData: ByteArray? = null
    
    private val mainHandler = Handler(Looper.getMainLooper())
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    
    // Pending permission callback
    private var pendingPermissionResult: MethodChannel.Result? = null
    
    // ==================== FlutterPlugin ====================
    
    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        Log.d(TAG, "onAttachedToEngine")
        context = binding.applicationContext
        
        // Setup method channel
        methodChannel = MethodChannel(binding.binaryMessenger, METHOD_CHANNEL)
        methodChannel?.setMethodCallHandler(this)
        
        // Setup event channels
        scanEventChannel = EventChannel(binding.binaryMessenger, EVENT_CHANNEL_SCAN)
        scanEventChannel?.setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                scanEventSink = events
            }
            override fun onCancel(arguments: Any?) {
                scanEventSink = null
            }
        })
        
        provisionEventChannel = EventChannel(binding.binaryMessenger, EVENT_CHANNEL_PROVISION)
        provisionEventChannel?.setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                provisionEventSink = events
            }
            override fun onCancel(arguments: Any?) {
                provisionEventSink = null
            }
        })
    }
    
    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        Log.d(TAG, "onDetachedFromEngine")
        cleanup()
        methodChannel?.setMethodCallHandler(null)
        methodChannel = null
        scanEventChannel = null
        provisionEventChannel = null
        context = null
    }
    
    // ==================== ActivityAware ====================
    
    override fun onAttachedToActivity(binding: ActivityPluginBinding) {
        Log.d(TAG, "onAttachedToActivity")
        activity = binding.activity
        binding.addRequestPermissionsResultListener(this)
    }
    
    override fun onDetachedFromActivityForConfigChanges() {
        activity = null
    }
    
    override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
        activity = binding.activity
        binding.addRequestPermissionsResultListener(this)
    }
    
    override fun onDetachedFromActivity() {
        activity = null
    }
    
    // ==================== MethodCallHandler ====================
    
    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        Log.d(TAG, "onMethodCall: ${call.method}")
        
        when (call.method) {
            "hasPermissions" -> hasPermissions(result)
            "requestPermissions" -> requestPermissions(result)
            "isBluetoothEnabled" -> isBluetoothEnabled(result)
            "startScan" -> startScan(result)
            "stopScan" -> stopScan(result)
            "provisionDevice" -> {
                val address = call.argument<String>("address")
                val name = call.argument<String>("name")
                val serviceDataHex = call.argument<String>("serviceData")
                if (address != null) {
                    provisionDevice(address, name, serviceDataHex, result)
                } else {
                    result.error("INVALID_ARGUMENT", "Address is required", null)
                }
            }
            "cancelProvisioning" -> cancelProvisioning(result)
            "getProvisionedNodes" -> getProvisionedNodes(result)
            else -> result.notImplemented()
        }
    }
    
    // ==================== Permission Handling ====================
    
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ): Boolean {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            val allGranted = grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }
            pendingPermissionResult?.success(allGranted)
            pendingPermissionResult = null
            return true
        }
        return false
    }
    
    private fun hasPermissions(result: MethodChannel.Result) {
        val ctx = context ?: run {
            result.error("NO_CONTEXT", "Context is null", null)
            return
        }
        
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        }
        
        val allGranted = permissions.all {
            ContextCompat.checkSelfPermission(ctx, it) == PackageManager.PERMISSION_GRANTED
        }
        
        result.success(allGranted)
    }

    private fun requestPermissions(result: MethodChannel.Result) {
        val ctx = context ?: run {
            result.error("NO_CONTEXT", "Context is null", null)
            return
        }
        val act = activity ?: run {
            result.error("NO_ACTIVITY", "Activity is null", null)
            return
        }
        
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        }
        
        val allGranted = permissions.all {
            ContextCompat.checkSelfPermission(ctx, it) == PackageManager.PERMISSION_GRANTED
        }
        
        if (allGranted) {
            result.success(true)
        } else {
            pendingPermissionResult = result
            ActivityCompat.requestPermissions(act, permissions, PERMISSION_REQUEST_CODE)
        }
    }
    
    private fun isBluetoothEnabled(result: MethodChannel.Result) {
        val bluetoothManager = context?.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        val enabled = bluetoothManager?.adapter?.isEnabled == true
        result.success(enabled)
    }
    
    // ==================== BLE Scanning ====================
    
    private fun startScan(result: MethodChannel.Result) {
        val ctx = context ?: run {
            result.error("NO_CONTEXT", "Context is null", null)
            return
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                result.error("NO_PERMISSION", "BLUETOOTH_SCAN permission is required", null)
                return
            }
        }
        
        if (bleScanner == null) {
            bleScanner = BleScanner(
                onDeviceFound = { device ->
                    Log.d(TAG, "Sending deviceFound event: ${device.name} (${device.device.address})")
                    sendScanEvent(mapOf(
                        "type" to "deviceFound",
                        "device" to mapOf(
                            "name" to (device.name ?: "Unknown"),
                            "address" to device.device.address,
                            "rssi" to device.rssi,
                            "macSuffix" to device.macSuffix,
                            "uuid" to device.deviceUuid?.toString(),
                            "serviceData" to (device.serviceData?.let { bytesToHex(it) } ?: "")
                        )
                    ))
                },
                onScanFailed = { errorCode ->
                    Log.e(TAG, "BLE scan failed: $errorCode")
                    sendScanEvent(mapOf(
                        "type" to "scanFailed",
                        "errorCode" to errorCode
                    ))
                }
            )
        }
        
        // Use startScanNoFilter() for broader discovery
        // Some devices may not properly advertise mesh service UUID
        bleScanner?.startScanNoFilter()
        result.success(true)
    }
    
    private fun stopScan(result: MethodChannel.Result) {
        bleScanner?.stopScan()
        result.success(true)
    }
    
    // ==================== Mesh Provisioning ====================
    
    private fun ensureMeshManagerInitialized(): NrfMeshManager? {
        if (meshManager == null) {
            val ctx = context ?: return null
            Log.d(TAG, "Lazy initializing mesh manager...")
            meshManager = NrfMeshManager(ctx).apply {
                // Setup callbacks for sending PDUs
                onSendProvisioningPdu = { device, pdu ->
                    Log.d(TAG, "Sending provisioning PDU: ${pdu.size} bytes")
                    bleMeshManager?.sendPdu(pdu)
                }
                onSendMeshPdu = { device, pdu ->
                    Log.d(TAG, "Sending mesh PDU: ${pdu.size} bytes")
                    bleMeshManager?.sendPdu(pdu)
                }
                onGetMtu = {
                    // Return MTU - 3 (BLE payload size) for proper PDU segmentation
                    // SDK uses this to segment PDUs at correct boundaries
                    val mtu = bleMeshManager?.mtuSize ?: 23
                    maxOf(mtu - 3, 20) // Minimum 20 bytes payload
                }
                onProvisioningComplete = { unicastAddress, deviceKey ->
                    Log.d(TAG, "Provisioning complete! Address: 0x${unicastAddress.toString(16)}")
                    sendProvisioningEvent(mapOf(
                        "status" to "success",
                        "unicastAddress" to unicastAddress,
                        "deviceKey" to deviceKey
                    ))
                }
                onProvisioningFailed = { error ->
                    Log.e(TAG, "Provisioning failed: $error")
                    sendProvisioningEvent(mapOf(
                        "status" to "failed",
                        "error" to error
                    ))
                }
                initializeMeshNetwork()
            }
        }
        return meshManager
    }
    
    private fun ensureBleMeshManager(): BleMeshManager {
        if (bleMeshManager == null) {
            val ctx = context ?: throw IllegalStateException("Context is null")
            bleMeshManager = BleMeshManager(ctx).apply {
                // Data received from device -> forward to mesh manager
                // Use payload size (MTU - 3) for correct SAR reassembly
                // SDK's removeSegmentation() uses this to calculate chunk boundaries
                onDataReceived = { pdu ->
                    Log.d(TAG, "Received PDU: ${pdu.size} bytes")
                    val payloadSize = maxOf(mtuSize - 3, 20)
                    meshManager?.handleNotification(payloadSize, pdu)
                }
                
                // Data sent to device -> notify mesh manager
                // Use payload size for consistency
                onDataSent = { pdu ->
                    Log.d(TAG, "PDU sent: ${pdu.size} bytes")
                    val payloadSize = maxOf(mtuSize - 3, 20)
                    meshManager?.handleWriteCallback(payloadSize, pdu)
                }
                
                // Mesh service discovered
                onMeshServiceDiscovered = { isProvisioning ->
                    Log.d(TAG, "Mesh service discovered, isProvisioning: $isProvisioning")
                    sendProvisioningEvent(mapOf("status" to "serviceDiscovered"))
                    
                    // Start provisioning now that service is ready
                    if (isProvisioning) {
                        startProvisioningAfterConnection()
                    } else {
                        // Proxy service is ready after provisioning complete
                        // Now we can send configuration messages
                        Log.d(TAG, "Proxy service ready, starting configuration after delay...")
                        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                            meshManager?.startConfigurationAfterProxyReady()
                        }, 2000) // 2 second delay like Nordic reference app
                    }
                }
            }
        }
        return bleMeshManager!!
    }
    
    private fun provisionDevice(address: String, name: String?, serviceDataHex: String?, result: MethodChannel.Result) {
        Log.d(TAG, "======== PROVISION DEVICE ========")
        Log.d(TAG, "Address: $address")
        Log.d(TAG, "Name: $name")
        Log.d(TAG, "ServiceDataHex: $serviceDataHex")
        Log.d(TAG, "ServiceDataHex length: ${serviceDataHex?.length ?: 0}")
        
        val ctx = context ?: run {
            result.error("NO_CONTEXT", "Context is null", null)
            return
        }
        
        val bluetoothManager = ctx.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        val adapter = bluetoothManager?.adapter ?: run {
            result.error("NO_BLUETOOTH", "Bluetooth adapter is null", null)
            return
        }
        
        val device = adapter.getRemoteDevice(address)
        currentDevice = device
        
        // Parse service data
        pendingServiceData = serviceDataHex?.let { hexToBytes(it) }
        Log.d(TAG, "Parsed serviceData: ${pendingServiceData?.size ?: 0} bytes")
        pendingServiceData?.let {
            Log.d(TAG, "ServiceData bytes: ${it.joinToString("") { b -> "%02X".format(b) }}")
        }
        
        // Initialize managers
        ensureMeshManagerInitialized()
        val bleManager = ensureBleMeshManager()
        
        sendProvisioningEvent(mapOf("status" to "connecting"))
        
        // Connect using Nordic BLE Library - this handles all GATT complexity!
        bleManager.connect(device)
            .retry(3, 200)
            .useAutoConnect(false)
            .timeout(30000)
            .done { connectedDevice ->
                Log.d(TAG, "Connected to device: ${connectedDevice.address}")
                sendProvisioningEvent(mapOf("status" to "connected"))
            }
            .fail { failedDevice, status ->
                Log.e(TAG, "Connection failed: $status")
                sendProvisioningEvent(mapOf(
                    "status" to "failed",
                    "error" to "Connection failed with status: $status"
                ))
            }
            .enqueue()
        
        result.success(true)
    }
    
    private fun startProvisioningAfterConnection() {
        val device = currentDevice ?: run {
            Log.e(TAG, "No current device for provisioning")
            return
        }
        
        val serviceData = pendingServiceData ?: run {
            Log.e(TAG, "No service data for provisioning")
            return
        }
        
        scope.launch {
            val manager = meshManager ?: run {
                Log.e(TAG, "Mesh manager is null")
                sendProvisioningEvent(mapOf(
                    "status" to "failed",
                    "error" to "Mesh manager not initialized"
                ))
                return@launch
            }
            
            // Wait for network to be ready
            var waitCount = 0
            while (!manager.isNetworkReady() && waitCount < 50) {
                delay(100)
                waitCount++
            }
            
            if (!manager.isNetworkReady()) {
                Log.e(TAG, "Mesh network not ready after timeout")
                sendProvisioningEvent(mapOf(
                    "status" to "failed",
                    "error" to "Mesh network not ready"
                ))
                return@launch
            }
            
            Log.d(TAG, "Starting provisioning process...")
            sendProvisioningEvent(mapOf("status" to "provisioning"))
            manager.startProvisioning(device, serviceData)
        }
    }
    
    private fun cancelProvisioning(result: MethodChannel.Result) {
        Log.d(TAG, "Cancelling provisioning")
        
        bleMeshManager?.disconnect()?.enqueue()
        currentDevice = null
        pendingServiceData = null
        meshManager?.cleanup()
        
        result.success(true)
    }
    
    private fun getProvisionedNodes(result: MethodChannel.Result) {
        val manager = ensureMeshManagerInitialized()
        val nodes = manager?.getProvisionedNodes() ?: emptyList()
        result.success(nodes)
    }
    
    // ==================== Event Helpers ====================
    
    private fun sendScanEvent(event: Map<String, Any?>) {
        mainHandler.post {
            scanEventSink?.success(event)
        }
    }
    
    private fun sendProvisioningEvent(event: Map<String, Any?>) {
        mainHandler.post {
            provisionEventSink?.success(event)
        }
    }
    
    // ==================== Utility ====================
    
    private fun bytesToHex(bytes: ByteArray): String {
        return bytes.joinToString("") { "%02X".format(it) }
    }
    
    private fun hexToBytes(hex: String): ByteArray {
        val cleanHex = hex.replace(" ", "")
        return ByteArray(cleanHex.length / 2) { i ->
            cleanHex.substring(i * 2, i * 2 + 2).toInt(16).toByte()
        }
    }
    
    // ==================== Cleanup ====================
    
    private fun cleanup() {
        bleScanner?.stopScan()
        bleMeshManager?.disconnect()?.enqueue()
        bleMeshManager = null
        currentDevice = null
        pendingServiceData = null
        meshManager?.cleanup()
        meshManager = null
        scope.cancel()
    }
}
