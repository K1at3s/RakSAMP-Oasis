#include "../src/BackendHttp.h"
#include "../src/HardwareIdentity.h"
#include "../src/NetworkPayload.h"
#include "../src/OperationQueue.h"
#include "../src/DurableResults.h"
#include "../src/LogPrivacy.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <atomic>

static void TestPackets() {
    RakNet::BitStream accepted;
    accepted.Write((unsigned char)16);
    accepted.Write((unsigned int)0x0100007f);
    accepted.Write((unsigned short)7777);
    accepted.Write((unsigned short)999);
    accepted.Write((unsigned int)0x12345678);
    for(unsigned int bits = 0; bits < accepted.GetNumberOfBitsUsed(); ++bits) {
        RakNet::BitStream truncated(accepted.GetData(), accepted.GetNumberOfBytesUsed(), false);
        truncated.SetWriteOffset(bits);
        unsigned short id = 42; unsigned int challenge = 99;
        assert(!ReadAcceptedConnection(truncated, id, challenge, 1000));
        assert(id == 42 && challenge == 99);
    }
    unsigned short id = 0; unsigned int challenge = 0;
    assert(ReadAcceptedConnection(accepted, id, challenge, 1000));
    assert(id == 999 && challenge == 0x12345678);
    for(unsigned short badId : { (unsigned short)1000, (unsigned short)65535 }) {
        RakNet::BitStream bad;
        bad.Write((unsigned char)16); bad.Write((unsigned int)0); bad.Write((unsigned short)7777);
        bad.Write(badId); bad.Write((unsigned int)0);
        id = 42; challenge = 99;
        assert(!ReadAcceptedConnection(bad, id, challenge, 1000) && id == 42 && challenge == 99);
    }
    const std::string payload(1024, 'x');
    RakNet::BitStream stream((unsigned char*)payload.data(), (unsigned int)payload.size(), false);
    struct Guarded { char before; char text[1024]; char after; } guarded = {};
    guarded.before = 'a'; guarded.after = 'z';
    assert(!ReadNetworkText(stream, guarded.text, 1024));
    assert(!ReadNetworkText(stream, guarded.text, -1));
    assert(!ReadNetworkText(stream, guarded.text, 65535));
    assert(ReadNetworkText(stream, guarded.text, 1023));
    assert(guarded.text[1023] == 0 && guarded.before == 'a' && guarded.after == 'z');
    char gameText[400]; stream.ResetReadPointer();
    assert(!ReadNetworkText(stream, gameText, 400));
    assert(ReadNetworkText(stream, gameText, 399));
    stream.SetReadOffset(1024 * 8 - 3);
    assert(!ReadNetworkText(stream, gameText, 1));
    stream.ResetReadPointer();
    assert(!stream.Read(gameText, -1));
    assert(!stream.Read(gameText, 0x40000000));
    assert(!stream.Read((char*)NULL, 1));
    assert(stream.Read((char*)NULL, 0));
    assert(IsSniperCommand("?zsnip") && IsSniperCommand("?zsnip off") && IsSniperCommand("?zsnip#5"));
    assert(!IsSniperCommand("?zsnipabc") && !IsSniperCommand("?zsn") && !IsSniperCommand(NULL));
}

static void TestIdentityAndQuery() {
    assert(IdentityDigest("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    const std::string first = BuildHardwareIdentity("same-cpu", "disk1", "machine1");
    assert(first == BuildHardwareIdentity("same-cpu", "disk1", "machine1"));
    assert(first != BuildHardwareIdentity("same-cpu", "disk2", "machine1"));
    assert(first != BuildHardwareIdentity("same-cpu", "disk1", "machine2"));
    assert(BuildHardwareIdentity("cpu", "", "").empty());
    assert(ScopeHardwareIdentity(first, "Bot") == ScopeHardwareIdentity(first, "bot"));
    assert(ScopeHardwareIdentity(first, "[Bot]") != ScopeHardwareIdentity(first, "Bot"));
    assert(ScopeHardwareIdentity(first, "1234567890123456A") != ScopeHardwareIdentity(first, "1234567890123456B"));
    assert(ScopeHardwareIdentity(first, "Bot").size() <= 64);
    const std::string raw = "A&B + 100% #\xC5\x9F";
    const std::string encoded = BackendQueryValue(raw);
    httplib::Params params;
    httplib::detail::parse_query_text("group=" + encoded, params);
    assert(params.size() == 1 && params.find("group")->second == raw);
}

static void TestBackgroundHttp() {
    httplib::Server server;
    std::mutex mutex; std::condition_variable wake;
    bool entered = false, release = false;
    server.Get("/slow", [&](const httplib::Request &, httplib::Response &res) {
        std::unique_lock<std::mutex> lock(mutex); entered = true; wake.notify_one();
        wake.wait(lock, [&] { return release; });
        res.set_content("done", "text/plain");
    });
    server.Get("/timeout", [](const httplib::Request &, httplib::Response &res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3500));
        res.set_content("too late", "text/plain");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    assert(port > 0);
    std::thread listener([&] { server.listen_after_bind(); });
    {
        BackendHttpQueue queue("http://127.0.0.1:" + std::to_string(port));
        const auto mainThread = std::this_thread::get_id();
        bool completed = false;
        assert(queue.Submit("GET", "/slow", "", [&](const BackendResponse &res) {
            assert(std::this_thread::get_id() == mainThread);
            assert(res && res->status == 200 && res->body == "done"); completed = true;
        }, "poll"));
        { std::unique_lock<std::mutex> lock(mutex); assert(wake.wait_for(lock, std::chrono::seconds(2), [&] { return entered; })); }
        assert(!queue.Submit("GET", "/slow", "", nullptr, "poll"));
        const auto start = std::chrono::steady_clock::now();
        for(int tick = 0; tick < 10000; ++tick) queue.Pump();
        assert(!completed && std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
        { std::lock_guard<std::mutex> lock(mutex); release = true; } wake.notify_one();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while(!completed && std::chrono::steady_clock::now() < deadline) { queue.Pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        assert(completed);
        bool timedOut = false;
        assert(queue.Submit("GET", "/timeout", "", [&](const BackendResponse &res) {
            assert(!res); timedOut = true;
        }));
        const auto timeoutDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while(!timedOut && std::chrono::steady_clock::now() < timeoutDeadline) {
            queue.Pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(timedOut);
    }
    server.stop(); listener.join();
    BackendHttpQueue offline("http://127.0.0.1:" + std::to_string(port));
    bool failed = false;
    assert(offline.Submit("GET", "/", "", [&](const BackendResponse &res) { assert(!res); failed = true; }));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while(!failed && std::chrono::steady_clock::now() < deadline) { offline.Pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    assert(failed);
}

static void TestOperationIsolation() {
    OperationQueue queue;
    QueuedOperation first; first.command = "?gstats"; first.actor = "Alice"; first.group = "Group A"; first.generation = 1; first.started = 100;
    assert(queue.Push(first, 2)); assert(!queue.Push(first, 2));
    QueuedOperation next = first; next.command = "?zsnip 5"; assert(queue.Push(next, 2));
    next.command = "?gmembers"; assert(!queue.Push(next, 2));
    QueuedOperation out;
    assert(!queue.Pop(110, 2, 1000, "Group A", [](const QueuedOperation &) { return true; }, out)); // reconnect
    assert(queue.Size() == 0);
    assert(queue.Push(first, 2));
    assert(!queue.Pop(110, 1, 1000, "Group B", [](const QueuedOperation &) { return true; }, out)); // group changed
    assert(queue.Push(first, 2));
    assert(!queue.Pop(110, 1, 1000, "Group A", [](const QueuedOperation &) { return false; }, out)); // reused player slot
    first.started = 0xfffffff0; assert(queue.Push(first, 2));
    assert(!queue.Pop(2000, 1, 1000, "Group A", [](const QueuedOperation &) { return true; }, out)); // tick rollover + expiry
    first.started = 100; assert(queue.Push(first, 2)); queue.CancelActor("Alice"); assert(queue.Size() == 0);
}

static void TestDurableResultsAndPrivacy() {
    const std::string path = "client/tests/obj/safety/war-test-" + std::to_string(GetCurrentProcessId()) + ".json";
    DeleteFileA(path.c_str());
    const nlohmann::json body = {{"hwid", "BOT-A"}, {"warId", "WAR-1"}, {"ownGroup", "Group A"}, {"redName", "Group A"}, {"blueName", "Group B"}, {"wonKnown", true}};
    {
        DurableResults outbox(path); assert(outbox.Enqueue(body, 2)); assert(outbox.Enqueue(body, 2)); assert(outbox.Size() == 1);
        auto rejected = body; rejected["warId"] = "REJECTED";
        assert(outbox.Enqueue(rejected, 2)); assert(outbox.Quarantine("BOT-A", "REJECTED", "wrong group"));
        assert(outbox.Size() == 1);
        DurableResults review(path + ".failed"); assert(review.Size() == 1 && review.Front()->at("_rejection") == "wrong group");
    }
    {
        DurableResults afterRestart(path); assert(afterRestart.Size() == 1 && *afterRestart.Front() == body);
        auto second = body; second["hwid"] = "BOT-B"; assert(afterRestart.Enqueue(second, 2));
        auto third = body; third["warId"] = "WAR-3"; assert(!afterRestart.Enqueue(third, 2));
        assert(afterRestart.Ack("BOT-A", "WAR-1")); assert(afterRestart.Size() == 1);
        assert(afterRestart.Front()->at("hwid") == "BOT-B");
        HANDLE lock = CreateFileA(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        assert(lock != INVALID_HANDLE_VALUE);
        assert(!afterRestart.Ack("BOT-B", "WAR-1") && afterRestart.Size() == 0);
        CloseHandle(lock);
    }
    { DurableResults lostAck(path); assert(lostAck.Size() == 1 && lostAck.Front()->at("hwid") == "BOT-B"); }
    { std::ofstream corrupt(path, std::ios::binary | std::ios::trunc); corrupt << "{bad-json"; }
    { DurableResults corrupt(path); assert(!corrupt.Healthy() && !corrupt.Enqueue(body, 2)); }
    { std::ifstream unchanged(path); std::string contents; std::getline(unchanged, contents); assert(contents == "{bad-json"); }
    DurableResults unwritable(path + "/missing/file.json"); assert(!unwritable.Enqueue(body, 2) && unwritable.Size() == 1 && *unwritable.Front() == body);
    DeleteFileA(path.c_str()); DeleteFileA((path + ".tmp").c_str());
    DeleteFileA((path + ".failed").c_str()); DeleteFileA((path + ".failed.tmp").c_str());
    for(const char *line : { "[LOGIN] password submitted: private123", "[SEC] input: private123", "CMD: /login private123", "sessionToken: private123", "Authorization: private123" })
        assert(RedactLogSecrets(line).find("private123") == std::string::npos);
    assert(RedactLogSecrets("[RUNTIME] Operation completed") == "[RUNTIME] Operation completed");
    unsigned int value = 5; assert(!RuntimeInteger("99999999999999999", 100, 60000, value) && value == 5);
    assert(!RuntimeInteger("-1", 100, 60000, value)); assert(RuntimeInteger("2500", 100, 60000, value) && value == 2500);
    assert(ValidBackendUrl("http://127.0.0.1:3001") && !ValidBackendUrl("file:///tmp") && !ValidBackendUrl("http://host\r\nx:y"));
}

int main() {
    TestPackets(); TestIdentityAndQuery(); TestBackgroundHttp(); TestOperationIsolation(); TestDurableResultsAndPrivacy();
    std::cout << "Client safety: OK (truncated packets, bounds, identity, URL encoding, stalled/offline HTTP)\n";
}
