package com.example.smart_light_app.mesh

import android.bluetooth.BluetoothDevice
import android.content.Context
import android.util.Log
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import no.nordicsemi.android.mesh.*
import no.nordicsemi.android.mesh.provisionerstates.ProvisioningState
import no.nordicsemi.android.mesh.provisionerstates.UnprovisionedMeshNode
import no.nordicsemi.android.mesh.transport.ConfigAppKeyAdd
import no.nordicsemi.android.mesh.transport.ConfigAppKeyStatus
import no.nordicsemi.android.mesh.transport.ConfigCompositionDataGet
import no.nordicsemi.android.mesh.transport.ConfigCompositionDataStatus
import no.nordicsemi.android.mesh.transport.ControlMessage
import no.nordicsemi.android.mesh.transport.MeshMessage
import no.nordicsemi.android.mesh.transport.ProvisionedMeshNode
import java.util.UUID

/**
 * Provisioning state sealed class for tracking progress
 */
sealed class ProvisioningStatus {
    object Idle : ProvisioningStatus()
    object Connecting : ProvisioningStatus()
    object Identifying : ProvisioningStatus()
    object Provisioning : ProvisioningStatus()
    data class ProvisioningProgress(val state: String) : ProvisioningStatus()
    object SendingNetworkKey : ProvisioningStatus()
    object SendingAppKey : ProvisioningStatus()
    object BindingAppKey : ProvisioningStatus()
    data class Success(val unicastAddress: Int, val deviceKey: String) : ProvisioningStatus()
    data class Failed(val error: String) : ProvisioningStatus()
}

/**
 * NrfMeshManager - Manages nRF Mesh SDK operations
 * Handles mesh network creation, provisioning, and configuration
 */
class NrfMeshManager(private val context: Context) : MeshManagerCallbacks, MeshProvisioningStatusCallbacks, MeshStatusCallbacks {
    
    companion object {
        private const val TAG = "NrfMeshManager"
    }
    
    private val meshManagerApi: MeshManagerApi = MeshManagerApi(context)
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    
    // Current state
    private var unprovisionedMeshNode: UnprovisionedMeshNode? = null
    private var provisionedMeshNode: ProvisionedMeshNode? = null
    private var bluetoothDevice: BluetoothDevice? = null
    
    // Provisioning status flow
    private val _provisioningStatus = MutableStateFlow<ProvisioningStatus>(ProvisioningStatus.Idle)
    val provisioningStatus: StateFlow<ProvisioningStatus> = _provisioningStatus
    
    // Network ready state
    @Volatile
    private var isNetworkReady = false
    private val networkReadyCallbacks = mutableListOf<() -> Unit>()
    
    // Callbacks for BLE operations (to be set by MeshPlugin)
    var onSendProvisioningPdu: ((BluetoothDevice, ByteArray) -> Unit)? = null
    var onSendMeshPdu: ((BluetoothDevice, ByteArray) -> Unit)? = null
    var onGetMtu: (() -> Int)? = null
    
    // Provisioning complete callback
    var onProvisioningComplete: ((Int, String) -> Unit)? = null
    var onProvisioningFailed: ((String) -> Unit)? = null
    
    init {
        meshManagerApi.setMeshManagerCallbacks(this)
        meshManagerApi.setProvisioningStatusCallbacks(this)
        meshManagerApi.setMeshStatusCallbacks(this) // Add this to receive mesh message responses!
        // Don't load network in init - let initializeMeshNetwork handle it
    }
    
    /**
     * Check if mesh network is ready
     */
    fun isNetworkReady(): Boolean = isNetworkReady
    
    /**
     * Wait for network to be ready, then execute callback
     */
    fun onNetworkReady(callback: () -> Unit) {
        if (isNetworkReady) {
            callback()
        } else {
            synchronized(networkReadyCallbacks) {
                networkReadyCallbacks.add(callback)
            }
        }
    }
    
    private fun notifyNetworkReady() {
        isNetworkReady = true
        synchronized(networkReadyCallbacks) {
            networkReadyCallbacks.forEach { it() }
            networkReadyCallbacks.clear()
        }
    }
    
    /**
     * Initialize or load mesh network with hardcoded keys
     */
    fun initializeMeshNetwork() {
        Log.d(TAG, "Initializing mesh network...")
        
        // First try to load existing network
        meshManagerApi.loadMeshNetwork()
    }
    
    private fun createNewMeshNetwork() {
        Log.d(TAG, "Creating new mesh network...")
        
        // Don't call resetMeshNetwork() if network is null - just create new one
        val existingNetwork = meshManagerApi.meshNetwork
        if (existingNetwork != null) {
            Log.d(TAG, "Resetting existing network")
            meshManagerApi.resetMeshNetwork()
        }
        
        // Wait for network to be created via callback
        // The actual network setup will happen in onNetworkLoaded callback
    }
    
    private fun setupNewNetwork(meshNetwork: MeshNetwork) {
        meshNetwork.meshName = "SmartLight Network"
        
        // Setup provisioner
        setupProvisioner(meshNetwork)
        
        // Add network key
        addNetworkKey(meshNetwork)
        
        // Add application key
        addApplicationKey(meshNetwork)
        
        Log.d(TAG, "Mesh network created successfully")
    }
    
    private fun ensureNetworkHasKeys(meshNetwork: MeshNetwork) {
        // Check if network key exists
        if (meshNetwork.netKeys.isEmpty()) {
            addNetworkKey(meshNetwork)
        }
        
        // Check if app key exists
        if (meshNetwork.appKeys.isEmpty()) {
            addApplicationKey(meshNetwork)
        }
        
        Log.d(TAG, "Network has ${meshNetwork.netKeys.size} net keys and ${meshNetwork.appKeys.size} app keys")
    }
    
    private fun setupProvisioner(meshNetwork: MeshNetwork) {
        val provisioner = meshNetwork.selectedProvisioner ?: return
        
        // Set provisioner name
        provisioner.provisionerName = MeshConstants.PROVISIONER_NAME
        
        // Allocate unicast range
        val unicastRange = AllocatedUnicastRange(
            MeshConstants.UNICAST_RANGE_LOW,
            MeshConstants.UNICAST_RANGE_HIGH
        )
        provisioner.allocatedUnicastRanges.clear()
        provisioner.allocatedUnicastRanges.add(unicastRange)
        
        // Allocate group range
        val groupRange = AllocatedGroupRange(
            MeshConstants.GROUP_RANGE_LOW,
            MeshConstants.GROUP_RANGE_HIGH
        )
        provisioner.allocatedGroupRanges.clear()
        provisioner.allocatedGroupRanges.add(groupRange)
        
        // Allocate scene range
        val sceneRange = AllocatedSceneRange(
            MeshConstants.SCENE_RANGE_LOW,
            MeshConstants.SCENE_RANGE_HIGH
        )
        provisioner.allocatedSceneRanges.clear()
        provisioner.allocatedSceneRanges.add(sceneRange)
        
        meshManagerApi.meshNetwork?.updateProvisioner(provisioner)
        
        Log.d(TAG, "Provisioner configured: ${provisioner.provisionerName}")
    }
    
    private fun addNetworkKey(meshNetwork: MeshNetwork) {
        val networkKey = meshNetwork.createNetworkKey()
        
        // Set the hardcoded key bytes
        val keyBytes = MeshConstants.networkKeyBytes
        
        // Update the network key with our hardcoded value
        val updatedKey = NetworkKey(
            MeshConstants.NETWORK_KEY_INDEX,
            keyBytes
        )
        
        meshNetwork.netKeys.clear()
        meshNetwork.netKeys.add(updatedKey)
        
        Log.d(TAG, "Network key added: ${MeshConstants.NETWORK_KEY_HEX}")
    }
    
    private fun addApplicationKey(meshNetwork: MeshNetwork) {
        val netKey = meshNetwork.netKeys.firstOrNull() ?: run {
            Log.e(TAG, "No network key found, cannot add app key")
            return
        }
        
        val appKey = ApplicationKey(
            MeshConstants.APP_KEY_INDEX,
            MeshConstants.appKeyBytes
        )
        appKey.boundNetKeyIndex = netKey.keyIndex
        
        meshNetwork.appKeys.clear()
        meshNetwork.appKeys.add(appKey)
        
        Log.d(TAG, "Application key added: ${MeshConstants.APP_KEY_HEX}")
    }
    
    /**
     * Get next available unicast address for provisioning
     */
    fun getNextUnicastAddress(): Int {
        val meshNetwork = meshManagerApi.meshNetwork ?: return MeshConstants.UNICAST_RANGE_LOW
        
        val provisioner = meshNetwork.selectedProvisioner ?: return MeshConstants.UNICAST_RANGE_LOW
        
        // Find the highest used address
        var highestAddress = MeshConstants.PROVISIONER_ADDRESS
        
        for (node in meshNetwork.nodes) {
            if (node.unicastAddress > highestAddress) {
                highestAddress = node.unicastAddress
            }
        }
        
        return highestAddress + 1
    }
    
    /**
     * Start provisioning process for a discovered device
     */
    fun startProvisioning(device: BluetoothDevice, serviceData: ByteArray?) {
        Log.d(TAG, "======== START PROVISIONING ========")
        Log.d(TAG, "Device: ${device.address}")
        Log.d(TAG, "Service data: ${serviceData?.size ?: 0} bytes")
        serviceData?.let {
            Log.d(TAG, "Service data hex: ${it.joinToString("") { b -> "%02X".format(b) }}")
        }
        
        bluetoothDevice = device
        _provisioningStatus.value = ProvisioningStatus.Connecting
        
        // Parse the service data to get UUID
        var uuid: UUID? = null
        
        if (serviceData != null && serviceData.size >= 16) {
            Log.d(TAG, "Parsing device UUID from service data...")
            uuid = meshManagerApi.getDeviceUuid(serviceData)
            if (uuid != null) {
                Log.d(TAG, "Device UUID parsed from service data: $uuid")
            }
        }
        
        // Fallback: Generate UUID from MAC address if no service data
        // Format: DDDD + MAC (no colons) + padding zeros = 16 bytes
        // Example: CC:BA:97:0D:E2:3A -> DDDDCCBA970DE23A00000000000000000000
        if (uuid == null) {
            Log.w(TAG, "No service data, generating UUID from MAC address...")
            val macHex = device.address.replace(":", "")
            // Create UUID: DDDD + MAC + zeros (same format as Gateway's service data)
            val uuidHex = "DDDD${macHex}0000000000000000"
            uuid = try {
                // Parse as UUID: ddddccba-970d-e23a-0000-000000000000
                UUID.fromString(
                    "${uuidHex.substring(0, 8)}-${uuidHex.substring(8, 12)}-${uuidHex.substring(12, 16)}-${uuidHex.substring(16, 20)}-${uuidHex.substring(20, 32)}"
                )
            } catch (e: Exception) {
                Log.e(TAG, "Failed to generate UUID from MAC: ${e.message}")
                null
            }
            Log.d(TAG, "Generated UUID from MAC: $uuid")
        }
        
        if (uuid != null) {
            identifyNode(device, uuid)
        } else {
            Log.e(TAG, "Cannot determine device UUID")
            _provisioningStatus.value = ProvisioningStatus.Failed("Cannot determine device UUID")
            onProvisioningFailed?.invoke("Cannot determine device UUID")
        }
    }
    
    private fun identifyNode(device: BluetoothDevice, uuid: UUID) {
        Log.d(TAG, "======== IDENTIFY NODE ========")
        Log.d(TAG, "UUID: $uuid")
        _provisioningStatus.value = ProvisioningStatus.Identifying
        
        val meshNetwork = meshManagerApi.meshNetwork ?: run {
            Log.e(TAG, "Mesh network is null!")
            _provisioningStatus.value = ProvisioningStatus.Failed("Mesh network not initialized")
            onProvisioningFailed?.invoke("Mesh network not initialized")
            return
        }
        Log.d(TAG, "Mesh network: ${meshNetwork.meshName}")
        
        // Get provisioning capabilities
        val unprovisionedDevice = UnprovisionedMeshNode(uuid)
        unprovisionedMeshNode = unprovisionedDevice
        
        // Identify - this will request capabilities (sends Provisioning Invite PDU)
        Log.d(TAG, "Calling meshManagerApi.identifyNode($uuid)...")
        Log.d(TAG, "This should trigger sendProvisioningPdu callback")
        meshManagerApi.identifyNode(uuid)
        Log.d(TAG, "identifyNode called - waiting for capabilities response")
    }
    
    /**
     * Continue provisioning after capabilities received
     */
    fun continueProvisioning() {
        val node = unprovisionedMeshNode ?: run {
            _provisioningStatus.value = ProvisioningStatus.Failed("No unprovisioned node")
            onProvisioningFailed?.invoke("No unprovisioned node")
            return
        }
        
        val meshNetwork = meshManagerApi.meshNetwork ?: run {
            _provisioningStatus.value = ProvisioningStatus.Failed("Mesh network not initialized")
            onProvisioningFailed?.invoke("Mesh network not initialized")
            return
        }
        
        _provisioningStatus.value = ProvisioningStatus.Provisioning
        
        // Get next unicast address - MUST call assignUnicastAddress before startProvisioning!
        // The SDK's startProvisioning uses meshNetwork.getUnicastAddress() internally
        val unicastAddress = getNextUnicastAddress()
        Log.d(TAG, "Assigning unicast address: 0x${unicastAddress.toString(16)}")
        
        try {
            // CRITICAL: Set the unicast address on the network BEFORE calling startProvisioning
            // Without this, the SDK uses whatever is already set (often 0x0001)
            meshNetwork.assignUnicastAddress(unicastAddress)
            Log.d(TAG, "Unicast address assigned to network: 0x${unicastAddress.toString(16)}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to assign unicast address: ${e.message}")
            _provisioningStatus.value = ProvisioningStatus.Failed("Failed to assign address: ${e.message}")
            onProvisioningFailed?.invoke("Failed to assign address: ${e.message}")
            return
        }
        
        // Get network key
        val netKey = meshNetwork.netKeys.firstOrNull() ?: run {
            _provisioningStatus.value = ProvisioningStatus.Failed("No network key")
            onProvisioningFailed?.invoke("No network key")
            return
        }
        
        // Start provisioning with No OOB
        Log.d(TAG, "Starting provisioning with unicast address 0x${unicastAddress.toString(16)}")
        meshManagerApi.startProvisioning(node)
    }
    
    /**
     * Handle incoming mesh PDU from BLE
     */
    fun handleNotification(payloadSize: Int, pdu: ByteArray) {
        Log.d(TAG, "======== HANDLE NOTIFICATION ========")
        Log.d(TAG, "Payload size: $payloadSize, PDU size: ${pdu.size} bytes")
        Log.d(TAG, "PDU hex: ${pdu.joinToString("") { b -> "%02X".format(b) }}")
        meshManagerApi.handleNotifications(payloadSize, pdu)
    }
    
    /**
     * Handle write callback from BLE
     */
    fun handleWriteCallback(payloadSize: Int, pdu: ByteArray) {
        meshManagerApi.handleWriteCallbacks(payloadSize, pdu)
    }
    
    /**
     * Send configuration messages to provisioned node
     */
    private fun configureNode(meshNode: ProvisionedMeshNode) {
        Log.d(TAG, "Configuring node: 0x${meshNode.unicastAddress.toString(16)}")
        
        // Get composition data first
        val configCompositionDataGet = ConfigCompositionDataGet()
        meshManagerApi.createMeshPdu(meshNode.unicastAddress, configCompositionDataGet)
    }
    
    /**
     * Add application key to node after getting composition data
     */
    private fun addAppKeyToNode(meshNode: ProvisionedMeshNode) {
        val meshNetwork = meshManagerApi.meshNetwork ?: return
        val netKey = meshNetwork.netKeys.firstOrNull() ?: return
        val appKey = meshNetwork.appKeys.firstOrNull() ?: return
        
        Log.d(TAG, "Adding app key to node: 0x${meshNode.unicastAddress.toString(16)}")
        
        _provisioningStatus.value = ProvisioningStatus.SendingAppKey
        
        val configAppKeyAdd = ConfigAppKeyAdd(netKey, appKey)
        meshManagerApi.createMeshPdu(meshNode.unicastAddress, configAppKeyAdd)
    }
    
    /**
     * Get list of provisioned nodes
     */
    fun getProvisionedNodes(): List<Map<String, Any>> {
        val meshNetwork = meshManagerApi.meshNetwork ?: return emptyList()
        
        return meshNetwork.nodes
            .filter { !it.uuid.equals(meshNetwork.selectedProvisioner?.provisionerUuid) }
            .map { node ->
                mapOf(
                    "uuid" to node.uuid.toString(),
                    "name" to (node.nodeName ?: "Unknown"),
                    "unicastAddress" to node.unicastAddress,
                    "deviceKey" to (node.deviceKey?.let { bytesToHex(it) } ?: "")
                )
            }
    }
    
    /**
     * Clean up resources
     */
    fun cleanup() {
        scope.cancel()
        unprovisionedMeshNode = null
        provisionedMeshNode = null
        bluetoothDevice = null
        _provisioningStatus.value = ProvisioningStatus.Idle
    }
    
    // ==================== MeshManagerCallbacks ====================
    
    override fun onNetworkLoaded(meshNetwork: MeshNetwork?) {
        Log.d(TAG, "onNetworkLoaded: ${meshNetwork?.meshName}")
        if (meshNetwork != null) {
            Log.d(TAG, "Network loaded successfully, ensuring keys...")
            ensureNetworkHasKeys(meshNetwork)
            // Mark network as ready and notify callbacks
            notifyNetworkReady()
        } else {
            Log.d(TAG, "Network is null after load, creating new one...")
            // Network loaded but is null - this shouldn't happen normally
            // but handle it gracefully
        }
    }
    
    override fun onNetworkUpdated(meshNetwork: MeshNetwork?) {
        Log.d(TAG, "onNetworkUpdated: ${meshNetwork?.meshName}")
    }
    
    override fun onNetworkLoadFailed(error: String?) {
        Log.e(TAG, "onNetworkLoadFailed: $error - will create new network")
        // Don't call createNewMeshNetwork here as it may cause issues
        // The network will be created when first provisioning attempt is made
        // For now, just log the error and mark as ready (SDK will create default network)
        scope.launch {
            try {
                // Small delay to let SDK stabilize
                delay(500)
                // Try to create a fresh network
                val currentNetwork = meshManagerApi.meshNetwork
                if (currentNetwork == null) {
                    Log.d(TAG, "Creating fresh mesh network after load failure...")
                    // The SDK should auto-create a default network on first access
                }
                // Mark ready anyway - SDK should handle this
                notifyNetworkReady()
            } catch (e: Exception) {
                Log.e(TAG, "Error handling network load failure: ${e.message}")
                notifyNetworkReady() // Still notify to unblock waiting operations
            }
        }
    }
    
    override fun onNetworkImported(meshNetwork: MeshNetwork?) {
        Log.d(TAG, "onNetworkImported: ${meshNetwork?.meshName}")
    }
    
    override fun onNetworkImportFailed(error: String?) {
        Log.e(TAG, "onNetworkImportFailed: $error")
    }
    
    override fun sendProvisioningPdu(meshNode: UnprovisionedMeshNode?, pdu: ByteArray?) {
        Log.d(TAG, "======== SEND PROVISIONING PDU ========")
        Log.d(TAG, "PDU size: ${pdu?.size ?: 0} bytes")
        pdu?.let { 
            Log.d(TAG, "PDU hex: ${it.joinToString("") { b -> "%02X".format(b) }}")
        }
        
        val device = bluetoothDevice
        if (device == null) {
            Log.e(TAG, "Cannot send PDU - bluetoothDevice is null!")
            return
        }
        
        if (pdu == null || pdu.isEmpty()) {
            Log.e(TAG, "Cannot send PDU - pdu is null or empty!")
            return
        }
        
        Log.d(TAG, "Invoking onSendProvisioningPdu callback...")
        if (onSendProvisioningPdu != null) {
            onSendProvisioningPdu?.invoke(device, pdu)
            Log.d(TAG, "PDU sent to BLE callback")
        } else {
            Log.e(TAG, "onSendProvisioningPdu callback is null!")
        }
    }
    
    override fun onMeshPduCreated(pdu: ByteArray?) {
        Log.d(TAG, "onMeshPduCreated: ${pdu?.size} bytes")
        val device = bluetoothDevice ?: return
        pdu?.let { onSendMeshPdu?.invoke(device, it) }
    }
    
    override fun getMtu(): Int {
        // Returns BLE payload size (MTU - 3) for proper PDU segmentation
        // SDK uses this to segment PDUs with SAR headers at correct boundaries
        val payloadSize = onGetMtu?.invoke() ?: 20
        Log.d(TAG, "getMtu() returning payload size: $payloadSize")
        return payloadSize
    }
    
    // ==================== MeshProvisioningStatusCallbacks ====================
    
    override fun onProvisioningStateChanged(
        meshNode: UnprovisionedMeshNode?,
        state: ProvisioningState.States?,
        data: ByteArray?
    ) {
        Log.d(TAG, "onProvisioningStateChanged: $state")
        
        // Save the node from SDK for use in continueProvisioning
        if (meshNode != null) {
            unprovisionedMeshNode = meshNode
            Log.d(TAG, "Saved UnprovisionedMeshNode from callback")
        }
        
        when (state) {
            ProvisioningState.States.PROVISIONING_INVITE -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Inviting...")
            }
            ProvisioningState.States.PROVISIONING_CAPABILITIES -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Got capabilities")
                // Auto-continue with No OOB
                scope.launch {
                    delay(100)
                    continueProvisioning()
                }
            }
            ProvisioningState.States.PROVISIONING_START -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Starting...")
            }
            ProvisioningState.States.PROVISIONING_PUBLIC_KEY_SENT -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Public key sent")
            }
            ProvisioningState.States.PROVISIONING_PUBLIC_KEY_RECEIVED -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Public key received")
            }
            ProvisioningState.States.PROVISIONING_AUTHENTICATION_STATIC_OOB_WAITING,
            ProvisioningState.States.PROVISIONING_AUTHENTICATION_OUTPUT_OOB_WAITING,
            ProvisioningState.States.PROVISIONING_AUTHENTICATION_INPUT_OOB_WAITING -> {
                // No OOB - should not reach here
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Authenticating...")
            }
            ProvisioningState.States.PROVISIONING_AUTHENTICATION_INPUT_ENTERED -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Auth input entered")
            }
            ProvisioningState.States.PROVISIONING_INPUT_COMPLETE -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Input complete")
            }
            ProvisioningState.States.PROVISIONING_CONFIRMATION_SENT -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Confirmation sent")
            }
            ProvisioningState.States.PROVISIONING_CONFIRMATION_RECEIVED -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Confirmation received")
            }
            ProvisioningState.States.PROVISIONING_RANDOM_SENT -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Random sent")
            }
            ProvisioningState.States.PROVISIONING_RANDOM_RECEIVED -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Random received")
            }
            ProvisioningState.States.PROVISIONING_DATA_SENT -> {
                _provisioningStatus.value = ProvisioningStatus.SendingNetworkKey
            }
            ProvisioningState.States.PROVISIONING_COMPLETE -> {
                _provisioningStatus.value = ProvisioningStatus.ProvisioningProgress("Provisioning complete")
            }
            ProvisioningState.States.PROVISIONING_FAILED -> {
                _provisioningStatus.value = ProvisioningStatus.Failed("Provisioning failed")
                onProvisioningFailed?.invoke("Provisioning failed")
            }
            else -> {
                Log.d(TAG, "Unhandled state: $state")
            }
        }
    }
    
    override fun onProvisioningFailed(
        meshNode: UnprovisionedMeshNode?,
        state: ProvisioningState.States?,
        data: ByteArray?
    ) {
        Log.e(TAG, "onProvisioningFailed: $state")
        _provisioningStatus.value = ProvisioningStatus.Failed("Provisioning failed: $state")
        onProvisioningFailed?.invoke("Provisioning failed: $state")
    }
    
    override fun onProvisioningCompleted(
        meshNode: ProvisionedMeshNode?,
        state: ProvisioningState.States?,
        data: ByteArray?
    ) {
        Log.d(TAG, "onProvisioningCompleted: ${meshNode?.unicastAddress}")
        
        meshNode?.let { node ->
            provisionedMeshNode = node
            // Don't start configuration here - wait for Proxy Service to be ready
            // Configuration will be triggered by startConfigurationAfterProxyReady()
            Log.d(TAG, "Provisioning done, waiting for Proxy Service to be ready...")
        }
    }
    
    /**
     * Start configuration after Proxy Service is ready
     * This should be called after service rediscovery completes (isProvisioning = false)
     */
    fun startConfigurationAfterProxyReady() {
        val node = provisionedMeshNode
        if (node == null) {
            Log.e(TAG, "Cannot start configuration - no provisioned node!")
            return
        }
        
        Log.d(TAG, "======== START CONFIGURATION ========")
        Log.d(TAG, "Configuring node: 0x${node.unicastAddress.toString(16)}")
        configureNode(node)
    }
    
    // ==================== MeshStatusCallbacks ====================
    
    /**
     * Handle mesh messages (for configuration)
     */
    override fun onMeshMessageReceived(src: Int, message: MeshMessage) {
        Log.d(TAG, "onMeshMessageReceived from 0x${src.toString(16)}: ${message.javaClass.simpleName}")
        
        when (message) {
            is ConfigCompositionDataStatus -> {
                Log.d(TAG, "Received composition data")
                // Now add app key
                provisionedMeshNode?.let { addAppKeyToNode(it) }
            }
            is ConfigAppKeyStatus -> {
                Log.d(TAG, "App key status: ${message.statusCode}")
                if (message.isSuccessful) {
                    // Provisioning and configuration complete!
                    provisionedMeshNode?.let { node ->
                        val deviceKey = node.deviceKey?.let { bytesToHex(it) } ?: ""
                        _provisioningStatus.value = ProvisioningStatus.Success(
                            node.unicastAddress,
                            deviceKey
                        )
                        onProvisioningComplete?.invoke(node.unicastAddress, deviceKey)
                    }
                } else {
                    _provisioningStatus.value = ProvisioningStatus.Failed("Failed to add app key: ${message.statusCode}")
                    onProvisioningFailed?.invoke("Failed to add app key")
                }
            }
        }
    }
    
    override fun onTransactionFailed(dst: Int, hasIncompleteTimerExpired: Boolean) {
        Log.e(TAG, "onTransactionFailed: dst=0x${dst.toString(16)}, timeout=$hasIncompleteTimerExpired")
    }
    
    override fun onUnknownPduReceived(src: Int, accessPayload: ByteArray?) {
        Log.w(TAG, "onUnknownPduReceived from 0x${src.toString(16)}")
    }
    
    override fun onBlockAcknowledgementProcessed(dst: Int, message: ControlMessage) {
        Log.d(TAG, "onBlockAcknowledgementProcessed: dst=0x${dst.toString(16)}")
    }
    
    override fun onBlockAcknowledgementReceived(src: Int, message: ControlMessage) {
        Log.d(TAG, "onBlockAcknowledgementReceived from 0x${src.toString(16)}")
    }
    
    override fun onMeshMessageProcessed(dst: Int, message: MeshMessage) {
        Log.d(TAG, "onMeshMessageProcessed: dst=0x${dst.toString(16)}, message=${message.javaClass.simpleName}")
    }
    
    override fun onMessageDecryptionFailed(meshLayer: String?, errorMessage: String?) {
        Log.e(TAG, "onMessageDecryptionFailed: layer=$meshLayer, error=$errorMessage")
    }
    
    // Helper function
    private fun bytesToHex(bytes: ByteArray): String {
        return bytes.joinToString("") { "%02X".format(it) }
    }
}
