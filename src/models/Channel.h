#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "JsonUtil.h"

namespace cardmesh::models {

struct Channel {
    std::string id;
    std::string name;
    int unreadCount = 0;

    static Channel fromJson(const nlohmann::json& j) {
        Channel channel;
        // MeshMonitor's channel "id" is a small integer (channel index 0-7),
        // unlike Source/Node ids which are strings -- accept either so a
        // numeric id doesn't throw a json type_error.
        auto idIt = j.find("id");
        if (idIt != j.end() && !idIt->is_null()) {
            channel.id = idIt->is_string() ? idIt->get<std::string>() : idIt->dump();
        }
        channel.name = requiredField<std::string>(j, "name", channel.id);
        channel.unreadCount = requiredField<int>(j, "unreadCount", 0);
        return channel;
    }
};

}  // namespace cardmesh::models
