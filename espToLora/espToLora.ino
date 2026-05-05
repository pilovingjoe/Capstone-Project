
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h> //for the rgb led
#include <queue>
#include <SPIMemory.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// the uart lora module uses "AT" commands. ex) Serial2.write("AT+SEND=...")
// UART_LoRaAT implements an api for interacting with the module.
// There are some AT commands that are not implemented yet cuz i didnt need them..
#include "UART_LoRaAT.h"

#include "RYLR998Link.h"

#include "Mesh.h"

BLEServer *bleServer = NULL;
BLECharacteristic *bleTxCharacteristic;
bool deviceConnected = false;
bool wasDeviceConnected = false; // represents if a device connected in the previous "loop" call

SPIFlash flash;
int address = 0;

std::queue<String> bleTXQueue; // We can only send like 20 bytes per 20 ms, as per ble standard.
SemaphoreHandle_t queueMutex;  // ble callbacks run on a different thread. phone ble message -> esp, queued in mutex. esp -> lora mesh
unsigned long lastBleSendTime = 0;
const int BLE_SEND_INTERVAL = 20; // ms between chunks

struct BLECommand
{
    char type;       // 'A'=broadcast, 'T'=targeted send, 'R'=request info
    uint16_t target; // the recipient of the message on the mesh network
    String message;
};

UART_LoRaAT lora_uart;
#define NODE_ADDRESS (uint16_t)(((__TIME__[0] - '0') * 100000u + (__TIME__[1] - '0') * 10000u + \
                                 (__TIME__[3] - '0') * 1000u + (__TIME__[4] - '0') * 100u +     \
                                 (__TIME__[6] - '0') * 10u + (__TIME__[7] - '0')) %             \
                                    65535 +                                                     \
                                1)
uint16_t nodeAddr = NODE_ADDRESS; // just give a random address at compile/flash time
RYLR998Link rylr_link(lora_uart, nodeAddr); // Address 1
Mesh lora_mesh(rylr_link);
std::queue<BLECommand> bleRXQueue; // the ble messages from the phone, are queued here by the ble callbacks, to be read in loop.

#define LED_PIN 8
Adafruit_NeoPixel rgbLed(1, LED_PIN, NEO_GRB + NEO_KHZ800);
// The soft pulse
uint32_t LEDAnimBreathe(int phase)
{
    int brightness = (5 / 2.0) * (1.0 + sin(radians(phase) - PI / 2));
    return rgbLed.Color(0, brightness, brightness); // Cyan
}

// A "Police Flash"
uint32_t LEDAnimEmergency(int phase)
{
    // Sharp on/off every 180 degrees
    if (phase % 180 < 90)
        return rgbLed.Color(10, 0, 0); // Red
    return rgbLed.Color(0, 0, 0);      // Off
}

// A "Heartbeat" for LoRa activity
uint32_t LEDAnimHeartbeat(int phase)
{
    if (phase < 30 || (phase > 60 && phase < 90))
        return rgbLed.Color(0, 10, 0);
    return rgbLed.Color(0, 0, 0);
}

unsigned long lastLedUpdate = 0;
int ledColorPhase = 0;                            // 0-360
typedef uint32_t (*LEDAnimation)(int phase);      // A custom type, a function pointer, the led will use as its animation.
LEDAnimation currentAnimation = LEDAnimHeartbeat; // Start with breathing. Just set this animation to a function, and the led will use it

#define RXD2 11 // connect this pin to the loras tx
#define TXD2 10
#define RST 7 // connec this pin to loras rst

// Every BLE "server" comes with a table of data,
// entries in this table are called "characteristics", identified by uuids (any uuid we want)
// The table is stored on the "server" of the connection. (this microcontroller)
// Devices can send read, write (reliable), and push (unreliable), requests, for "characteristics"
// and by default BLE will use the table for these requests.
// We can however, override the default BLE behaivour and make callbacks that run on these requests instead.
// The point of the table, is that its doesnt require the cpu, and is way faster.
// https://www.uuidgenerator.net/
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // some uuids are standard, these ones are standard for a
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // fake "serial" over bluetooth. (will work with bluetooth serial app)

class MyBLEServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *bleServer)
    {
        deviceConnected = true;
        Serial.println("Device connected");
        currentAnimation = LEDAnimBreathe;
    };

    void onDisconnect(BLEServer *bleServer)
    {
        deviceConnected = false;
        Serial.println("Device disconnected");
        currentAnimation = LEDAnimEmergency;
    }
};

// MESSAGES ARE SPLIT UP INTO 20 byte CHUNKS, \n at the very end means "end of message", if msg ends with \n already, another \n is added.
void bleSendString(String msg)
{
    if (!deviceConnected || bleTxCharacteristic == NULL)
        return;

    int pos = 0;
    int len = msg.length();
    String carry = ""; // any \n bumped from the end of the previous chunk

    do
    {
        bool isFinal = (pos + 19 >= len);
        int chunkSize = isFinal ? (len - pos) : 19;
        String chunk = carry + msg.substring(pos, pos + chunkSize);
        carry = "";

        if (isFinal)
        {
            chunk += "\n"; // sole end-of-message delimiter
        }
        else
        {
            // If chunk happens to end with \n, it would be misread as end-of-message.
            // Bump it to the front of the next chunk instead.
            if (chunk.endsWith("\n"))
            {
                carry = "\n";
                chunk = chunk.substring(0, chunk.length() - 1);
            }
        }

        bleTXQueue.push(chunk);
        pos += chunkSize;
    } while (pos < len);

    if (len == 0)
    {
        bleTXQueue.push("\n");
    }
}

bool parseBLECommand(const String &raw, BLECommand &out)
{
    if (raw.length() == 0)
        return false;

    Serial.print("[PARSE] len=");
    Serial.print(raw.length());
    Serial.print(" bytes: ");
    for (int i = 0; i < raw.length(); i++)
    {
        Serial.printf("%02X ", (uint8_t)raw.charAt(i));
    }
    Serial.println();

    out.type = raw.charAt(0);

    if (out.type == 'A')
    {
        out.target = 0;
        out.message = raw.substring(1);
        out.message.trim();
        return out.message.length() > 0;
    }

    if (out.type == 'T')
    {
        if (raw.length() < 3)
            return false;
        out.target = (uint16_t)(uint8_t)raw.charAt(1) << 8 | (uint8_t)raw.charAt(2);
        out.message = raw.substring(3);
        out.message.trim();
        return out.message.length() > 0;
    }

    if (out.type == 'R')
    {
        // Optional sub-command string after the 'R', e.g. "RALL", "RNET", "RMSG", "RSTATS"
        // If omitted or unrecognised, defaults to "ALL"
        out.target = 0;
        out.message = raw.substring(1);
        out.message.trim();
        out.message.toUpperCase();
        if (out.message.length() == 0)
            out.message = "ALL";
        return true;
    }

    return false; // unknown command
}

// Build and send a node info report over BLE.
// sub is one of: ALL, NET, MSG, STATS
void handleInfoRequest(const String &sub)
{
    Serial.printf("[INFO] request sub=%s\n", sub.c_str());
 
    if (sub == "NET" || sub == "ALL")
    {
        String s = "";
        s += "ADDR: " + String(nodeAddr) + "\n";
        s += "UPTIME: " + String(millis() / 1000) + "s\n";
        s += "ROUTES: " + String(lora_mesh.getRouteCount()) + "\n";
        bleSendString(s);
    }
 
    if (sub == "MSG" || sub == "ALL")
    {
        String s = "";
        const char *lastMsg = lora_mesh.getLastMessage();
        uint16_t lastSnd = lora_mesh.getLastSender();
        if (lastSnd == 0 && (lastMsg == nullptr || lastMsg[0] == '\0'))
        {
            s += "no msg yet\n";
        }
        else
        {
            s += "FROM: " + String(lastSnd) + "\n";
            s += "DATA: " + String(lastMsg) + "\n";
        }
        bleSendString(s);
    }
 
    if (sub == "STATS" || sub == "ALL")
    {
        String s = "";
        s += "TX_MSGS: " + String(lora_uart.getTxMessageCount()) + "\n";
        s += "RX_MSGS: " + String(lora_uart.getRxMessageCount()) + "\n";
        s += "RX_ERRS: " + String(lora_uart.getRxErrorCount()) + "\n";
        s += "OVERFLOWS: " + String(lora_uart.getOverflowCount()) + "\n";
        bleSendString(s);
    }
 
    if (sub != "NET" && sub != "MSG" && sub != "STATS" && sub != "ALL")
    {
        bleSendString("ERR: unknown sub-cmd '" + sub + "'. Use: ALL,NET,MSG, or STATS\n");
    }
}


void handleCommand(const String &input)
{
    BLECommand cmd;
    if (!parseBLECommand(input, cmd))
    {
        Serial.printf("[CMD] bad command, first byte=0x%02X\n", (uint8_t)input.charAt(0));
        return;
    }

    if (xSemaphoreTake(queueMutex, portMAX_DELAY))
    {
        bleRXQueue.push(cmd); // push the struct
        xSemaphoreGive(queueMutex);
    }
}

class MyBLECharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        String rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0)
        {
            Serial.println("*********");
            Serial.print("Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
            {
                Serial.print(rxValue[i]);
            }

            handleCommand(rxValue);

            Serial.println();
            Serial.println("*********");
        }
    }
};

bool parseSerialCommand(const String &raw, BLECommand &out)
{
    if (raw.length() == 0)
        return false;

    out.type = raw.charAt(0);

    if (out.type == 'A')
    {
        out.message = raw.substring(1);
        out.message.trim();
        out.target = 0;
        return out.message.length() > 0;
    }

    if (out.type == 'T')
    {
        // Expect "T<addr> <message>", e.g. "T4129 hello"
        int spaceIdx = raw.indexOf(' ');
        if (spaceIdx < 2)
            return false;
        out.target = (uint16_t)raw.substring(1, spaceIdx).toInt();
        out.message = raw.substring(spaceIdx + 1);
        out.message.trim();
        return out.target > 0 && out.message.length() > 0;
    }

    if (out.type == 'R')
    {
        // e.g. "RALL", "RNET", "RMSG", "RSTATS" or just "R"
        out.target = 0;
        out.message = raw.substring(1);
        out.message.trim();
        out.message.toUpperCase();
        if (out.message.length() == 0)
            out.message = "ALL";
        return true;
    }

    return false;
}

void setup()
{

    Serial.begin(115200); // USB debug
    Serial1.begin(115200, SERIAL_8N1, RXD2, TXD2);
    lora_uart.setSerial(&Serial1);

    flash.begin();

    rgbLed.begin();
    rgbLed.clear();

    // The lora device is customizable.
    // lora_uart.setAddress(1); //each of these lora modules has an "address", just a number to specify recipiant of message.
    digitalWrite(RST, HIGH); // release reset (low means reset) (idk what this does ngl)
    queueMutex = xSemaphoreCreateMutex();
    BLEDevice::init("capstone device");
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyBLEServerCallbacks());
    BLEService *pService = bleServer->createService(SERVICE_UUID);

    // "Descriptors" are metadata attached to characteristics that change the behaivour.
    // This BLE2902, decides if the client (the phone) gets notified of data changes for pTXChara..
    // Attaching this descriptor gives the client the option to get notified of data changes.
    bleTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY); // Create a BLE Characteristic
    bleTxCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic *bleRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    bleRxCharacteristic->setCallbacks(new MyBLECharacteristicCallbacks());
    pService->start();
    bleServer->getAdvertising()->start(); //"Advertising" = broadcasting our precense in bluetooth.

    delay(1000);
    Serial.println("LoRa UART ready");
    lora_uart.setRFPower(12); // Max range is 22. Range(high) vs Speed(low)

    rylr_link.begin();
    lora_mesh.announce(); // join the mesh

    // In setup(), after lora_uart.setSerial(&Serial1):
    delay(1000);

    // Check AT
    Serial1.println("AT");
    delay(500);
    while (Serial1.available())
        Serial.write(Serial1.read());

    // Check address
    Serial1.println("AT+ADDRESS?");
    Serial.printf("My mesh address: %d\n", nodeAddr);
    delay(500);
    while (Serial1.available())
        Serial.write(Serial1.read());

    // Check network ID
    Serial1.println("AT+NETWORKID?");
    delay(500);
    while (Serial1.available())
        Serial.write(Serial1.read());

    // Check RF params
    Serial1.println("AT+PARAMETER?");
    delay(500);
    while (Serial1.available())
        Serial.write(Serial1.read());

    lora_mesh.send(0, "MESH HELLO"); // Broadcast via mesh
}

unsigned long lastPing = 0;
const unsigned long PING_INTERVAL = 150000; // every 150 seconds
void loop()
{

    // ---------- LED stuff
    if (millis() - lastLedUpdate >= 5)
    { // Is it time to update yet?
        lastLedUpdate = millis();
        float phase = radians(ledColorPhase);
        int maxBrightness = 3; // 5 out of 255, why is the led so bright as the sun
        int brightness = (maxBrightness / 2.0) * (1.0 + sin(phase - PI / 2));
        uint32_t color = currentAnimation(ledColorPhase);
        rgbLed.setPixelColor(0, color);
        rgbLed.show();

        ledColorPhase = (ledColorPhase + 1) % 360;
    }

    // ------- Serial over USB
    if (Serial.available() > 0)
    {
        currentAnimation = LEDAnimHeartbeat;
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() > 0)
        {
            BLECommand cmd;
            if (parseSerialCommand(input, cmd))
            {
                if (xSemaphoreTake(queueMutex, portMAX_DELAY))
                {
                    bleRXQueue.push(cmd);
                    xSemaphoreGive(queueMutex);
                }
            }
            else
            {
                Serial.println("[CMD] bad command. Use: A<msg> or T<addr> <msg>");
            }
        }
    }

    // ------- Random ping broadcast
    if (millis() - lastPing >= PING_INTERVAL)
    {
        lastPing = millis();
        // BLECommand ping;
        // ping.type = 'A';
        // ping.target = 0;
        // ping.message = "PING from " + String(rylr_link.getAddress());
        // if (xSemaphoreTake(queueMutex, portMAX_DELAY)) {
        //     bleRXQueue.push(ping);
        //     xSemaphoreGive(queueMutex);
        // }
        lora_mesh.announce(); // add this alongside the ping
    }

    // Print any messages we received on lora mesh
    if (lora_mesh.poll())
    {
        currentAnimation = LEDAnimHeartbeat;

        uint16_t sender = lora_mesh.getLastSender();
        String msg = lora_mesh.getLastMessage();

        // Log to USB serial
        Serial.printf("[MESH RX] From %d: %s\n", sender, msg.c_str());

        // Forward to connected BLE phone in a readable format
        // Format: "FROM <addr>: <msg>\n"
        String bleMsg = "FROM:" + String(sender) + ":MSG:" + msg + "\n";
        flash.writeStr(address, bleMsg);
        address+=sizeof(bleMsg);
        bleSendString(bleMsg);
    }

    // ------- BLE TX queue drain
    if (deviceConnected && bleTxCharacteristic != NULL &&
        !bleTXQueue.empty() &&
        millis() - lastBleSendTime >= BLE_SEND_INTERVAL)
    {
        lastBleSendTime = millis();
        String chunk = bleTXQueue.front();
        bleTXQueue.pop();
        bleTxCharacteristic->setValue((uint8_t *)chunk.c_str(), chunk.length());
        bleTxCharacteristic->notify();
    }

    while (!bleRXQueue.empty())
    {
        BLECommand cmd;
        if (xSemaphoreTake(queueMutex, 0))
        {
            if (bleRXQueue.empty())
            {
                xSemaphoreGive(queueMutex);
                break;
            }
            cmd = bleRXQueue.front();
            bleRXQueue.pop();
            xSemaphoreGive(queueMutex);
        }
        else
            break;

        if (cmd.type == 'R')
        {
            Serial.printf("[INFO] BLE info request: %s\n", cmd.message.c_str());
            handleInfoRequest(cmd.message);
        }
        else if (cmd.type == 'A')
        {
            Serial.printf("[LORA TX] Broadcast: %s\n", cmd.message.c_str());
            lora_mesh.send(0, cmd.message.c_str());
        }
        else if (cmd.type == 'T')
        {
            Serial.printf("[LORA TX] To %d: %s\n", cmd.target, cmd.message.c_str());
            lora_mesh.send(cmd.target, cmd.message.c_str());
        }
    }

    // disconnecting
    if (!deviceConnected && wasDeviceConnected)
    {
        delay(500);                  // give the bluetooth stack the chance to get things ready (what does this mean??)
        bleServer->startAdvertising(); // restart advertising (broadcasting our precense)
        Serial.println("Started advertising again...");
        wasDeviceConnected = false;
    }
    // connecting
    if (deviceConnected && !wasDeviceConnected)
    {
        //Dump Messages
        // Our flash devices are not rated for 5 volts, so we will not be demoing it, but this method should work when the chip is supplied with 3.3 V
        int tempAddr = 0;
        String msg;
        do{
            msg = flash.readStr(tempAddr, msg);
            bleTxCharacteristic->setValue((uint8_t *)msg.c_str(), msg.length());
            bleTxCharacteristic->notify();
            flash.eraseSector(tempAddr);
            tempAddr +=sizeof(msg);

        }while(msg)!= "";
        

        wasDeviceConnected = true;
    }
}
