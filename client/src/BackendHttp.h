#ifndef BACKEND_HTTP_H
#define BACKEND_HTTP_H
#include "httplib.h"
#include "RuntimeOptions.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <chrono>

typedef std::shared_ptr<httplib::Response> BackendResponse;

inline void SetBackendTimeouts(httplib::Client &client) {
    const RuntimeOptions &options = ClientOptions();
    client.set_connection_timeout(options.connectTimeoutMs / 1000, (options.connectTimeoutMs % 1000) * 1000);
    client.set_read_timeout(options.readTimeoutMs / 1000, (options.readTimeoutMs % 1000) * 1000);
    client.set_write_timeout(options.writeTimeoutMs / 1000, (options.writeTimeoutMs % 1000) * 1000);
    client.set_max_timeout(options.totalTimeoutMs);
}

// The worker owns HTTP clients only. Completion handlers run on the game thread.
class BackendHttpQueue {
public:
    struct Status { size_t outstanding = 0; unsigned int failures = 0; int lastHttp = 0; bool online = false; std::string error, lastError; };
    typedef std::function<void(const BackendResponse &)> Callback;
    explicit BackendHttpQueue(const std::string &base) : base_(base), stopping_(false), outstanding_(0), worker_(&BackendHttpQueue::Run, this) {}
    ~BackendHttpQueue() {
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; pending_.clear(); }
        wake_.notify_one();
        worker_.join();
    }
    bool Submit(const std::string &method, const std::string &path, const std::string &body,
        Callback callback, const std::string &key = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_ || outstanding_ >= 128 || (!key.empty() && keys_.count(key))) return false;
        if(!key.empty()) keys_.insert(key);
        pending_.push_back(Job{method, path, body, key, callback, std::chrono::steady_clock::now()});
        ++outstanding_;
        wake_.notify_one();
        return true;
    }
    void Pump() {
        std::deque<Completion> ready;
        { std::lock_guard<std::mutex> lock(mutex_);
          ready.swap(completed_);
          for(const Completion &item : ready) {
              if(!item.key.empty()) keys_.erase(item.key);
              --outstanding_;
          }
        }
        for(const Completion &item : ready) {
            try { if(item.callback) item.callback(item.response); }
            catch(const std::exception &) { std::lock_guard<std::mutex> lock(mutex_); status_.lastError = status_.error = "backend response could not be processed"; }
            catch(...) { std::lock_guard<std::mutex> lock(mutex_); status_.lastError = status_.error = "backend response handler failed"; }
        }
    }
    Status GetStatus() { std::lock_guard<std::mutex> lock(mutex_); Status value = status_; value.outstanding = outstanding_; return value; }
private:
    struct Job { std::string method, path, body, key; Callback callback; std::chrono::steady_clock::time_point queuedAt; };
    struct Completion { std::string key; Callback callback; BackendResponse response; };
    void Run() {
        for(;;) {
            Job job;
            { std::unique_lock<std::mutex> lock(mutex_);
              wake_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
              if(stopping_) return;
              job = std::move(pending_.front()); pending_.pop_front();
            }
            BackendResponse response;
            try {
                if(std::chrono::steady_clock::now() - job.queuedAt > std::chrono::seconds(10))
                    throw std::runtime_error("Backend request expired in queue");
                httplib::Client client(base_);
                SetBackendTimeouts(client);
                auto result = job.method == "GET" ? client.Get(job.path.c_str()) :
                    client.Post(job.path.c_str(), job.body, "application/json");
                if(result) response = std::make_shared<httplib::Response>(*result);
            } catch(...) {}
            { std::lock_guard<std::mutex> lock(mutex_);
              status_.lastHttp = response ? response->status : 0;
              status_.online = response && response->status < 500;
              if(status_.online) { status_.failures = 0; status_.error.clear(); }
              else { ++status_.failures; status_.error = response ? "backend HTTP error" : "backend unavailable or timed out"; status_.lastError = status_.error; }
              completed_.push_back(Completion{job.key, std::move(job.callback), std::move(response)});
            }
        }
    }
    std::string base_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> pending_;
    std::deque<Completion> completed_;
    std::set<std::string> keys_;
    bool stopping_;
    size_t outstanding_;
    Status status_;
    std::thread worker_;
    BackendHttpQueue(const BackendHttpQueue &) = delete;
    BackendHttpQueue &operator=(const BackendHttpQueue &) = delete;
};

inline std::string BackendQueryValue(const std::string &value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for(unsigned char ch : value) {
        if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') encoded += ch;
        else { encoded += '%'; encoded += hex[ch >> 4]; encoded += hex[ch & 15]; }
    }
    return encoded;
}
#endif
