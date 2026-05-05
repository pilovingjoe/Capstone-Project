extends CanvasLayer

@export var theme_to_be_modified: Theme

@export var preview: bool:
	set(v):
		_on_resized()

var _resize_timer: SceneTreeTimer = null

func _ready() -> void:
	get_viewport().size_changed.connect(_on_resized)
	_on_resized()

func _on_resized():
	if theme_to_be_modified == null: return
	if _resize_timer != null: return  # already waiting

	_resize_timer = get_tree().create_timer(0.05)
	_resize_timer.timeout.connect(func():
		_resize_timer = null
		_apply_theme()
	)

func _apply_theme():
	if theme_to_be_modified == null: return
	var vw = get_viewport().get_visible_rect().size.x
	var vh = get_viewport().get_visible_rect().size.y
	theme_to_be_modified.set_font_size("font_size", "Label", int(vw * 0.1))
	theme_to_be_modified.set_font_size("font_size", "StatusLabel", int(vw * 0.05))
	theme_to_be_modified.set_default_font_size(int(vw * 0.05))
	theme_to_be_modified.set_font_size("font_size", "TabContainer", int(vw * 0.05))
	theme_to_be_modified.set_constant("icon_max_width", "ItemList", int(vw * 0.1))
	theme_to_be_modified.set_constant("v_separation", "ItemList", int(vw * 0.02))
	theme_to_be_modified.set_constant("h_separation", "HFlowContainer", int(vw * 0.01))
	theme_to_be_modified.set_constant("v_separation", "HFlowContainer", int(vw * 0.01))
	var margin_value = int(vw * 0.05)

	# Set all four margins
	theme_to_be_modified.set_constant("margin_top", "MarginContainer", margin_value)
	theme_to_be_modified.set_constant("margin_bottom", "MarginContainer", margin_value)
	theme_to_be_modified.set_constant("margin_left", "MarginContainer", margin_value)
	theme_to_be_modified.set_constant("margin_right", "MarginContainer", margin_value)
