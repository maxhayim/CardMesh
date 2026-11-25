#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "MeshCoreMessagesModel.h"
#include "lora/LoraRadio.h"
#include "mesh/MeshCoreCrypto.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

// Drives CardMesh's direct MeshCore path on a background thread: brings up
// the SX1262 hat with MeshCore's own RF parameters (915MHz/BW250/SF10,
// distinct from Meshtastic's), and sends/receives GRP_TXT messages on known
// channels (just "Public" by default -- see KnownMeshCoreChannel).
//
// MeshCoreWorker and MeshtasticWorker both need exclusive ownership of the
// single physical LoRa radio (incompatible RF configs), so only one may be
// start()ed at a time -- the caller (ui_main.cpp) enforces this via a
// manual toggle, stopping whichever is running before starting the other.
class MeshCoreWorker {
public:
    MeshCoreWorker(storage::AppSettings& settings, MeshCoreMessagesModel& messages);
    ~MeshCoreWorker();

    MeshCoreWorker(const MeshCoreWorker&) = delete;
    MeshCoreWorker& operator=(const MeshCoreWorker&) = delete;

    void start();
    void stop();
    bool isRunning() const { return running_; }

    // Replaces the set of known channels; always includes "Public" in
    // addition to whatever is passed here, mirroring MeshtasticWorker's
    // setChannels -- see KnownMeshCoreChannel for why there's normally
    // nothing else to add yet.
    void setChannels(std::vector<mesh::KnownMeshCoreChannel> channels);

    // Queues an outgoing GRP_TXT message on the named channel; sent from
    // the worker thread on its next idle tick. Returns false only if the
    // named channel isn't known.
    bool sendText(const std::string& channelName, const std::string& text);

    // Names of every channel this node currently knows (for a channel
    // picker UI).
    std::vector<std::string> channelNames();

private:
    void run();
    void sendNext();
    void pollIncoming();

    std::vector<mesh::KnownMeshCoreChannel> channelsSnapshot();
    const mesh::KnownMeshCoreChannel* findChannel(const std::string& name,
                                                   const std::vector<mesh::KnownMeshCoreChannel>& channels);

    lora::LoraRadio radio_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex channelsMutex_;
    std::vector<mesh::KnownMeshCoreChannel> channels_;

    struct OutgoingMessage {
        std::string channelName;
        std::string text;
    };
    std::mutex outgoingMutex_;
    std::deque<OutgoingMessage> outgoingQueue_;

    storage::AppSettings& settings_;
    MeshCoreMessagesModel& messages_;
};

}  // namespace cardmesh::ui
