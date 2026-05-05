class_name BluetoothConnectionGUI
extends MarginContainer

var device_address: String
@onready var rich_text_label:RichTextLabel = $ColorRect/VBoxContainer/RichTextLabel
@onready var msg_line_edit:LineEdit = $ColorRect/VBoxContainer/Msg/LineEdit
@onready var recipient_line_edit:LineEdit = $ColorRect/VBoxContainer/Recipient/LineEdit
func setup(pAddress: String) -> void:
	device_address = pAddress
	$ColorRect/VBoxContainer/AddressLabel.text = pAddress

func append_line(line: String) -> void:
	rich_text_label.append_text(line + " \n")
	# Auto scroll to bottom
	await get_tree().process_frame
	rich_text_label.scroll_to_line(rich_text_label.get_line_count() - 1)

func _on_send_button_pressed() -> void:
	var text: String = msg_line_edit.text
	var recipient_text: String = recipient_line_edit.text
	
	if text == "": return
		
	if recipient_text == "" or recipient_text == "0":
		# Regular text broadcast
		BLEManager.send_line(device_address, "A" + text)
	else:
		# Raw packet for Unicast
		var packet = PackedByteArray()
		packet.append(84) # 'T'
		
		var addr_int = recipient_text.to_int()
		packet.append((addr_int >> 8) & 0xFF) # High Byte
		packet.append(addr_int & 0xFF)        # Low Byte
		
		packet.append_array(text.to_utf8_buffer())
		
		# Use the RAW sender to avoid UTF-8 mangling of the address bytes
		BLEManager.send_raw(device_address, packet)
	
	#msg_line_edit.text = ""

func _on_info_button_pressed() -> void:
	var text: String = msg_line_edit.text
	if text == "": return
	BLEManager.send_line(device_address, "R" + text)
	
func _on_clear_button_pressed() -> void:
	rich_text_label.clear()
	

func _on_disconnect_button_pressed() -> void:
	BLEManager.disconnect_from(device_address)
