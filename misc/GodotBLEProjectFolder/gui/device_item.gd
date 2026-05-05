extends VBoxContainer

signal connect_requested(address: String)

var info: Dictionary

func _ready() -> void:
	$ExpandedPanel.visible = false
	BLEManager.connected.connect(_on_ble_connected)
	BLEManager.disconnected.connect(_on_ble_disconnected)

func setup(device_info: Dictionary) -> void:
	info = device_info
	var display_name = info["name"]
	if display_name == "" or display_name == null:
		display_name = info["address"]
	$HBoxContainer/Label.text = "%s  %d dBm" % [display_name, info["rssi"]]
	_update_info_panel()
	
func _on_touch(event: InputEvent) -> void:
	if event is InputEventScreenTouch and not event.pressed:
		$ExpandedPanel.visible = !$ExpandedPanel.visible
		if $ExpandedPanel.visible:
			_update_info_panel()

func _on_connect_button_pressed() -> void:
	connect_requested.emit(info["address"])

func _on_ble_connected(address: String) -> void:
	if address == info["address"]:
		_update_info_panel()

func _on_ble_disconnected(address: String) -> void:
	if address == info["address"]:
		_update_info_panel()

func _update_info_panel() -> void:
	var rt: RichTextLabel = $ExpandedPanel/RichTextLabel
	var is_connected := BLEManager.is_connected_to(info["address"])

	var text := ""

	# Connection status
	if is_connected:
		text += "[color=green]● Connected[/color]\n\n"
	else:
		text += "[color=red]● Not Connected[/color]\n\n"

	# All fields from the scan info dict
	text += "[b]Address:[/b] %s\n" % info.get("address", "?")
	text += "[b]RSSI:[/b] %d dBm\n" % info.get("rssi", 0)
	text += "[b]Bond State:[/b] %s\n" % info.get("bond_state", "?")
	text += "[b]Type:[/b] %s\n" % info.get("type", "?")
	text += "[b]Connectable:[/b] %s\n" % str(info.get("connectable", "?"))
	text += "[b]TX Power:[/b] %s\n" % str(info.get("tx_power", "?"))
	text += "[b]MTU:[/b] %s\n" % str(info.get("mtu", "?"))

	var service_uuids = info.get("service_uuids", [])
	if service_uuids.size() > 0:
		text += "\n[b]Advertised Services:[/b]\n"
		for uuid in service_uuids:
			text += "  %s\n" % uuid

	var manufacturer_id = info.get("manufacturer_id", -1)
	if manufacturer_id != -1:
		text += "\n[b]Manufacturer ID:[/b] %d\n" % manufacturer_id

	rt.bbcode_enabled = true
	rt.text = text
