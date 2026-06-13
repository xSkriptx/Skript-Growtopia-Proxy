#pragma once
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>

namespace utils {
    template<typename... Args>
    class EventManager {
    public:
        using Callback = std::function<void(Args...)>;
        
        bool Register(const std::string& id, Callback callback) {
            if (callbacks_.find(id) != callbacks_.end()) {
                return false;
            }
            callbacks_[id] = callback;
            return true;
        }
        
        void Remove(const std::string& id) {
            callbacks_.erase(id);
        }
        
        void Invoke(Args... args) {
            for (auto& [id, callback] : callbacks_) {
                callback(args...);
            }
        }
        
        void Clear() {
            callbacks_.clear();
        }
        
    private:
        std::unordered_map<std::string, Callback> callbacks_;
    };
}