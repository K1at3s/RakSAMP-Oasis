#ifndef DURABLE_RESULTS_H
#define DURABLE_RESULTS_H
#include <windows.h>
#include <fstream>
#include <string>
#include "json.hpp"
class DurableResults {
public:
    explicit DurableResults(const std::string &path) : path_(path) { Load(); }
    bool Enqueue(const nlohmann::json &body, size_t limit) {
        if(!healthy_ || !body.contains("warId") || !body.contains("hwid") || body.value("warId", "").empty()) return false;
        const std::string key = Key(body);
        for(const auto &row : rows_) if(Key(row) == key) return dirty_ ? Save() : true;
        if(rows_.size() >= limit) { error_ = "war result queue full"; return false; }
        rows_.push_back(body);
        // Keep an unsaved result in memory if the disk is temporarily unwritable.
        return Save();
    }
    bool Ack(const std::string &hwid, const std::string &war) {
        for(auto it = rows_.begin(); it != rows_.end(); ++it) {
            if(it->value("hwid", "") == hwid && it->value("warId", "") == war) {
                // A failed acknowledgement write leaves the old disk record for
                // replay after restart; the backend receipt prevents a second post.
                rows_.erase(it); return Save();
            }
        }
        return true;
    }
    bool Quarantine(const std::string &hwid, const std::string &war, const std::string &reason) {
        for(const auto &row : rows_) if(row.value("hwid", "") == hwid && row.value("warId", "") == war) {
            auto rejected = row; rejected["_rejection"] = reason;
            DurableResults failed(path_ + ".failed");
            if(!failed.Enqueue(rejected, 1000)) { error_ = "rejected result could not be preserved"; return false; }
            return Ack(hwid, war);
        }
        return true;
    }
    const nlohmann::json *Front() const { return rows_.empty() ? NULL : &rows_[0]; }
    size_t Size() const { return rows_.size(); }
    const std::string &Error() const { return error_; }
    bool Healthy() const { return healthy_; }
private:
    static std::string Key(const nlohmann::json &row) { return nlohmann::json::array({row.value("hwid", ""), row.value("warId", "")}).dump(); }
    void Load() {
        if(GetFileAttributesA(path_.c_str()) == INVALID_FILE_ATTRIBUTES) return;
        try {
            std::ifstream file(path_, std::ios::binary | std::ios::ate);
            if(!file || file.tellg() < 0 || file.tellg() > 8 * 1024 * 1024) throw std::runtime_error("read");
            file.seekg(0); nlohmann::json saved; file >> saved;
            if(!saved.is_array()) throw std::runtime_error("format");
            for(const auto &row : saved) if(!row.is_object() || row.value("warId", "").empty() || row.value("hwid", "").empty()) throw std::runtime_error("context");
            rows_ = std::move(saved);
        } catch(...) { healthy_ = false; error_ = "war result file unreadable; original file preserved"; }
    }
    bool Save() {
        const std::string temp = path_ + ".tmp";
        try {
            const std::string data = rows_.dump();
            HANDLE file = CreateFileA(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if(file == INVALID_HANDLE_VALUE) throw std::runtime_error("open");
            DWORD written = 0;
            bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, NULL) && written == data.size() && FlushFileBuffers(file);
            CloseHandle(file);
            if(!ok || !MoveFileExA(temp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("write");
            dirty_ = false; error_.clear(); return true;
        } catch(...) { dirty_ = true; error_ = "war result file could not be saved"; return false; }
    }
    std::string path_, error_;
    nlohmann::json rows_ = nlohmann::json::array();
    bool healthy_ = true;
    bool dirty_ = false;
};
#endif
