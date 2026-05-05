#include "MeshLink.h"
#include "UART_LoRaAT.h"
#include <string.h>

// This object has a UART_LoRaAT object reference
// and implements sending and receiving messages with it.
class RYLR998Link : public MeshLink
{
public:
    RYLR998Link(UART_LoRaAT &lora, uint16_t addr) : lora(lora), address(addr) {}

    void begin()
    {
        lora.setAddress(address);
    }

    void send(const uint8_t *data, size_t len, uint16_t to_addr) override
    {
        lora.startTxMessage();
        lora.addTxData((const char *)data);
        lora.sendTxMessage(to_addr);
    }

    bool readyToSend() override { return lora.readyToSend(); }

    bool poll(uint8_t *buffer, size_t *len, uint16_t *from_addr) override
    {
        UART_LoRaAT_Message *msg = lora.checkMessage();
        if (!msg)
            return false;

        *from_addr = msg->from_address;
        *len = msg->data_len;
        memcpy(buffer, msg->data, msg->data_len);
        buffer[msg->data_len] = '\0'; // Safety

        return true;
    }

    uint16_t getAddress() override { return address; }

private:
    UART_LoRaAT &lora;
    uint16_t address;
};
