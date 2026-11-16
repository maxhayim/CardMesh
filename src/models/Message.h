#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "JsonUtil.h"

namespace cardmesh::models {

enum class ConversationType { Channel, DirectMessage };

enum class MessageState { Queued, Sending, Sent, Ack, Failed };

inline ConversationType conversationTypeFromString(const std::string& value) {
    return value == "DM" ? ConversationType::DirectMessage : ConversationType::Channel;
}

inline MessageState messageStateFromString(const std::string& value) {
    // Real MeshMonitor deliveryState values are lowercase ("pending",
    // "delivered", "failed"), verified against the live server -- not the
    // uppercase guesses this originally checked for.
    if (value == "pending") return MessageState::Sending;
    if (value == "delivered") return MessageState::Ack;
    if (value == "failed") return MessageState::Failed;
    return MessageState::Sent;
}

struct Message {
    std::string id;
    ConversationType conversationType = ConversationType::Channel;
    std::string conversationId;
    std::string senderId;
    std::string senderName;
    std::string text;
    std::int64_t timestamp = 0;
    MessageState state = MessageState::Sent;

    static Message fromJson(const nlohmann::json& j) {
        // Field names verified directly against the real MeshMonitor server
        // (GET .../messages), not just assumed: there is no
        // "conversationType"/"conversationId"/"senderId"/"senderName"/
        // "state" in the real response -- those were an earlier guess.
        // Real shape uses "channel" (numeric index) or "toNodeId", a node
        // id string like "!8469da0a" as the sender (fromNodeId, no separate
        // display name on the message itself), and "deliveryState".
        Message message;
        message.id = requiredField<std::string>(j, "id", "");
        const bool isDm = requiredField<std::string>(j, "toNodeId", "broadcast") != "broadcast" &&
                          requiredField<std::string>(j, "toNodeId", "broadcast") != "!ffffffff";
        message.conversationType = isDm ? ConversationType::DirectMessage : ConversationType::Channel;
        const auto channelIt = j.find("channel");
        message.conversationId = (channelIt != j.end() && !channelIt->is_null())
            ? std::to_string(channelIt->get<int>())
            : requiredField<std::string>(j, "toNodeId", "");
        message.senderId = requiredField<std::string>(j, "fromNodeId", "");
        message.senderName = message.senderId;
        message.text = requiredField<std::string>(j, "text", "");
        message.timestamp = requiredField<std::int64_t>(j, "timestamp", 0);
        message.state = messageStateFromString(requiredField<std::string>(j, "deliveryState", "SENT"));
        return message;
    }
};

}  // namespace cardmesh::models
