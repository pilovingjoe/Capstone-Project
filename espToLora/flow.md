### Flashing the thing
1. Open the ino file in arduino ide
2. flash the esp c6

### How to connect to the device from a phone app
From your phone app, connect to the esp with ble. I used the "blessed" kotlin libarary. but u can use whatever ble lib
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
APP -> ESP characteristic = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" 
ESP -> APP characteristic = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" 

THE ESP CAN ONLY SEND 20 bytes at a time, so keep each message from the esp in a buffer until you get a message that ENDS with '\n' the char. The message may contain \n but until what you read ENDS with \n it aint the full message yet.

You can just send as much as you want to the ESP tho i think
The first byte of your message should be "A" for broadcast, "T" for a specific recipient, and "R" to request data from the device like network ID
"T(two bytes, the binary of hte recievers u16 address on the mesh.)message"
"Amessage" 
"Rnet" (the device will return its ID

Dont spam the esp cuz it will queue all of your messages and will take a long time to send it all lora is slow. 

