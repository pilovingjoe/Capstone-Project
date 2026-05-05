extends Button

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.
#98:A3:16:BF:3B:36

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass


func _on_button_down() -> void:
	BLEManager.found_devices.clear()
	if BLEManager.ble_plugin and BLEManager.ble_plugin.isBluetoothReady():
		BLEManager.ble_plugin.scanForAddresses(["98:A3:16:BF:3B:36"])
