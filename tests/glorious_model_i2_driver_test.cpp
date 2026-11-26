// Exercise the Glorious Model I 2 Wireless driver with a simulated HID transport; no USB writes or hardware needed.
// Build:
// g++ -std=c++17 -O2 -Wall -Wextra tests/glorious_model_i2_driver_test.cpp -o build/glorious_model_i2_driver_test.exe -lhid -lsetupapi
#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>
#include <map>

static ULONGLONG fakeNow = 10000000;
ULONGLONG FakeTickCount64() { return fakeNow; }

#define GetTickCount64 FakeTickCount64
#include "../src/drivers/glorious_model_i2.cpp"
#undef GetTickCount64

namespace {
std::map<std::wstring, std::vector<std::vector<BYTE>>> reports;
std::map<std::wstring, int> failOpens;
int opens = 0;

HidColl Collection(USHORT pid, const wchar_t* path, USHORT page = 0xFF00, USHORT usage = 0xFF00) {
    HidColl c;
    c.path = path;
    c.vid = 0x093A;
    c.pid = pid;
    c.caps.UsagePage = page;
    c.caps.Usage = usage;
    c.caps.InputReportByteLength = 65;
    return c;
}
}  // namespace

// Stand-ins for the Win32 HID transport. The production driver is included unchanged.
bool HidDev::OpenReadOnly(const HidColl& c) {
    auto it = failOpens.find(c.path);
    if (it != failOpens.end() && it->second > 0) {
        --it->second;
        h_ = INVALID_HANDLE_VALUE;
        return false;
    }
    c_ = c;
    h_ = reinterpret_cast<HANDLE>(1);
    ++opens;
    return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
int HidDev::Read(std::vector<BYTE>& out, int timeoutMs) {
    assert(timeoutMs > 0 && timeoutMs <= 5000);
    auto& queue = reports[c_.path];
    if (queue.empty()) return -1;
    out = queue.front();
    queue.erase(queue.begin());
    return (int)out.size();
}
std::wstring Hex4(USHORT v) {
    wchar_t text[8];
    swprintf(text, 8, L"%04X", v);
    return text;
}
int Clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

int main() {
    auto receiver = Collection(0x821D, L"receiver");
    auto cable = Collection(0x821A, L"cable");

    // a. Receiver queue [06 00 00 00, 06 FB 63 00] -> one device, 99%, not charging, claimed.
    PollContext ctx;
    ctx.colls = { receiver };
    reports = { { L"receiver", { { 0x06, 0x00, 0x00, 0x00 }, { 0x06, 0xFB, 0x63, 0x00 } } } };
    opens = 0;
    PollGloriousModelI2(ctx);
    assert(opens == 1);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"glorious:821A");
    assert(ctx.out[0].name == L"Glorious Model I 2 Wireless");
    assert(ctx.out[0].percent == 99 && !ctx.out[0].charging && !ctx.out[0].asleep);
    assert(ctx.out[0].via == L"2.4 GHz receiver" && ctx.out[0].kind == DeviceKind::Mouse);
    assert(ctx.out[0].graceSec == 1800);
    assert(ctx.IsClaimed(0x093A, 0x821D));

    // b. Next poll, empty queue, same collection -> still 99 (remembered), same key, not asleep.
    ctx = {};
    ctx.colls = { receiver };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(opens == 1);  // persistent handle reused
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"glorious:821A");
    assert(ctx.out[0].percent == 99 && !ctx.out[0].charging && !ctx.out[0].asleep);
    assert(ctx.out[0].via == L"2.4 GHz receiver");

    // c. Fresh collection (new path) with empty queue -> no output, but 093A pid is claimed.
    ctx = {};
    ctx.colls = { Collection(0x821D, L"fresh-empty") };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(ctx.out.empty());
    assert(ctx.IsClaimed(0x093A, 0x821D));

    // d. Invalid 06 FB C8 00 (200) is ignored -> no output for a fresh collection.
    ctx = {};
    ctx.colls = { Collection(0x821D, L"fresh-invalid") };
    reports = { { L"fresh-invalid", { { 0x06, 0xFB, 0xC8, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(ctx.out.empty());
    assert(ctx.IsClaimed(0x093A, 0x821D));

    // e. Cable 821A (100%, charging) and receiver 821D (99%) in the same poll -> cable wins, both claimed.
    ctx = {};
    ctx.colls = { receiver, cable };
    reports = { { L"cable", { { 0x06, 0xFB, 0x64, 0x01 } } },
                { L"receiver", { { 0x06, 0xFB, 0x63, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"glorious:821A");
    assert(ctx.out[0].via == L"USB cable");
    assert(ctx.out[0].percent == 100 && ctx.out[0].charging);
    assert(ctx.IsClaimed(0x093A, 0x821A) && ctx.IsClaimed(0x093A, 0x821D));

    // f. Another usage (FF00:0001) or another PID (093A:EB01) is never opened.
    opens = 0;
    ctx = {};
    ctx.colls = { Collection(0x821D, L"wrong-usage", 0xFF00, 0x0001),
                  Collection(0xEB01, L"other-pid", 0xFF00, 0xFF00) };
    reports = { { L"wrong-usage", { { 0x06, 0xFB, 0x50, 0x00 } } },
                { L"other-pid", { { 0x06, 0xFB, 0x50, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(opens == 0 && ctx.out.empty() && ctx.claimed.empty());

    // g. Fresh cable collection (new path) with empty queue -> unknown level with note, claimed.
    ctx = {};
    ctx.colls = { Collection(0x821A, L"fresh-cable") };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"glorious:821A");
    assert(ctx.out[0].name == L"Glorious Model I 2 Wireless");
    assert(ctx.out[0].kind == DeviceKind::Mouse);
    assert(ctx.out[0].via == L"USB cable");
    assert(ctx.out[0].percent == -1 && !ctx.out[0].charging && ctx.out[0].connected);
    assert(ctx.out[0].graceSec == 0);
    assert(ctx.out[0].note == L"no battery report over the USB cable");
    assert(ctx.IsClaimed(0x093A, 0x821A));

    // h. Fresh cable (empty queue) and fresh receiver (06 FB 63 00) in one poll -> receiver wins, both claimed.
    ctx = {};
    ctx.colls = { Collection(0x821A, L"fresh-cable-2"), Collection(0x821D, L"fresh-receiver-2") };
    reports = { { L"fresh-receiver-2", { { 0x06, 0xFB, 0x63, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"glorious:821A");
    assert(ctx.out[0].via == L"2.4 GHz receiver");
    assert(ctx.out[0].percent == 99 && !ctx.out[0].charging && ctx.out[0].note.empty());
    assert(ctx.out[0].graceSec == 1800);
    assert(ctx.IsClaimed(0x093A, 0x821A) && ctx.IsClaimed(0x093A, 0x821D));

    // i. Receiver with a reading (99%), then +100 s with empty queue -> no reopen, still 99%, not asleep.
    opens = 0;
    ctx = {};
    ctx.colls = { receiver };
    reports = { { L"receiver", { { 0x06, 0xFB, 0x63, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(opens == 1);
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 99 && !ctx.out[0].asleep);

    fakeNow += 100000;
    ctx = {};
    ctx.colls = { receiver };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(opens == 1);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 99 && !ctx.out[0].charging && !ctx.out[0].asleep);

    // j. +60 s more (160 s silent), empty queue -> one reopen, still 99%, not asleep; immediate re-poll -> no reopen.
    fakeNow += 60000;
    ctx = {};
    ctx.colls = { receiver };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(opens == 2);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 99 && !ctx.out[0].charging && !ctx.out[0].asleep);

    ctx = {};
    ctx.colls = { receiver };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(opens == 2);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 99 && !ctx.out[0].charging && !ctx.out[0].asleep);

    // k. After the reopen, queue 06 FB 61 00 on the same path -> 97%, not asleep, no reopen.
    ctx = {};
    ctx.colls = { receiver };
    reports = { { L"receiver", { { 0x06, 0xFB, 0x61, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(opens == 2);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 97 && !ctx.out[0].charging && !ctx.out[0].asleep);

    // l. +210 s with no reports -> listed asleep with remembered percent (reopen does not hide it).
    fakeNow += 210000;
    ctx = {};
    ctx.colls = { receiver };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(opens == 3);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 97 && !ctx.out[0].charging && ctx.out[0].asleep);

    // m. Reopen fails once -> remembered reading kept in output, and the following poll opens again.
    fakeNow += 160000;
    failOpens[L"receiver"] = 1;
    ctx = {};
    ctx.colls = { receiver };
    reports.clear();
    PollGloriousModelI2(ctx);
    assert(failOpens[L"receiver"] == 0);
    assert(opens == 3);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 97 && !ctx.out[0].charging && ctx.out[0].asleep);

    ctx = {};
    ctx.colls = { receiver };
    reports = { { L"receiver", { { 0x06, 0xFB, 0x60, 0x00 } } } };
    PollGloriousModelI2(ctx);
    assert(opens == 4);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 96 && !ctx.out[0].charging && !ctx.out[0].asleep);

    std::puts("Glorious Model I 2 Wireless driver tests passed.");
}
