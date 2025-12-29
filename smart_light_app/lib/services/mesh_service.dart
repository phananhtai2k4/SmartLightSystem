import 'dart:async';
import 'package:flutter/services.dart';

/// Represents an unprovisioned BLE mesh device
class UnprovisionedMeshDevice {
  final String address;
  final String? name;
  final int rssi;
  final String? macSuffix;
  final String? uuid;
  final String? serviceData; // Hex string of service data for provisioning

  UnprovisionedMeshDevice({
    required this.address,
    this.name,
    required this.rssi,
    this.macSuffix,
    this.uuid,
    this.serviceData,
  });

  factory UnprovisionedMeshDevice.fromMap(Map<dynamic, dynamic> map) {
    return UnprovisionedMeshDevice(
      address: map['address'] as String? ?? '',
      name: map['name'] as String?,
      rssi: map['rssi'] as int? ?? -100,
      macSuffix: map['macSuffix'] as String?,
      uuid: map['uuid'] as String?,
      serviceData: map['serviceData'] as String?,
    );
  }

  /// Check if this device is a Gateway (name starts with GATEWAY_)
  bool get isGateway => name?.startsWith('GATEWAY_') ?? false;

  @override
  String toString() =>
      'UnprovisionedMeshDevice(address: $address, name: $name, rssi: $rssi, serviceData: ${serviceData?.length ?? 0} bytes)';
}

/// Represents a provisioned mesh node
class ProvisionedMeshNode {
  final String uuid;
  final String name;
  final int unicastAddress;
  final String deviceKey;

  ProvisionedMeshNode({
    required this.uuid,
    required this.name,
    required this.unicastAddress,
    required this.deviceKey,
  });

  factory ProvisionedMeshNode.fromMap(Map<dynamic, dynamic> map) {
    return ProvisionedMeshNode(
      uuid: map['uuid'] as String? ?? '',
      name: map['name'] as String? ?? 'Unknown',
      unicastAddress: map['unicastAddress'] as int? ?? 0,
      deviceKey: map['deviceKey'] as String? ?? '',
    );
  }
}

/// Provisioning status enum
enum ProvisioningStatus {
  idle,
  connecting,
  connected,
  serviceDiscovered,
  provisioning,
  sendingNetworkKey,
  sendingAppKey,
  success,
  failed,
  disconnected,
}

/// Provisioning event data
class ProvisioningEvent {
  final ProvisioningStatus status;
  final String? error;
  final int? unicastAddress;
  final String? deviceKey;

  ProvisioningEvent({
    required this.status,
    this.error,
    this.unicastAddress,
    this.deviceKey,
  });

  factory ProvisioningEvent.fromMap(Map<dynamic, dynamic> map) {
    final statusStr = map['status'] as String? ?? 'idle';
    ProvisioningStatus status;

    switch (statusStr) {
      case 'connecting':
        status = ProvisioningStatus.connecting;
        break;
      case 'connected':
        status = ProvisioningStatus.connected;
        break;
      case 'serviceDiscovered':
        status = ProvisioningStatus.serviceDiscovered;
        break;
      case 'provisioning':
        status = ProvisioningStatus.provisioning;
        break;
      case 'sendingNetworkKey':
        status = ProvisioningStatus.sendingNetworkKey;
        break;
      case 'sendingAppKey':
        status = ProvisioningStatus.sendingAppKey;
        break;
      case 'success':
        status = ProvisioningStatus.success;
        break;
      case 'failed':
        status = ProvisioningStatus.failed;
        break;
      case 'disconnected':
        status = ProvisioningStatus.disconnected;
        break;
      default:
        status = ProvisioningStatus.idle;
    }

    return ProvisioningEvent(
      status: status,
      error: map['error'] as String?,
      unicastAddress: map['unicastAddress'] as int?,
      deviceKey: map['deviceKey'] as String?,
    );
  }
}

/// Scan event data
class ScanEvent {
  final String type;
  final UnprovisionedMeshDevice? device;
  final int? errorCode;

  ScanEvent({required this.type, this.device, this.errorCode});

  factory ScanEvent.fromMap(Map<dynamic, dynamic> map) {
    UnprovisionedMeshDevice? device;
    if (map['device'] != null) {
      device = UnprovisionedMeshDevice.fromMap(
        map['device'] as Map<dynamic, dynamic>,
      );
    }

    return ScanEvent(
      type: map['type'] as String? ?? 'unknown',
      device: device,
      errorCode: map['errorCode'] as int?,
    );
  }
}

/// MeshService - Flutter service for BLE Mesh operations
/// Communicates with native Android via Platform Channels
class MeshService {
  static const MethodChannel _methodChannel = MethodChannel('smart_light/mesh');
  static const EventChannel _scanEventChannel = EventChannel(
    'smart_light/mesh/scan',
  );
  static const EventChannel _provisionEventChannel = EventChannel(
    'smart_light/mesh/provision',
  );

  // Singleton instance
  static final MeshService _instance = MeshService._internal();
  factory MeshService() => _instance;
  MeshService._internal();

  // Stream controllers
  final _scanController = StreamController<ScanEvent>.broadcast();
  final _provisionController = StreamController<ProvisioningEvent>.broadcast();

  // Discovered devices
  final List<UnprovisionedMeshDevice> _discoveredDevices = [];

  // Stream subscriptions
  StreamSubscription? _scanSubscription;
  StreamSubscription? _provisionSubscription;

  /// Initialize the service and start listening to events
  void initialize() {
    _scanSubscription?.cancel();
    _provisionSubscription?.cancel();

    _scanSubscription = _scanEventChannel.receiveBroadcastStream().listen(
      (event) {
        if (event is Map) {
          final scanEvent = ScanEvent.fromMap(event);

          // Add device to list if found
          if (scanEvent.type == 'deviceFound' && scanEvent.device != null) {
            final device = scanEvent.device!;
            // Avoid duplicates
            if (!_discoveredDevices.any((d) => d.address == device.address)) {
              _discoveredDevices.add(device);
            }
          }

          _scanController.add(scanEvent);
        }
      },
      onError: (error) {
        print('MeshService scan error: $error');
      },
    );

    _provisionSubscription = _provisionEventChannel
        .receiveBroadcastStream()
        .listen(
          (event) {
            if (event is Map) {
              final provisionEvent = ProvisioningEvent.fromMap(event);
              _provisionController.add(provisionEvent);
            }
          },
          onError: (error) {
            print('MeshService provision error: $error');
          },
        );
  }

  /// Dispose resources
  void dispose() {
    _scanSubscription?.cancel();
    _provisionSubscription?.cancel();
    _scanController.close();
    _provisionController.close();
  }

  /// Stream of scan events
  Stream<ScanEvent> get scanEvents => _scanController.stream;

  /// Stream of provisioning events
  Stream<ProvisioningEvent> get provisioningEvents =>
      _provisionController.stream;

  /// Get list of discovered devices
  List<UnprovisionedMeshDevice> get discoveredDevices =>
      List.unmodifiable(_discoveredDevices);

  /// Request Bluetooth permissions
  Future<bool> requestPermissions() async {
    try {
      final result = await _methodChannel.invokeMethod<bool>(
        'requestPermissions',
      );
      return result ?? false;
    } catch (e) {
      print('MeshService requestPermissions error: $e');
      return false;
    }
  }

  /// Check if permissions are granted
  Future<bool> hasPermissions() async {
    try {
      final result = await _methodChannel.invokeMethod<bool>('hasPermissions');
      return result ?? false;
    } catch (e) {
      print('MeshService hasPermissions error: $e');
      return false;
    }
  }

  /// Check if Bluetooth is enabled
  Future<bool> isBluetoothEnabled() async {
    try {
      final result = await _methodChannel.invokeMethod<bool>(
        'isBluetoothEnabled',
      );
      return result ?? false;
    } catch (e) {
      print('MeshService isBluetoothEnabled error: $e');
      return false;
    }
  }

  /// Start scanning for unprovisioned devices
  Future<bool> startScan() async {
    try {
      _discoveredDevices.clear();
      final result = await _methodChannel.invokeMethod<bool>('startScan');
      return result ?? false;
    } catch (e) {
      print('MeshService startScan error: $e');
      return false;
    }
  }

  /// Stop scanning
  Future<bool> stopScan() async {
    try {
      final result = await _methodChannel.invokeMethod<bool>('stopScan');
      return result ?? false;
    } catch (e) {
      print('MeshService stopScan error: $e');
      return false;
    }
  }

  /// Get list of discovered devices from native
  Future<List<UnprovisionedMeshDevice>> getDiscoveredDevices() async {
    try {
      final result = await _methodChannel.invokeMethod<List<dynamic>>(
        'getDiscoveredDevices',
      );
      if (result != null) {
        return result
            .map(
              (e) =>
                  UnprovisionedMeshDevice.fromMap(e as Map<dynamic, dynamic>),
            )
            .toList();
      }
      return [];
    } catch (e) {
      print('MeshService getDiscoveredDevices error: $e');
      return [];
    }
  }

  /// Start provisioning a device
  /// [address] - BLE MAC address
  /// [name] - Device name (optional)
  /// [serviceData] - Service data hex string from scan (required for mesh provisioning)
  Future<bool> provisionDevice(
    String address, {
    String? name,
    String? serviceData,
  }) async {
    try {
      print(
        'MeshService: provisionDevice address=$address, name=$name, serviceData=${serviceData?.length ?? 0} chars',
      );
      final result = await _methodChannel.invokeMethod<bool>(
        'provisionDevice',
        {'address': address, 'name': name, 'serviceData': serviceData},
      );
      return result ?? false;
    } catch (e) {
      print('MeshService provisionDevice error: $e');
      return false;
    }
  }

  /// Cancel ongoing provisioning
  Future<bool> cancelProvisioning() async {
    try {
      final result = await _methodChannel.invokeMethod<bool>(
        'cancelProvisioning',
      );
      return result ?? false;
    } catch (e) {
      print('MeshService cancelProvisioning error: $e');
      return false;
    }
  }

  /// Get list of provisioned nodes
  Future<List<ProvisionedMeshNode>> getProvisionedNodes() async {
    try {
      final result = await _methodChannel.invokeMethod<List<dynamic>>(
        'getProvisionedNodes',
      );
      if (result != null) {
        return result
            .map((e) => ProvisionedMeshNode.fromMap(e as Map<dynamic, dynamic>))
            .toList();
      }
      return [];
    } catch (e) {
      print('MeshService getProvisionedNodes error: $e');
      return [];
    }
  }

  /// Get next available unicast address
  Future<int> getNextUnicastAddress() async {
    try {
      final result = await _methodChannel.invokeMethod<int>(
        'getNextUnicastAddress',
      );
      return result ?? 0x0002;
    } catch (e) {
      print('MeshService getNextUnicastAddress error: $e');
      return 0x0002;
    }
  }

  /// Scan for a specific Gateway by MAC suffix
  /// [macSuffix] - Last 4 characters of Gateway MAC (from GATEWAY_XXXX name)
  /// [timeoutSeconds] - Scan timeout in seconds (default 30)
  /// Returns the found device or null if not found
  Future<UnprovisionedMeshDevice?> scanForGateway(
    String macSuffix, {
    int timeoutSeconds = 30,
  }) async {
    print('MeshService: Scanning for Gateway with MAC suffix: $macSuffix');

    // Clear previous devices
    _discoveredDevices.clear();

    // Start scanning
    final started = await startScan();
    if (!started) {
      print('MeshService: Failed to start scan');
      return null;
    }

    final targetSuffix = macSuffix.toUpperCase();
    final completer = Completer<UnprovisionedMeshDevice?>();
    Timer? timeoutTimer;
    StreamSubscription? subscription;

    // Listen for scan events
    subscription = scanEvents.listen((event) {
      if (event.type == 'deviceFound' && event.device != null) {
        final device = event.device!;
        print('MeshService: Found device: ${device.name} (${device.address})');

        // Check if this is our target Gateway
        if (device.isGateway &&
            device.macSuffix?.toUpperCase() == targetSuffix) {
          print('MeshService: Found target Gateway: ${device.name}');
          timeoutTimer?.cancel();
          subscription?.cancel();
          stopScan();
          if (!completer.isCompleted) {
            completer.complete(device);
          }
        }
      } else if (event.type == 'scanFailed') {
        print('MeshService: Scan failed with error: ${event.errorCode}');
        timeoutTimer?.cancel();
        subscription?.cancel();
        if (!completer.isCompleted) {
          completer.complete(null);
        }
      }
    });

    // Set timeout
    timeoutTimer = Timer(Duration(seconds: timeoutSeconds), () {
      print('MeshService: Scan timeout after $timeoutSeconds seconds');
      subscription?.cancel();
      stopScan();
      if (!completer.isCompleted) {
        completer.complete(null);
      }
    });

    return completer.future;
  }

  /// Provision a Gateway device and wait for completion
  /// Returns ProvisioningEvent with success/failure status
  Future<ProvisioningEvent> provisionGatewayAndWait(
    UnprovisionedMeshDevice device, {
    int timeoutSeconds = 60,
  }) async {
    print(
      'MeshService: Starting provisioning for ${device.name} (${device.address})',
    );

    // Note: If no serviceData, native side will generate UUID from MAC address
    // This allows provisioning devices that are already in provisioning state
    // or devices that don't advertise service data properly
    if (device.serviceData == null || device.serviceData!.isEmpty) {
      print(
        'MeshService: WARNING - Device has no serviceData, will generate UUID from MAC address',
      );
    }

    final completer = Completer<ProvisioningEvent>();
    Timer? timeoutTimer;
    StreamSubscription? subscription;

    // Listen for provisioning events
    subscription = provisioningEvents.listen((event) {
      print('MeshService: Provisioning event: ${event.status}');

      if (event.status == ProvisioningStatus.success) {
        print(
          'MeshService: Provisioning successful! Address: ${event.unicastAddress}',
        );
        timeoutTimer?.cancel();
        subscription?.cancel();
        if (!completer.isCompleted) {
          completer.complete(event);
        }
      } else if (event.status == ProvisioningStatus.failed) {
        print('MeshService: Provisioning failed: ${event.error}');
        timeoutTimer?.cancel();
        subscription?.cancel();
        if (!completer.isCompleted) {
          completer.complete(event);
        }
      } else if (event.status == ProvisioningStatus.disconnected) {
        print('MeshService: Disconnected during provisioning');
        timeoutTimer?.cancel();
        subscription?.cancel();
        if (!completer.isCompleted) {
          completer.complete(
            ProvisioningEvent(
              status: ProvisioningStatus.failed,
              error: 'Disconnected during provisioning',
            ),
          );
        }
      }
    });

    // Start provisioning - pass serviceData which contains device UUID
    print(
      'MeshService: Starting provision for ${device.name} with serviceData=${device.serviceData?.length ?? 0} chars',
    );
    final started = await provisionDevice(
      device.address,
      name: device.name,
      serviceData: device.serviceData,
    );
    if (!started) {
      subscription.cancel();
      return ProvisioningEvent(
        status: ProvisioningStatus.failed,
        error: 'Failed to start provisioning',
      );
    }

    // Set timeout
    timeoutTimer = Timer(Duration(seconds: timeoutSeconds), () {
      print('MeshService: Provisioning timeout after $timeoutSeconds seconds');
      subscription?.cancel();
      cancelProvisioning();
      if (!completer.isCompleted) {
        completer.complete(
          ProvisioningEvent(
            status: ProvisioningStatus.failed,
            error: 'Provisioning timeout',
          ),
        );
      }
    });

    return completer.future;
  }
}
