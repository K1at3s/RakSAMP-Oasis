#include "../src/BackendHttp.h"
#include "../src/OperationQueue.h"
#include "../src/DurableResults.h"
#include <psapi.h>
#include <atomic>
#include <iostream>
#undef NDEBUG
#include <cassert>

static size_t PrivateBytes() {
    PROCESS_MEMORY_COUNTERS_EX memory = {};
    assert(GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memory, sizeof(memory)));
    return memory.PrivateUsage;
}
int main(int argc, char **argv) {
    const int seconds = argc > 1 ? atoi(argv[1]) : 60;
    assert(seconds >= 1 && seconds <= 86400);
    const std::string report = argc > 2 ? argv[2] : "client/tests/obj/stress/summary.json";
    ClientOptions().connectTimeoutMs = ClientOptions().readTimeoutMs = 100;
    ClientOptions().writeTimeoutMs = 100; ClientOptions().totalTimeoutMs = 300;
    httplib::Server server;
    std::atomic<unsigned int> requests(0);
    server.Get("/chaos", [&](const httplib::Request &, httplib::Response &response) {
        const unsigned int id = ++requests;
        if(id % 41 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(160));
        else if(id % 5 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(15));
        response.status = id % 7 == 0 ? 503 : 200;
        response.set_content(std::string(2048, 'x'), "text/plain");
    });
    const int port = server.bind_to_any_port("127.0.0.1"); assert(port > 0);
    std::thread listener([&] { server.listen_after_bind(); });
    unsigned long long accepted = 0, completed = 0, rejected = 0, failed = 0, ticks = 0, diskFailures = 0, offlineFailures = 0;
    size_t maxQueue = 0, baselineMemory = 0, peakMemory = 0;
    long long maxPumpUs = 0;
    const auto owner = std::this_thread::get_id();
    const auto start = std::chrono::steady_clock::now();
    const std::string file = "client/tests/obj/stress/outbox-" + std::to_string(GetCurrentProcessId()) + ".json";
    OperationQueue operations;
    unsigned int generation = 1;
    int lastPrinted = -1;
    {
        BackendHttpQueue queue("http://127.0.0.1:" + std::to_string(port));
        while(std::chrono::steady_clock::now() - start < std::chrono::seconds(seconds)) {
            ++ticks;
            for(int i = 0; i < 6; ++i) {
                const bool queued = queue.Submit("GET", "/chaos", "", [&](const BackendResponse &response) {
                    assert(std::this_thread::get_id() == owner);
                    ++completed;
                    if(!response || response->status >= 500) ++failed;
                }, "job-" + std::to_string(ticks) + "-" + std::to_string(i));
                if(queued) ++accepted; else ++rejected;
            }
            const auto beforePump = std::chrono::steady_clock::now(); queue.Pump();
            const auto pumpUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - beforePump).count();
            if(pumpUs > maxPumpUs) maxPumpUs = pumpUs;
            const size_t outstanding = queue.GetStatus().outstanding;
            assert(outstanding <= 128); if(outstanding > maxQueue) maxQueue = outstanding;
            for(int i = 0; i < 10; ++i) {
                QueuedOperation item; item.command = std::to_string(ticks) + "-" + std::to_string(i);
                item.actor = "Actor"; item.group = "Group"; item.generation = generation; item.started = static_cast<uint32_t>(ticks);
                operations.Push(item, 24);
            }
            assert(operations.Size() <= 24);
            if(ticks % 128 == 0) ++generation;
            QueuedOperation item;
            while(operations.Pop(static_cast<uint32_t>(ticks), generation, 100, "Group", [](const QueuedOperation &row) { return row.actor == "Actor"; }, item)) assert(item.generation == generation);
            if(ticks % 1000 == 0) {
                DurableResults outbox(file);
                auto first = nlohmann::json{{"hwid", "BOT-A"}, {"warId", std::to_string(ticks)}, {"ownGroup", "Group"}};
                assert(outbox.Enqueue(first, 8));
                HANDLE lock = CreateFileA(file.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                assert(lock != INVALID_HANDLE_VALUE);
                auto second = first; second["warId"] = std::to_string(ticks) + "-2";
                assert(!outbox.Enqueue(second, 8) && outbox.Size() == 2); ++diskFailures;
                CloseHandle(lock);
                assert(outbox.Enqueue(second, 8));
                DurableResults restarted(file); assert(restarted.Size() == 2 && restarted.Front()->at("ownGroup") == "Group");
                assert(restarted.Ack("BOT-A", first["warId"].get<std::string>()));
                assert(restarted.Ack("BOT-A", second["warId"].get<std::string>()));
                // A separate closed loopback port exercises actual connection failure.
                httplib::Server unused; const int closed = unused.bind_to_any_port("127.0.0.1"); assert(closed > 0); unused.stop();
                BackendHttpQueue offline("http://127.0.0.1:" + std::to_string(closed));
                bool done = false;
                assert(offline.Submit("GET", "/", "", [&](const BackendResponse &response) { assert(!response); done = true; ++offlineFailures; }));
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while(!done && std::chrono::steady_clock::now() < deadline) { offline.Pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
                assert(done);
            }
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count());
            const size_t memory = PrivateBytes();
            if(elapsed >= 5 && !baselineMemory) baselineMemory = memory;
            if(memory > peakMemory) peakMemory = memory;
            if(baselineMemory) assert(memory <= baselineMemory + 32 * 1024 * 1024);
            if(elapsed / 60 != lastPrinted) { lastPrinted = elapsed / 60; std::cout << "Stress: " << elapsed << "s, completed=" << completed << ", queue=" << outstanding << std::endl; }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const auto drainUntil = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while(queue.GetStatus().outstanding && std::chrono::steady_clock::now() < drainUntil) { queue.Pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        assert(queue.GetStatus().outstanding == 0 && completed == accepted);
    }
    server.stop(); listener.join();
    assert(completed > 0 && failed > 0 && rejected > 0 && maxPumpUs < 500000);
    nlohmann::json result = {{"seconds", seconds}, {"accepted", accepted}, {"completed", completed}, {"queueRejected", rejected}, {"httpFailures", failed},
        {"offlineFailures", offlineFailures}, {"diskWriteFailures", diskFailures}, {"maxQueue", maxQueue}, {"maxPumpMicroseconds", maxPumpUs},
        {"baselinePrivateBytes", baselineMemory}, {"peakPrivateBytes", peakMemory}, {"operationTicks", ticks}, {"passed", true}};
    std::ofstream output(report, std::ios::binary); assert(output); output << result.dump(2); output.close();
    DeleteFileA(file.c_str()); DeleteFileA((file + ".tmp").c_str());
    std::cout << "Runtime stress: OK " << result.dump() << std::endl;
}
