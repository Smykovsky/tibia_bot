#pragma once

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

namespace bot::core {

struct Event {
    std::string type;
    nlohmann::json data;
    int64_t timestamp;

    Event(std::string t, nlohmann::json d = {})
        : type(std::move(t)), data(std::move(d)) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

using EventCallback = std::function<void(const Event&)>;

class EventManager {
public:
    static EventManager& instance() {
        static EventManager inst;
        return inst;
    }

    size_t subscribe(const std::string& eventType, EventCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t id = ++m_nextSubscriptionId;
        m_subscribers[eventType].push_back({id, std::move(callback)});
        return id;
    }

    void unsubscribe(const std::string& eventType, size_t subscriptionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscribers.find(eventType);
        if (it != m_subscribers.end()) {
            auto& vec = it->second;
            vec.erase(
                std::remove_if(vec.begin(), vec.end(), [subscriptionId](const Subscription& sub) {
                    return sub.id == subscriptionId;
                }),
                vec.end()
            );
        }
    }

    void publish(const Event& event) {
        std::vector<EventCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_subscribers.find(event.type);
            if (it != m_subscribers.end()) {
                for (const auto& sub : it->second) {
                    callbacks.push_back(sub.callback);
                }
            }
            // Also notify wildcard subscribers ("*")
            auto wild = m_subscribers.find("*");
            if (wild != m_subscribers.end()) {
                for (const auto& sub : wild->second) {
                    callbacks.push_back(sub.callback);
                }
            }
        }

        for (const auto& cb : callbacks) {
            try {
                cb(event);
            } catch (const std::exception& e) {
                // Ignore or log error
            }
        }
    }

    void publish(const std::string& type, const nlohmann::json& data = {}) {
        publish(Event(type, data));
    }

private:
    EventManager() = default;

    struct Subscription {
        size_t id;
        EventCallback callback;
    };

    std::mutex m_mutex;
    size_t m_nextSubscriptionId{0};
    std::unordered_map<std::string, std::vector<Subscription>> m_subscribers;
};

} // namespace bot::core
