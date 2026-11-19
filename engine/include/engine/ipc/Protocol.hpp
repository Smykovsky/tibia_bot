#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace bot::ipc {

inline nlohmann::json makeSuccessResponse(const nlohmann::json& id, const nlohmann::json& result) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    response["id"] = id;
    return response;
}

inline nlohmann::json makeErrorResponse(const nlohmann::json& id, int code, const std::string& message) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"] = {
        {"code", code},
        {"message", message}
    };
    response["id"] = id;
    return response;
}

inline nlohmann::json makeNotification(const std::string& method, const nlohmann::json& params) {
    nlohmann::json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = method;
    notification["params"] = params;
    return notification;
}

} // namespace bot::ipc
