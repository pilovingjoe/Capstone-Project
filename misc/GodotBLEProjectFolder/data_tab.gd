extends VBoxContainer

const ConnectionPanel = preload("res://mainScreen/connection_chat_gui.tscn")

# address -> BluetoothConnectionGUI node
var _panels: Dictionary = {}

func _ready() -> void:
	BLEManager.connected.connect(_on_connected)
	BLEManager.disconnected.connect(_on_disconnected)
	BLEManager.line_received.connect(_on_line_received)

	# In case already connected when this tab opens
	for address in BLEManager.get_connected_addresses():
		_add_panel(address)

func _on_connected(address: String) -> void:
	_add_panel(address)

func _on_disconnected(address: String) -> void:
	if _panels.has(address):
		_panels[address].queue_free()
		_panels.erase(address)

func _on_line_received(address: String, line: String) -> void:
	if _panels.has(address):
		_panels[address].append_line(line)

func _add_panel(address: String) -> void:
	if _panels.has(address):
		return
	var panel: BluetoothConnectionGUI = ConnectionPanel.instantiate()
	add_child(panel)
	panel.setup(address)
	_panels[address] = panel
