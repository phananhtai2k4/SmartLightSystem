package com.example.smart_light_app.mesh

import no.nordicsemi.android.mesh.utils.MeshParserUtils

/**
 * Mesh network constants - hardcoded keys for the entire app
 */
object MeshConstants {
    // Network Key (16 bytes hex string)
    const val NETWORK_KEY_HEX = "23B15EDAB49C34E95475E3DD8771FB3B"
    
    // Application Key (16 bytes hex string)  
    const val APP_KEY_HEX = "AD8D68C0BADE355F3F2DA405FFA1A63A"
    
    // Key indices
    const val NETWORK_KEY_INDEX = 0
    const val APP_KEY_INDEX = 0
    
    // Provisioner configuration
    const val PROVISIONER_NAME = "SmartLight Provisioner"
    const val PROVISIONER_ADDRESS = 0x0001
    
    // Unicast address range for provisioned nodes
    const val UNICAST_RANGE_LOW = 0x0002
    const val UNICAST_RANGE_HIGH = 0x7FFF
    
    // Group address range
    const val GROUP_RANGE_LOW = 0xC000
    const val GROUP_RANGE_HIGH = 0xFEFF
    
    // Scene address range
    const val SCENE_RANGE_LOW = 0x0001
    const val SCENE_RANGE_HIGH = 0xFFFF
    
    // BLE Scan timeout (milliseconds)
    const val BLE_SCAN_TIMEOUT_MS = 30000L
    
    // Provisioning timeout (milliseconds)
    const val PROVISIONING_TIMEOUT_MS = 60000L
    
    // Gateway device name prefix
    const val GATEWAY_NAME_PREFIX = "GATEWAY_"
    
    // Convert hex string to byte array
    fun hexToBytes(hex: String): ByteArray {
        return MeshParserUtils.toByteArray(hex)
    }
    
    val networkKeyBytes: ByteArray
        get() = hexToBytes(NETWORK_KEY_HEX)
    
    val appKeyBytes: ByteArray
        get() = hexToBytes(APP_KEY_HEX)
}
