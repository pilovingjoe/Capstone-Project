#ifndef __LORA_MESH_H__
#define __LORA_MESH_H__

// This file defines the routing logic, so that all the links act like a mesh, spreading messages and routing tables and stuff.
// POLL must be constantly called in a loop
#include "MeshLink.h"

#define MAX_MESH_HISTORY 32 //a ring buffer of the last 32 message IDs the node has seen.
#define MAX_ROUTING_ENTRIES 32 // how many other nodes this node can know how to reach. (with flash memory this could be increased)
#define DEFAULT_TTL 64 //Each packet starts with 64 and gets decremented every time a node forwards it. Hits 0? Dead, nobody forwards it anymore. 
#define MESH_MAX_DATA 128 // LoRa has a hard packet size limit. Idk what it is exactly ngl I would just look it up 

// The routing logic here adds a header to the data you send. The first byte defines the message "type"
// These are all the message types so far.
#define MESH_PACKET_HEADER 'M' // a normal message sending any arbitrary data from a to b
#define MESH_PACKET_ANN    'A' // an announcement for routing purposes.

// format for M-packets: 'M'(1) + msg_id(2BE) + sender(2BE) + target(2BE) + ttl(1) + data
#define MESH_PACKET_HEADER_SIZE 8

// format for A-packets: 'A'(1) + origin(2BE) + hops(1)
#define MESH_ANN_SIZE 4

struct MeshPacket
{
    uint16_t msg_id;
    uint16_t sender;
    uint16_t target;
    uint8_t ttl;
    char data[MESH_MAX_DATA];
};

struct HistoryEntry
{
    uint16_t msg_id;
    uint16_t sender;
};

// "I can reach origin via the node who just sent me this, and it costs hops+1"
// This will improve over time if it finds shorter paths. 
struct RoutingEntry
{
    uint16_t destination;
    uint16_t next_hop;
    uint8_t hop_count;
};

#define MESH_QUEUE_SIZE 16

struct QueueEntry
{
    uint16_t target;
    uint16_t next_hop;
    // uint8_t retries;
    // unsigned long last_sent; //timestamp
    uint8_t data[MESH_PACKET_HEADER_SIZE + MESH_MAX_DATA];
    int data_len;
};

class Mesh
{
public:
    Mesh(MeshLink &link);

    void begin();

    // Broadcast an announcement to help neighbors build routes
    void announce();

    // Send a message via mesh (now uses routing table if available)
    void send(uint16_t target, const char *data);

    // Check for received mesh messages
    // Returns true if a message for THIS node was received
    bool poll();

    // Get the last received message content
    const char *getLastMessage() { return lastData; }
    uint16_t getLastSender() { return lastSender; }
    int getRouteCount() { return routingCount; }

    uint16_t getNextHop(uint16_t target, uint16_t exclude = 0);
    bool updateRoutingTable(uint16_t dest, uint16_t next_hop, uint8_t hops);

private:
    MeshLink &link;
    uint16_t nextMsgId;

    QueueEntry txQueue[MESH_QUEUE_SIZE];
    int queueHead;
    int queueTail;
    int queueCount;

    bool enqueue(uint16_t target, uint16_t next_hop, const uint8_t *data, int len); // returns false if fail cuz queue is full
    void drainQueue();
    HistoryEntry history[MAX_MESH_HISTORY];
    int historyIndex;

    RoutingEntry routingTable[MAX_ROUTING_ENTRIES];
    int routingCount;

    char lastData[MESH_MAX_DATA];
    uint16_t lastSender;

    bool isDuplicate(uint16_t msg_id, uint16_t sender);
    void addToHistory(uint16_t msg_id, uint16_t sender);
    void forward(MeshPacket &pkg, uint16_t exclude);

    // Serialization helpers
    int  serialize(const MeshPacket &pkg, uint8_t *buffer);
    bool deserialize(const uint8_t *buffer, MeshPacket &pkg);
};

#endif