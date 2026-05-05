extends Button

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass


func _on_button_down() -> void:
	#$"../../ThisNodeIsJustToHoldAScript".start_scan()
	BLEManager.connect_to("98:A3:16:BF:3B:36")
