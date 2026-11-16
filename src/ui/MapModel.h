#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace cardmesh::ui {

// Which worker last reported a node. Both NetworkWorker (MeshMonitor REST)
// and MeshtasticWorker (direct LoRa) write into the same MapModel for a
// unified map, even though their message lists stay separate -- this tags
// each entry so one worker's refresh never clobbers the other's nodes.
enum class MapNodeSource { MeshMonitor, DirectLora };

struct MapNodeEntry {
    std::string id;
    std::string name;
    double latitude = 0.0;
    double longitude = 0.0;
    bool favorite = false;
    MapNodeSource source = MapNodeSource::MeshMonitor;
};

// Shared state written by NetworkWorker and MeshtasticWorker (background
// threads) and read by MapScreen (main thread). All access must hold
// `mutex`. Separate from DashboardModel since most nodes won't have a
// known position and the Dashboard doesn't need per-node detail, just
// counts.
struct MapModel {
    std::mutex mutex;

    bool connected = false;
    std::vector<MapNodeEntry> nodes;  // only nodes with a known lat/lon

    // Replaces only this source's nodes, leaving the other source's nodes
    // (written by the other worker) untouched. Caller must not hold
    // `mutex` already.
    void replaceSourceNodes(MapNodeSource source, std::vector<MapNodeEntry> freshNodes) {
        std::lock_guard<std::mutex> lock(mutex);
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                   [source](const MapNodeEntry& e) { return e.source == source; }),
                    nodes.end());
        for (auto& entry : freshNodes) {
            entry.source = source;
        }
        nodes.insert(nodes.end(), freshNodes.begin(), freshNodes.end());
    }
};

}  // namespace cardmesh::ui
