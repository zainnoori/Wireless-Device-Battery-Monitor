// Glorious Model I 2 Wireless mouse (passive battery status reports, read-only).
// Sources:
//  * Glorious CORE 2.1.23 (Electron app): device table `ModelI2Wireless` (USB 093A:821A,
//    receiver 093A:821D, Bluetooth 093A:821B; status read from collection FF00:FF00) and
//    `MouseV2DeviceHandler.handleDeviceMessage`, which treats input report
//    `06 FB <percent> <charging>` as the battery status (percent 0..100; charging when
//    byte 3 != 0). CORE sends no battery request for this mouse; it only listens.
//  * Owner's measurement on the receiver: the mouse pushes `06 FB 63 00` (99 %, not
//    charging) about every 65 s even when the level is unchanged, each followed by an
//    all-zero `06 00 ...` report that must be ignored. The first report after plugging in
//    the receiver arrived after ~37 s. GET_FEATURE and GET_REPORT(Input) are not answered,
//    so there is no read-only query; the handle stays open between polls (like PollRapoo)
//    and the last reading is remembered.
//  * Owner's test on the USB cable (093A:821A): no status report arrived (~7 min across every
//    openable collection, plus 15 s after replugging), so the cable is listed with an unknown
//    level; CORE's "charging" on the cable comes from the USB connection itself, not the mouse.
//  * Owner saw a long-lived receiver handle go silent after hours while a new one got reports; reopen after ~150 s.
#include "drivers.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace {

constexpr USHORT kVid = 0x093A;
constexpr USHORT kPidCable = 0x821A;
constexpr USHORT kPidReceiver = 0x821D;
constexpr ULONGLONG kReopenAfterMs = 150000;  // two missed ~65 s pushes
constexpr ULONGLONG kAsleepAfterMs = 200000;  // three missed ~65 s pushes

struct Slot {
    std::unique_ptr<HidDev> dev;
    bool have = false;
    BYTE percent = 0;
    bool charging = false;
    ULONGLONG at = 0;
    ULONGLONG heard = 0;
};

}  // namespace

// Keep open read-only handles between polls (like PollRapoo) so Windows buffers passive status
// reports arriving between refreshes, and remember the last reading.
static std::map<std::wstring, Slot> g_modelI2;

void PollGloriousModelI2(PollContext& ctx) {
    std::set<std::wstring> present;
    for (auto [vid, pid] : ctx.Products(kVid)) {
        if (pid != kPidCable && pid != kPidReceiver) continue;
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
                 return c.page() == 0xFF00 && c.usage() == 0xFF00;
             })) {
            present.insert(c->path);
            Slot& slot = g_modelI2[c->path];
            if (!slot.dev) {
                slot.dev = std::make_unique<HidDev>();
                if (!slot.dev->OpenReadOnly(*c)) {
                    slot.dev.reset();
                    if (!slot.have) {
                        g_modelI2.erase(c->path);
                        continue;
                    }
                } else {
                    slot.heard = GetTickCount64();
                }
            } else if (GetTickCount64() - slot.heard > kReopenAfterMs) {
                slot.dev->Close();
                if (slot.dev->OpenReadOnly(*c)) {
                    slot.heard = GetTickCount64();
                } else {
                    slot.dev.reset();
                }
            }
            if (slot.dev) {
                for (int i = 0; i < 64; i++) {
                    std::vector<BYTE> r;
                    if (slot.dev->Read(r, 30) < 0) break;
                    if (r.size() >= 4 && r[0] == 0x06 && r[1] == 0xFB && r[2] <= 100) {
                        slot.have = true;
                        slot.percent = r[2];
                        slot.charging = r[3] != 0;
                        slot.at = GetTickCount64();
                        slot.heard = slot.at;
                    }
                }
            }
            if (!slot.have && pid != kPidCable) {
                ctx.Claim(vid, pid);
                continue;
            }
            DeviceStatus s;
            s.key = L"glorious:821A";
            s.name = L"Glorious Model I 2 Wireless";
            s.kind = DeviceKind::Mouse;
            s.via = pid == kPidCable ? L"USB cable" : L"2.4 GHz receiver";
            if (slot.have) {
                s.percent = Clamp100(slot.percent);
                s.charging = slot.charging;
                s.graceSec = 1800;
                if (GetTickCount64() - slot.at > kAsleepAfterMs) s.asleep = true;
            } else {
                s.percent = -1;
                s.charging = false;
                s.connected = true;
                s.graceSec = 0;
                s.note = L"no battery report over the USB cable";
            }
            auto existing = std::find_if(ctx.out.begin(), ctx.out.end(), [](const DeviceStatus& other) {
                return other.key == L"glorious:821A";
            });
            if (existing != ctx.out.end()) {
                if (existing->percent < 0 && slot.have) {
                    s.vid = vid;
                    *existing = s;
                }
                ctx.Claim(vid, pid);
                if (slot.have) break;
                continue;
            }
            ctx.Add(s, vid, pid);
            if (slot.have) break;
        }
    }
    for (auto it = g_modelI2.begin(); it != g_modelI2.end();) {
        if (present.count(it->first)) {
            ++it;
        } else {
            if (it->second.dev) it->second.dev->Close();
            it = g_modelI2.erase(it);
        }
    }
}
