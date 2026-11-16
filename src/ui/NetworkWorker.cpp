#include "NetworkWorker.h"

#include <chrono>
#include <ctime>
#include <utility>
#include <vector>

#include "api/MeshMonitorClient.h"

namespace cardmesh::ui {

namespace {

// MeshMonitor does not define an "active" node status; this is CardMesh's
// own heuristic for the dashboard's "Active" tile.
constexpr std::int64_t kActiveWindowSeconds = 15 * 60;

using cardmesh::api::ConnectionConfig;
using cardmesh::api::MeshMonitorClient;

}  // namespace

NetworkWorker::NetworkWorker(storage::AppSettings settings, DashboardModel& model, MapModel& mapModel,
                              MeshMonitorMessagesModel& messagesModel)
    : settings_(std::move(settings)), model_(model), mapModel_(mapModel), messagesModel_(messagesModel) {}

NetworkWorker::~NetworkWorker() { stop(); }

void NetworkWorker::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&NetworkWorker::run, this);
}

void NetworkWorker::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void NetworkWorker::requestRefreshNow() { refreshRequested_ = true; }

bool NetworkWorker::queueSendMessage(const std::string& text) {
    {
        std::lock_guard<std::mutex> lock(messagesModel_.mutex);
        if (messagesModel_.primaryChannelId.empty()) {
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(outgoingMutex_);
        outgoingQueue_.push_back(text);
    }
    refreshRequested_ = true;  // don't wait up to 10s to send/show it
    return true;
}

void NetworkWorker::run() {
    using namespace std::chrono_literals;

    while (running_) {
        if (refreshRequested_.exchange(false)) {
            pollOnce();
        }

        for (int waited = 0; waited < 10000 && running_; waited += 200) {
            std::this_thread::sleep_for(200ms);
            if (refreshRequested_) {
                break;
            }
        }
    }
}

void NetworkWorker::pollOnce() {
    ConnectionConfig config;
    config.server = settings_.server;
    config.port = settings_.port;
    config.https = settings_.https;
    config.apiToken = settings_.apiToken;

    MeshMonitorClient client(config);

    const auto sources = client.getSources();
    if (!sources.ok || sources.value.empty()) {
        {
            std::lock_guard<std::mutex> lock(model_.mutex);
            model_.connected = false;
            model_.statusMessage = sources.ok ? "NO SOURCES" : "SERVER UNREACHABLE";
        }
        std::lock_guard<std::mutex> lock(mapModel_.mutex);
        mapModel_.connected = false;
        return;
    }

    const models::Source* selected = nullptr;
    for (const auto& source : sources.value) {
        if (source.id == settings_.preferredSource) {
            selected = &source;
            break;
        }
    }
    if (selected == nullptr) {
        selected = &sources.value.front();
    }

    const auto nodes = client.getNodes(selected->id);
    const auto channels = client.getChannels(selected->id);

    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    int activeCount = 0;
    if (nodes.ok) {
        for (const auto& node : nodes.value) {
            if (node.lastHeard > 0 && (now - node.lastHeard) <= kActiveWindowSeconds) {
                ++activeCount;
            }
        }
    }

    int unreadCount = 0;
    std::string primaryChannelId;
    if (channels.ok) {
        for (const auto& channel : channels.value) {
            unreadCount += channel.unreadCount;
        }
        if (onChannelsUpdated) {
            onChannelsUpdated(channels.value);
        }
        // MeshMonitor convention: the Primary channel's stored name is
        // often empty (same convention mirrored for direct-LoRa decoding --
        // see ui_main.cpp). Fall back to the first channel if none match.
        for (const auto& channel : channels.value) {
            if (channel.name.empty()) {
                primaryChannelId = channel.id;
                break;
            }
        }
        if (primaryChannelId.empty() && !channels.value.empty()) {
            primaryChannelId = channels.value.front().id;
        }
    }
    if (!primaryChannelId.empty()) {
        std::lock_guard<std::mutex> lock(messagesModel_.mutex);
        messagesModel_.primaryChannelId = primaryChannelId;
    }

    sendQueuedMessages(selected->id, primaryChannelId);

    const auto messages = client.getMessages(selected->id);
    if (messages.ok) {
        std::vector<models::Message> newMessages;
        for (const auto& message : messages.value) {
            // Fall back to a composite key if "id" is ever missing from the
            // server's response -- otherwise every such message would
            // dedupe against the first (all empty ids collapsing to one).
            const std::string key = message.id.empty()
                ? (message.conversationId + "|" + message.senderId + "|" +
                   std::to_string(message.timestamp) + "|" + message.text)
                : message.id;
            if (seenMessageIds_.insert(key).second) {
                newMessages.push_back(message);
            }
        }
        if (!newMessages.empty()) {
            std::lock_guard<std::mutex> lock(messagesModel_.mutex);
            for (auto& message : newMessages) {
                messagesModel_.messages.push_back(std::move(message));
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(model_.mutex);
        model_.connected = true;
        model_.statusMessage = "CONNECTED";
        model_.sourceName = selected->name;
        model_.sourceCount = static_cast<int>(sources.value.size());
        model_.nodeCount = nodes.ok ? static_cast<int>(nodes.value.size()) : model_.nodeCount;
        model_.activeCount = activeCount;
        model_.channelCount = channels.ok ? static_cast<int>(channels.value.size()) : model_.channelCount;
        model_.unreadCount = unreadCount;
    }

    if (nodes.ok) {
        std::vector<MapNodeEntry> positioned;
        for (const auto& node : nodes.value) {
            if (!node.latitude.has_value() || !node.longitude.has_value()) {
                continue;
            }
            MapNodeEntry entry;
            entry.id = node.id;
            entry.name = node.longName.empty() ? node.shortName : node.longName;
            entry.latitude = *node.latitude;
            entry.longitude = *node.longitude;
            entry.favorite = node.favorite;
            positioned.push_back(std::move(entry));
        }

        {
            std::lock_guard<std::mutex> lock(mapModel_.mutex);
            mapModel_.connected = true;
        }
        mapModel_.replaceSourceNodes(MapNodeSource::MeshMonitor, std::move(positioned));
    }
}

void NetworkWorker::sendQueuedMessages(const std::string& sourceId, const std::string& channelId) {
    if (channelId.empty()) {
        return;  // not yet known (no successful channels fetch this run)
    }

    std::deque<std::string> toSend;
    {
        std::lock_guard<std::mutex> lock(outgoingMutex_);
        toSend.swap(outgoingQueue_);
    }
    if (toSend.empty()) {
        return;
    }

    ConnectionConfig config;
    config.server = settings_.server;
    config.port = settings_.port;
    config.https = settings_.https;
    config.apiToken = settings_.apiToken;
    MeshMonitorClient client(config);

    for (const auto& text : toSend) {
        client.sendMessage(sourceId, models::ConversationType::Channel, channelId, text);
    }
}

}  // namespace cardmesh::ui
