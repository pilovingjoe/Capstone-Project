extends ScrollContainer

var is_dragging := false
var velocity := Vector2.ZERO
var _did_drag := false

func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		is_dragging = event.pressed
		if event.pressed:
			velocity = Vector2.ZERO
			_did_drag = false
		else:
			if _did_drag:
				get_viewport().set_input_as_handled()  # consume the lift event

	elif event is InputEventScreenDrag:
		velocity = event.relative
		scroll_vertical -= int(event.relative.y)
		_did_drag = true
		get_viewport().set_input_as_handled()  # consume drag so items don't see it

func _process(delta: float) -> void:
	if not is_dragging and velocity.length() > 1.0:
		scroll_vertical -= int(velocity.y)
		velocity = velocity.lerp(Vector2.ZERO, delta * 8)
