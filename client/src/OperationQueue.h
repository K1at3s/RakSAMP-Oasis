#ifndef OPERATION_QUEUE_H
#define OPERATION_QUEUE_H
#include <deque>
#include <string>
#include <functional>
#include <cstdint>
#include <mutex>
struct QueuedOperation {
    std::string command, actor, group;
    int sender = -1;
    uint32_t started = 0, generation = 0;
    std::function<void()> action;
};
class OperationQueue {
public:
    bool Push(const QueuedOperation &item, size_t limit) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(items_.size() >= limit) return false;
        for(const auto &old : items_) if(old.command == item.command && old.actor == item.actor && old.generation == item.generation) return false;
        items_.push_back(item); return true;
    }
    bool Pop(uint32_t now, uint32_t generation, uint32_t timeout, const std::string &group,
        const std::function<bool(const QueuedOperation &)> &validActor, QueuedOperation &out) {
        std::lock_guard<std::mutex> lock(mutex_);
        while(!items_.empty()) {
            QueuedOperation item = items_.front(); items_.pop_front();
            if(item.generation != generation || item.group != group || now - item.started > timeout || !validActor(item)) continue;
            out = std::move(item); return true;
        }
        return false;
    }
    void Clear() { std::lock_guard<std::mutex> lock(mutex_); items_.clear(); }
    void CancelActor(const std::string &actor) {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto it = items_.begin(); it != items_.end();) if(it->actor == actor) it = items_.erase(it); else ++it;
    }
    size_t Size() const { std::lock_guard<std::mutex> lock(mutex_); return items_.size(); }
private:
    mutable std::mutex mutex_;
    std::deque<QueuedOperation> items_;
};
#endif
