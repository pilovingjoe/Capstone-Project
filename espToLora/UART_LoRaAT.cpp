#include "UART_LoRaAT.h"

// checkMessage must be constantly called on a loop.
// Implements an object that represents the uart lora module, and provides functions for like sending stuff and receiving stuff.
// Keeps a small interal state machine, and queues messages, because you cannot send while waiting for the last messages AK from the uart

UART_LoRaAT::UART_LoRaAT()
 : serial(NULL),
    rx_index(0), read_index(0), tx_index(0),
    overflow_count(0), overwrite_count(0),
    rx_error_count(0), tx_message_count(0), rx_message_count(0),
    tx_state(AT_IDLE), tx_sent_at(0), last_send_result(0)
{
}

int UART_LoRaAT::checkStatus()
{
    const char* CMD = "AT\r\n";
    this->serial->write(CMD, strlen(CMD));
    return resultValue(getResponse());
}

int UART_LoRaAT::setAddress(uint16_t address)
{
    sprintf(this->tx_buffer, "AT+ADDRESS=%d\r\n", address);
    this->serial->write(this->tx_buffer, strlen(this->tx_buffer));
    return resultValue(getResponse());
}

int UART_LoRaAT::setRFParameters(uint8_t spread, uint8_t bandwidth, uint8_t coding_rate, uint8_t preamble)
{
    sprintf(this->tx_buffer, "AT+PARAMETER=%d,%d,%d,%d\r\n", spread, bandwidth, coding_rate, preamble);
    this->serial->write(this->tx_buffer, strlen(this->tx_buffer));
    return resultValue(getResponse());
}

int UART_LoRaAT::setRFPower(uint8_t power)
{
    sprintf(this->tx_buffer, "AT+CRFOP=%d\r\n", power);
    this->serial->write(this->tx_buffer, strlen(this->tx_buffer));
    return resultValue(getResponse());
}

int UART_LoRaAT::setPassword(const char* password)
{
    sprintf(this->tx_buffer, "AT+CPIN=%s\r\n", password);
    this->serial->write(this->tx_buffer, strlen(this->tx_buffer));
    return resultValue(getResponse());
}


void UART_LoRaAT::startTxMessage()
{
    tx_index = 0;
    memset(this->tx_buffer, 0xff, sizeof(this->tx_buffer));
}

void UART_LoRaAT::addTxData(const char* data)
{
    int len = min(strlen(data), size_t(MAX_DATA_LEN - this->tx_index));
    strncpy(this->tx_buffer + this->tx_index, data, len);
    this->tx_index += len;
}

void UART_LoRaAT::addTxData(int len, const char* data)
{
    len = min(len, MAX_DATA_LEN - this->tx_index);
    memcpy(this->tx_buffer + this->tx_index, data, len);
    this->tx_index += len;
}

void UART_LoRaAT::addTxData(int data)
{
    int max_len = MAX_DATA_LEN - this->tx_index;
    int count = snprintf(this->tx_buffer + this->tx_index, max_len + 1, "%d", data);
    this->tx_buffer[this->tx_index + count] = 0xff;
    this->tx_index += min(max_len, count);
}

void UART_LoRaAT::addTxData(double data)
{
    int max_len = MAX_DATA_LEN - this->tx_index;
    int count = snprintf(this->tx_buffer + this->tx_index, max_len + 1, "%.4f", data);
    this->tx_buffer[this->tx_index + count] = 0xff;
    this->tx_index += min(max_len, count);
}

int UART_LoRaAT::sendTxMessage(uint16_t to_address)
{
    if (tx_state != AT_IDLE) return -1; // busy

    char addr_str[6];
    char len_str[6];
    this->tx_message_count++;

    snprintf(addr_str, 6, "%u", to_address);
    snprintf(len_str, 6, "%d", this->tx_index);
    Serial.printf("[AT TX] AT+SEND=%s,%s,<data>\n", addr_str, len_str);

    // this->serial is the uart to the lora module.
    this->serial->print("AT+SEND=");
    this->serial->print(addr_str);
    this->serial->print(",");
    this->serial->print(len_str);
    this->serial->print(",");
    this->serial->write(this->tx_buffer, this->tx_index);
    this->serial->print("\r\n");

    this->tx_index = 0;
    this->tx_sent_at = millis();
    this->tx_state = AT_WAITING_ACK;

    return 0; // fired, not yet confirmed
}

// Receive a message
// Return NULL if no message is ready for reading.
UART_LoRaAT_Message* UART_LoRaAT::checkMessage()
{
    // Service the TX ACK state machine first
    if (tx_state == AT_WAITING_ACK) {
        if (this->serial->available()) {
            last_send_result = resultValue(getResponse());
            tx_state = AT_IDLE;
        } else if (millis() - tx_sent_at > SEND_TIMEOUT_MS) {
            Serial.println("[AT] send timeout");
            last_send_result = -1;
            tx_state = AT_IDLE;
        }
        return NULL; // don't read messages while waiting for ACK
    }

    if (this->rx_index == this->read_index && this->serial->available()) {
        this->resetRxBuffer();
        this->processInput();
    }
    if (this->rx_index > this->read_index) {
        char *ptr1, *ptr2;

        // Parse from address field
        ptr1 = this->rx_buffer + this->read_index + 5;
        ptr2 = strchr(ptr1, ',');
        if (ptr2 == NULL) {
            this->resetRxBuffer();
            this->rx_error_count++;
            return NULL;
        }
        *ptr2 = '\0';
        this->rx_message.from_address = atoi(ptr1);

        // Parse length field
        ptr1 = ptr2 + 1;
        ptr2 = strchr(ptr1, ',');
        if (ptr2 == NULL) {
            this->resetRxBuffer();
            this->rx_error_count++;
            return NULL;
        }
        *ptr2 = '\0';
        int length = atoi(ptr1);
        this->rx_message.data_len = length;

        // Extract data field — use length, not strchr, so \n inside data works fine
        ptr1 = ptr2 + 1;
        ptr2 = ptr1 + length;

        if ((ptr2 > (this->rx_buffer + this->rx_index)) || *ptr2 != ',') {
            this->resetRxBuffer();
            this->rx_error_count++;
            return NULL;
        }
        *ptr2 = '\0';
        this->rx_message.data = ptr1;

        // Extract RSSI
        ptr1 = ptr2 + 1;
        ptr2 = strchr(ptr1, ',');
        if (ptr2 == NULL) {
            this->resetRxBuffer();
            this->rx_error_count++;
            return NULL;
        }
        *ptr2 = '\0';
        this->rx_message.rssi = atoi(ptr1);

        // Extract SNR
        ptr1 = ptr2 + 1;
        this->rx_message.snr = atoi(ptr1);
        ptr2 = ptr1 + strlen(ptr1) + 1;
        this->read_index = (ptr2 - this->rx_buffer);

        if (this->read_index >= this->rx_index) {
            this->resetRxBuffer();
        }

        this->rx_message_count++;
        return &(this->rx_message);
    }
    return NULL;
}


// Process expected input from the LoRa device.
// Save any received messages in the RX buffer.
const char* UART_LoRaAT::getResponse()
{
    bool done = false;
    char* response = NULL;

    while (!done) {
        int index = processInput();
        if (index == -1) {
            done = true;
            continue;
        }

        // If this is not a RECV message, it's a command response.
        // Otherwise save it in the buffer and keep reading.
        if (strncmp(this->rx_buffer + index, RECV, strlen(RECV)) != 0) {
            this->rx_index = index;
            response = this->rx_buffer + index;
            done = true;
        }
    }
    return response;
}

// Read a SINGLE response from the lora module into the RX buffer.
// The +RECV format is:
//   +RECV=<addr>,<len>,<data>,<rssi>,<snr>\r\n
int UART_LoRaAT::processInput()
{
    bool done = false;
    int  read_count = 0;
    int  to_read_count = 0;  // bytes of raw data payload still to consume
    bool parse_data_len = false;
    int  comma_count = 0;

    // Ensure enough space in buffer
    if (this->rx_index > (RX_BUFFER_LEN - MAX_RX_MSG)) {
        this->overflow_count++;
        this->resetRxBuffer();
    }

    //continuously read till we get the full packet
    while (!done) {
        unsigned long now = millis();
        while (!this->serial->available()) {
            if (millis() - now > READ_TIMEOUT_MS) {
                this->rx_error_count++;
                this->resetRxBuffer();
                return -1;
            }
        }

        int data = this->serial->read();
        if (data == -1) {
            this->resetRxBuffer();
            this->rx_error_count++;
            return -1;
        }

        // Save byte into buffer
        if (read_count <= MAX_RX_MSG)
            this->rx_buffer[this->rx_index + read_count] = (char)data;
        read_count++;

        // After 5 bytes we know if this is a +RECV message
        if (read_count == 5) {
            if (memcmp(this->rx_buffer + this->rx_index, RECV, strlen(RECV)) == 0) {
                parse_data_len = true;
            }
        }

        // Count commas, but only while we are not inside the raw data field.
        // Once to_read_count > 0 we are consuming payload bytes — don't count commas.
        if (to_read_count == 0 && data == ',') {
            comma_count++;
        }

        // Once we've seen the 2nd comma of a +RECV we can parse the length field
        if (parse_data_len && comma_count == 2) {
            to_read_count = this->parseDataLength(read_count);
            parse_data_len = false;
        }

        if (to_read_count > 0) {
            // We are inside the raw payload — consume without any special treatment.
            // \n here is just data, not a terminator.
            to_read_count--;
        } else {
            // Outside the payload: \n ends the packet
            if (data == '\n') {
                this->rx_buffer[this->rx_index + read_count] = '\0';
                done = true;
            }
        }
    }

    if (read_count > MAX_RX_MSG) {
        this->resetRxBuffer();
        return -1;
    }

    int msg_start = this->rx_index;
    this->rx_index += read_count + 1;
    return msg_start;
}

// Clear all data in the receive buffer.
void UART_LoRaAT::resetRxBuffer()
{
    this->rx_index  = 0;
    this->read_index = 0;
}

// Extract the data length value from a partially-read +RECV message.
int UART_LoRaAT::parseDataLength(int data_size)
{
    char* ptr1 = (char*)memchr(this->rx_buffer + this->rx_index, ',', data_size);
    if (ptr1 == NULL) {
        this->rx_error_count++;
        return 0;
    }

    int offset = ptr1 - (this->rx_buffer + this->rx_index);
    char* ptr2 = (char*)memchr(ptr1 + 1, ',', data_size - offset - 1);
    if (ptr2 == NULL) {
        this->rx_error_count++;
        return 0;
    }

    *ptr2 = '\0';
    int length = atoi(ptr1 + 1);
    *ptr2 = ',';

    return length;
}

// Parse a command response string, return:
//   0   : +OK
//  >0   : error code from +ERR=<n>
//  -1   : NULL or unrecognised format
int UART_LoRaAT::resultValue(const char* valstr)
{
    if (valstr == NULL)
        return -1;

    // strcmp returns 0 on match
    if (strcmp(valstr, "+OK\r\n") == 0 || strcmp(valstr, "+OK") == 0)
        return 0;

    if (strncmp(valstr, "+ERR=", 5) == 0)
        return atoi(valstr + 5);

    return -1;
}

void UART_LoRaAT::dumpMessage()
{
    if (this->tx_buffer[this->tx_index] != 0xff) {
        this->overwrite_count++;
        Serial.print(" - overwrite -");
    }

    this->tx_buffer[this->tx_index] = '\0';
    Serial.print("len = ");
    Serial.println(strlen(this->tx_buffer));
    Serial.print(this->tx_buffer);
    Serial.println();
}

void UART_LoRaAT::dumpStats()
{
    Serial.print("Overflow count: ");
    Serial.println(this->overflow_count);
    Serial.print("Buffer overwrite count: ");
    Serial.println(this->overwrite_count);
    Serial.print("RX Errors: ");
    Serial.println(this->rx_error_count);
    Serial.print("TX Messages: ");
    Serial.println(this->tx_message_count);
    Serial.print("RX Messages: ");
    Serial.println(this->rx_message_count);
    Serial.print("TX Index: ");
    Serial.println(this->tx_index);
    Serial.print("RX Index: ");
    Serial.println(this->rx_index);
    Serial.print("Read Index: ");
    Serial.println(this->read_index);
}
