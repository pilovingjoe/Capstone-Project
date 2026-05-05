extends VBoxContainer


const DeviceItem = preload("res://gui/device_item.tscn")

func _ready() -> void:
	if OS.get_name() == "Linux":
		_fill_fake_data()
		return
	BLEManager.device_found.connect(_on_device_found)
	#theBluetoothDoer.connected.connect(_on_connected)
	BLEManager.line_received.connect(_on_line)
	BLEManager.start_scan()
	var filter_nameless: CheckBox = $"../../buttons/FilterNameless"
	filter_nameless.toggled.connect(_on_filter_nameless_toggled)
	
func _fill_fake_data() -> void:
	for i in range(100):
		var info := {
			"name": "LoRaMesh Node %d" % i,
			"address": "AA:BB:CC:DD:%02X:%02X" % [i / 256, i % 256],
			"rssi": randi_range(-100, -40)
		}
		_on_device_found(info)

func _on_device_found(info: Dictionary) -> void:
	# Check if already exists and update
	for child in get_children():
		if child.info["address"] == info["address"]:
			child.setup(info)
			return
	
	var item = DeviceItem.instantiate()
	add_child(item)
	item.setup(info)
	item.connect_requested.connect(_on_connect_requested)
	_apply_filter(item)
func _apply_filter(item: Node) -> void:
	var filter_nameless: CheckBox = $"../../buttons/FilterNameless"
	if filter_nameless.button_pressed and (item.info["name"] == "" or item.info["name"] == null):
		item.visible = false
	else:
		item.visible = true

func _on_filter_nameless_toggled(_pressed: bool) -> void:
	for child in get_children():
		_apply_filter(child)
		
func _on_connect_requested(address: String) -> void:
	BLEManager.stop_scan()
	BLEManager.connect_to(address)

	
#func _on_connected(_address: String) -> void:
	##pass
	#theBluetoothDoer.send_line("hello")

func _on_line(address, line) -> void:
	print("Got: ", line)
