extends Node

const NUS_SERVICE := "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
const NUS_TX      := "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
const NUS_RX      := "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

signal device_found(info: Dictionary)
signal line_received(address: String, line: String)  # now includes address
signal raw_received(address: String, data: PackedByteArray)  # now includes address
signal connected(address: String)
signal disconnected(address: String)
signal connected_devices_changed(addresses: Array)

var ble_plugin = null

# address -> rx buffer string
var _rx_buffers: Dictionary = {}

# All devices found during the current scan: address -> info dict
var found_devices: Dictionary = {}

func _ready() -> void:
	if Engine.has_singleton("godotandroidble"):
		BlePermissionsManager.ensure_permissions()
		ble_plugin = Engine.get_singleton("godotandroidble")
		_connect_signals()
		ble_plugin.initPlugin()
	else:
		push_error("BLE plugin not found — make sure the AAR is in android/plugins/ - or ur on pc")

# ---- Public API -------------------------------------------------------------

func start_scan() -> void:
	found_devices.clear()
	if ble_plugin and ble_plugin.isBluetoothReady():
		ble_plugin.scanForAllPeripherals()

func stop_scan() -> void:
	if ble_plugin:
		ble_plugin.stopScanning()

func connect_to(address: String) -> void:
	if ble_plugin:
		ble_plugin.connectToAddress(address)

func disconnect_from(address: String) -> void:
	if ble_plugin and address != "":
		ble_plugin.disconnectPeripheral(address)

func disconnect_all() -> void:
	if ble_plugin:
		ble_plugin.disconnectAll()

func send_bytes(address: String, data: PackedByteArray) -> void:
	if not is_connected_to(address):
		push_warning("BLE: not connected to %s" % address)
		return
	ble_plugin.writeCharacteristic(address, NUS_SERVICE, NUS_RX, data, "WITH_RESPONSE")

func send_line(address: String, text: String) -> void:
	if not text.ends_with("\n"):
		text += "\n"
	send_bytes(address, text.to_utf8_buffer())
	
func send_line_bytes(address: String, text: String) -> void:
	if not text.ends_with("\n"):
		text += "\n"
	send_bytes(address, text.to_utf8_buffer())

func send_string(address: String, text: String) -> void:
	send_bytes(address, text.to_utf8_buffer())

func send_raw(address: String, buffer: PackedByteArray) -> void:
	if buffer.size() > 0 and buffer[buffer.size() - 1] != 10:
		buffer.append(10)
	send_bytes(address, buffer)

func is_connected_to(address: String) -> bool:
	return _rx_buffers.has(address)

func is_any_connected() -> bool:
	return not _rx_buffers.is_empty()

func get_connected_addresses() -> Array:
	return _rx_buffers.keys()

# ---- Internal ---------------------------------------------------------------

func _connect_signals() -> void:
	ble_plugin.bluetooth_device_connected.connect(_on_ble_connected)
	ble_plugin.bluetooth_device_disconnected.connect(_on_ble_disconnected)
	ble_plugin.bluetooth_device_found.connect(_on_ble_device_found)
	ble_plugin.services_discovered.connect(_on_services_discovered)
	ble_plugin.characteristic_received.connect(_on_characteristic_received)
	ble_plugin.connected_devices_changed.connect(_on_connected_devices_changed)
	ble_plugin.permission_required.connect(_on_permission_required)
	ble_plugin.plugin_message.connect(_on_plugin_message)
	ble_plugin.plugin_error.connect(_on_plugin_error)

func _on_ble_device_found(info: Dictionary) -> void:
	found_devices[info["address"]] = info
	device_found.emit(info)

func _on_ble_connected(namee: String, address: String) -> void:
	_rx_buffers[address] = ""
	print("BLE connected: %s (%s)" % [namee, address])
	connected.emit(address)

func _on_ble_disconnected(namee: String, address: String, _status: String) -> void:
	_rx_buffers.erase(address)
	print("BLE disconnected: %s (%s)" % [namee, address])
	disconnected.emit(address)

func _on_connected_devices_changed(addresses: Array) -> void:
	connected_devices_changed.emit(addresses)

func _on_services_discovered(address: String) -> void:
	ble_plugin.startNotify(address, NUS_SERVICE, NUS_TX)

func _on_characteristic_received(address: String, _service_uuid: String,
		char_uuid: String, data: PackedByteArray) -> void:
	print("[BLE RX] char=", char_uuid, " data=", data.get_string_from_utf8())
	if char_uuid.to_upper() != NUS_TX.to_upper():
		return
	raw_received.emit(address, data)
	if not _rx_buffers.has(address):
		_rx_buffers[address] = ""

	var chunk: String = data.get_string_from_utf8()
	var terminated: bool = chunk.ends_with("\n")

	_rx_buffers[address] += chunk

	if terminated:
		# Strip the trailing \n and emit the complete message
		var line: String = _rx_buffers[address].left(_rx_buffers[address].length() - 1)
		_rx_buffers[address] = ""
		if line != "":
			line_received.emit(address, line)

func _on_permission_required(permission: String) -> void:
	push_warning("BLE needs permission: " + permission)
	OS.request_permission(permission)

func _on_plugin_message(msg: String) -> void:
	print("[BLE]: " + msg)

func _on_plugin_error(msg: String) -> void:
	push_error("[BLE]: " + msg)
