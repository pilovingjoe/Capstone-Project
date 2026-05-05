#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <queue>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <random>

#include "Mesh.h"
#include "MeshLink.h"

// THIS FILE IS JUST FOR TESTING
// I didnt even write this i just made ai write it.
// u have to compile it with mesh.cpp and use the fake arduino stub file

// --- BANDWIDTH SIMULATION CONSTANTS ---
const double DISCOVERY_LOSS_CHANCE = 0.00;
const double DATA_LOSS_CHANCE = 0.05;
double g_currentLossChance = DISCOVERY_LOSS_CHANCE;
const double BYTES_PER_TICK = 100.0;

struct InflightPacket
{
    int arrivalTick;
    uint16_t fromAddr;
    uint16_t toAddr;
    std::vector<uint8_t> data;
};

std::vector<InflightPacket> g_inflight;
int g_currentTick = 0;
std::map<uint16_t, std::vector<uint16_t>> g_links;
std::mt19937 g_rng(42);

class SimulatedLink : public MeshLink
{
public:
    uint16_t address;
    std::queue<std::pair<uint16_t, std::vector<uint8_t>>> rxQueue;
    int busyUntil = 0;

    SimulatedLink(uint16_t addr) : address(addr) {}

    void send(const uint8_t *data, size_t len, uint16_t to_addr) override
    {
        int airtime = (int)(len / BYTES_PER_TICK) + 1;
        busyUntil = g_currentTick + airtime;

        for (uint16_t neighbor : g_links[address])
        {
            if (to_addr != 0 && neighbor != to_addr)
                continue;

            std::uniform_int_distribution<int> delayDist(2, 6);
            std::uniform_real_distribution<double> lossDist(0.0, 1.0);

            if (lossDist(g_rng) > g_currentLossChance)
            {
                InflightPacket p;
                p.arrivalTick = g_currentTick + airtime + delayDist(g_rng);
                p.fromAddr = address;
                p.toAddr = neighbor;
                p.data.assign(data, data + len);
                g_inflight.push_back(p);
            }
        }
    }

    bool poll(uint8_t *buffer, size_t *len, uint16_t *from_addr) override
    {
        if (rxQueue.empty())
            return false;

        auto packet = rxQueue.front();
        rxQueue.pop();

        *from_addr = packet.first;
        *len = packet.second.size();
        memcpy(buffer, packet.second.data(), *len);
        buffer[*len] = '\0';
        return true;
    }

    // Sim link is never "AT-layer busy" — queue can always drain
    bool readyToSend() override { return true; }

    uint16_t getAddress() override { return address; }
};

// Deliver any in-flight packets whose arrival tick has passed
static void deliverPackets()
{
    auto it = g_inflight.begin();
    while (it != g_inflight.end())
    {
        if (it->arrivalTick <= g_currentTick)
        {
            extern std::map<uint16_t, SimulatedLink *> g_link_objs;
            g_link_objs[it->toAddr]->rxQueue.push({it->fromAddr, it->data});
            it = g_inflight.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::map<uint16_t, Mesh *> g_nodes;
std::map<uint16_t, SimulatedLink *> g_link_objs;

static void tickAll(int ticks)
{
    for (int t = 0; t < ticks; t++, g_currentTick++)
    {
        deliverPackets();
        for (auto &[addr, node] : g_nodes)
            node->poll();
    }
}

int main()
{
    const int NUM_NODES = 50;
    std::cout << "--- Smart Mesh & Bandwidth Simulation ---" << std::endl;

    for (int i = 1; i <= NUM_NODES; i++)
    {
        g_link_objs[i] = new SimulatedLink(i);
        g_nodes[i] = new Mesh(*g_link_objs[i]);
    }

    // Random graph topology (~4 neighbors each)
    for (int i = 1; i <= NUM_NODES; i++)
    {
        while ((int)g_links[i].size() < 4)
        {
            int neighbor = 1 + (g_rng() % NUM_NODES);
            if (neighbor != i &&
                std::find(g_links[i].begin(), g_links[i].end(), (uint16_t)neighbor) == g_links[i].end())
            {
                g_links[i].push_back(neighbor);
                g_links[neighbor].push_back(i);
            }
        }
    }

    // Phase 1: Staggered announcements
    std::cout << "Phase 1: Mesh Formation (Staggered Announcements)..." << std::endl;
    for (int i = 1; i <= NUM_NODES; i++)
    {
        g_nodes[i]->announce();
        tickAll(40);
    }

    std::cout << "Waiting for network to settle..." << std::endl;
    tickAll(1000);

    // Route table check
    std::cout << "Node 1 route table size: " << g_nodes[1]->getRouteCount() << std::endl;
    uint16_t next = g_nodes[1]->getNextHop(25);
    std::cout << "Next hop to Node 25: " << next
              << (next == 0 ? " (FLOOD)" : " (ROUTED)") << std::endl;

    // Phase 2: Data transfer with packet loss
    g_currentLossChance = DATA_LOSS_CHANCE;
    std::cout << "Phase 2: Data Transfer (Node 1 -> Node 25)..." << std::endl;
    g_nodes[1]->send(25, "Mesh Smarts!");

    bool reached = false;
    int dataPhaseEnd = g_currentTick + 2000;

    for (; g_currentTick < dataPhaseEnd; g_currentTick++)
    {
        deliverPackets();
        for (auto &[addr, node] : g_nodes)
        {
            if (node->poll() && addr == 25)
            {
                std::cout << "[Tick " << g_currentTick << "] Node 25 RECEIVED: "
                          << g_nodes[25]->getLastMessage() << std::endl;
                reached = true;
            }
        }
        if (reached && g_inflight.empty())
            break;
    }

    if (reached)
        std::cout << "SUCCESS: Smart Routing and Bandwidth Modeling Verified." << std::endl;
    else
        std::cout << "FAILED: Message lost in congestion or unknown route." << std::endl;

    // Cleanup
    for (auto &[addr, node] : g_nodes)
        delete node;
    for (auto &[addr, link] : g_link_objs)
        delete link;

    return 0;
}
