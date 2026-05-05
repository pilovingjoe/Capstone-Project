#ifndef __MESH_LINK_H__
#define __MESH_LINK_H__

#include <stddef.h>
#include <stdint.h>

// An generic interface, represents any "link" object that can be called
// to deliver a packet and receive packets.
// This allows to write test code with fake devices.

class MeshLink
{
public:
    virtual ~MeshLink() {}

    // Send or queue a raw packet over the link.
    // to_addr = 0 means broadcast.
    virtual void send(const uint8_t *data, size_t len, uint16_t to_addr) = 0;

    virtual bool readyToSend() { return true; }

    // Polls for incoming packets.
    // Returns true if a packet was received.
    // Data is copied into the provided buffer.
    // The mesh logic does assume the link is not able to receive half messages at a time.
    virtual bool poll(uint8_t *buffer, size_t *len, uint16_t *from_addr) = 0;

    // Physical address of this node on this link
    virtual uint16_t getAddress() = 0;
};

#endif
