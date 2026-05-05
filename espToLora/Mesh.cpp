#include "Mesh.h"
#include <stdio.h>
#include <string.h>
#include "Arduino.h" // i use a fake arduino.h when im compiling test_mesh.cpp for simulation.
// #include <Arduino.h>

// This code uses the RYLRLink object to make a mesh of nodes

Mesh::Mesh(MeshLink &link)
    : link(link), nextMsgId(1), historyIndex(0), routingCount(0),
      queueHead(0), queueTail(0), queueCount(0)
{
    memset(history, 0, sizeof(history));
    memset(lastData, 0, sizeof(lastData));
    memset(routingTable, 0, sizeof(routingTable));
    memset(txQueue, 0, sizeof(txQueue));
}

// just a wrapper for enqueueing a message
void Mesh::send(uint16_t target, const char *data)
{
    MeshPacket pkg;
    pkg.msg_id = nextMsgId++;
    pkg.sender = link.getAddress();
    pkg.target = target;
    pkg.ttl = DEFAULT_TTL;
    strncpy(pkg.data, data, MESH_MAX_DATA - 1);
    pkg.data[MESH_MAX_DATA - 1] = '\0';

    addToHistory(pkg.msg_id, pkg.sender);

    uint8_t buffer[MESH_PACKET_HEADER_SIZE + MESH_MAX_DATA];
    int len = serialize(pkg, buffer);

    uint16_t next_hop = getNextHop(target);
    enqueue(target, next_hop, buffer, len);
}

void Mesh::announce()
{
    // Format: 'A' + origin (2B BE) + hops (1B)
    uint8_t buffer[MESH_ANN_SIZE];
    uint16_t addr = link.getAddress();
    buffer[0] = MESH_PACKET_ANN;
    buffer[1] = (addr >> 8) & 0xFF;
    buffer[2] = addr & 0xFF;
    buffer[3] = 0;
    enqueue(0, 0, buffer, MESH_ANN_SIZE);
}

bool Mesh::enqueue(uint16_t target, uint16_t next_hop, const uint8_t *data, int len)
{
    if (queueCount >= MESH_QUEUE_SIZE)
    {
        Serial.println("[MESH] TX queue full, dropping packet");
        return false;
    }
    QueueEntry &e = txQueue[queueTail];
    e.target = target;
    e.next_hop = next_hop;
    e.data_len = min(len, (int)sizeof(e.data));
    memcpy(e.data, data, e.data_len);

    queueTail = (queueTail + 1) % MESH_QUEUE_SIZE;
    queueCount++;
    return true;
}

void Mesh::drainQueue()
{
    if (queueCount == 0)
        return;
    if (!link.readyToSend())
        return;

    QueueEntry &e = txQueue[queueHead];

    // Re-resolve next hop in case routing table improved since enqueue
    uint16_t next_hop = getNextHop(e.target);
    if (next_hop != e.next_hop)
    {
        Serial.printf("[MESH] updated next_hop for %d: %d -> %d\n",
                      e.target, e.next_hop, next_hop);
        e.next_hop = next_hop;
    }

    link.send(e.data, e.data_len, e.next_hop);
    // e.last_sent = millis();

    queueHead = (queueHead + 1) % MESH_QUEUE_SIZE;
    queueCount--;
}

bool Mesh::poll()
{

    drainQueue();

    uint8_t buffer[MESH_PACKET_HEADER_SIZE + MESH_MAX_DATA];
    size_t len;
    uint16_t from;
    if (!link.poll(buffer, &len, &from))
        return false;

    Serial.printf("[MESH RAW] from=%d len=%d type=%02X\n", from, len, buffer[0]);
    // This whole block is for propagating announcments
    if (buffer[0] == MESH_PACKET_ANN)
    {
        // Format: 'A' + origin (2B BE) + hops (1B)
        if (len < MESH_ANN_SIZE)
            return false;
        uint16_t origin = ((uint16_t)buffer[1] << 8) | buffer[2];
        uint8_t hops = buffer[3];

        if (origin != link.getAddress())
        {
            if (updateRoutingTable(origin, from, hops + 1))
            {
                // Only forward if this announcement actually GAVE us a better route
                if (hops < DEFAULT_TTL)
                {
                    uint8_t nextAnn[MESH_ANN_SIZE];
                    nextAnn[0] = MESH_PACKET_ANN;
                    nextAnn[1] = (origin >> 8) & 0xFF;
                    nextAnn[2] = origin & 0xFF;
                    nextAnn[3] = hops + 1;
                    // link.send(nextAnn, MESH_ANN_SIZE, 0); // we could send directly
                    enqueue(0, 0, nextAnn, MESH_ANN_SIZE);
                }
            }
        }
        return false;
    }

    MeshPacket pkg;
    if (!deserialize(buffer, pkg))
        return false;

    // Serial.printf("[MESH] deserialized ok — target=%d myaddr=%d duplicate=%d\n",
    // pkg.target, link.getAddress(), isDuplicate(pkg.msg_id, pkg.sender));

    if (isDuplicate(pkg.msg_id, pkg.sender))
    {
        return false;
    }

    addToHistory(pkg.msg_id, pkg.sender);
    updateRoutingTable(pkg.sender, from, 1);

    if (pkg.ttl > DEFAULT_TTL) pkg.ttl = DEFAULT_TTL;
    // Is it for me? (or broadcast)
    if (pkg.target == link.getAddress() || pkg.target == 0)
    {
        strncpy(lastData, pkg.data, MESH_MAX_DATA - 1);
        lastSender = pkg.sender;

        // If it was a broadcast, we might still want to forward it
        if (pkg.target == 0 && pkg.ttl > 0)
        {
            forward(pkg, from);
        }
        return true;
    }

    // Not for me, forward if TTL > 0
    if (pkg.ttl > 0)
    {
        forward(pkg, from);
    }

    return false;
}

void Mesh::forward(MeshPacket &pkg, uint16_t exclude)
{
    pkg.ttl--;
    uint8_t buffer[MESH_PACKET_HEADER_SIZE + MESH_MAX_DATA];
    int len = serialize(pkg, buffer);

    uint16_t next_hop = getNextHop(pkg.target, exclude);
    if (next_hop == exclude)
        return; // nowhere to send that isn't back where it came from
    enqueue(pkg.target, next_hop, buffer, len);
}

bool Mesh::isDuplicate(uint16_t msg_id, uint16_t sender)
{
    for (int i = 0; i < MAX_MESH_HISTORY; i++)
    {
        if (history[i].msg_id == msg_id && history[i].sender == sender)
        {
            return true;
        }
    }
    return false;
}

void Mesh::addToHistory(uint16_t msg_id, uint16_t sender)
{
    history[historyIndex].msg_id = msg_id;
    history[historyIndex].sender = sender;
    historyIndex = (historyIndex + 1) % MAX_MESH_HISTORY;
}

int Mesh::serialize(const MeshPacket &pkg, uint8_t *buffer)
{
    // Format: 'M' + msg_id (2B BE) + sender (2B BE) + target (2B BE) + ttl (1B) + data
    buffer[0] = MESH_PACKET_HEADER;
    buffer[1] = (pkg.msg_id >> 8) & 0xFF;
    buffer[2] = pkg.msg_id & 0xFF;
    buffer[3] = (pkg.sender >> 8) & 0xFF;
    buffer[4] = pkg.sender & 0xFF;
    buffer[5] = (pkg.target >> 8) & 0xFF;
    buffer[6] = pkg.target & 0xFF;
    buffer[7] = pkg.ttl;
    int data_len = strlen(pkg.data);
    memcpy(buffer + MESH_PACKET_HEADER_SIZE, pkg.data, data_len);
    return MESH_PACKET_HEADER_SIZE + data_len;
}

uint16_t Mesh::getNextHop(uint16_t target, uint16_t exclude)
{
    for (int i = 0; i < routingCount; i++)
    {
        if (routingTable[i].destination == target &&
            routingTable[i].next_hop != exclude)
        {
            return routingTable[i].next_hop;
        }
    }

    return 0;
}

bool Mesh::updateRoutingTable(uint16_t dest, uint16_t next_hop, uint8_t hops)
{
    // Check if we already have this destination
    for (int i = 0; i < routingCount; i++)
    {
        if (routingTable[i].destination == dest)
        {
            if (hops < routingTable[i].hop_count)
            {
                routingTable[i].next_hop = next_hop;
                routingTable[i].hop_count = hops;
                return true;
            }
            return false;
        }
    }

    // Add new entry
    if (routingCount < MAX_ROUTING_ENTRIES)
    {
        routingTable[routingCount++] = {dest, next_hop, hops};
        return true;
    }
    return false;
}

bool Mesh::deserialize(const uint8_t *buffer, MeshPacket &pkg)
{
    if (buffer[0] != MESH_PACKET_HEADER)
        return false;

    pkg.msg_id = ((uint16_t)buffer[1] << 8) | buffer[2];
    pkg.sender = ((uint16_t)buffer[3] << 8) | buffer[4];
    pkg.target = ((uint16_t)buffer[5] << 8) | buffer[6];
    pkg.ttl = buffer[7];

    strncpy(pkg.data, (const char *)(buffer + MESH_PACKET_HEADER_SIZE), MESH_MAX_DATA - 1);
    pkg.data[MESH_MAX_DATA - 1] = '\0';

    return true;
}