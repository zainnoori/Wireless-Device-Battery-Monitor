#pragma once
// Each driver looks at the enumerated HID collections, talks to the devices it recognises and
// appends what it finds. A driver claims a device (vendor/product id) once it has handled it so
// that the generic fallbacks further down the chain do not report it twice.
#include <set>
#include <utility>
#include <vector>

#include "../devices.h"
#include "../hid.h"

struct PollContext
{
    std::vector<HidColl> colls;
    std::set<std::pair<USHORT, USHORT>> claimed;
    std::set<std::wstring> btContainers; // container ids PollBluetooth already reported
    std::vector<DeviceStatus> out;
    // Device keys that already had battery readings in the history when the app started (null in tests and
    // with history recording off). A driver can trust such a device to be real even when it is silent now.
    const std::set<std::wstring>* seenBefore = nullptr;
    bool SeenBefore(const std::wstring &key) const { return seenBefore && seenBefore->count(key) != 0; }

    bool IsClaimed(USHORT vid, USHORT pid) const { return claimed.count({vid, pid}) != 0; }
    void Claim(USHORT vid, USHORT pid) { claimed.insert({vid, pid}); }
    void Add(const DeviceStatus &s, USHORT vid, USHORT pid)
    {
        out.push_back(s);
        if (!out.back().vid) out.back().vid = vid;
        Claim(vid, pid);
    }
    // A device key that stays the same when the receiver moves to another USB port (HID paths change
    // with the port, so they must not be part of a key): `base` for the first such device in this poll,
    // then `base#2`, `base#3` ... for more identical ones. Settings and battery history follow the key.
    std::wstring StableKey(const std::wstring &base) const
    {
        auto taken = [&](const std::wstring &k) {
            for (const auto &s : out)
                if (s.key == k && s.connected) return true;
            return false;
        };
        if (!taken(base)) return base;
        for (int n = 2;; ++n)
            if (!taken(base + L"#" + std::to_wstring(n))) return base + L"#" + std::to_wstring(n);
    }
    // Distinct (vid, pid) pairs for a vendor that are not claimed yet.
    std::vector<std::pair<USHORT, USHORT>> Products(USHORT vid) const
    {
        std::set<std::pair<USHORT, USHORT>> s;
        for (const auto &c : colls)
            if (c.vid == vid && !IsClaimed(c.vid, c.pid))
                s.insert({c.vid, c.pid});
        return {s.begin(), s.end()};
    }
    // Collections of one product, optionally filtered.
    template <class Pred>
    std::vector<const HidColl *> Find(USHORT vid, USHORT pid, Pred pred) const
    {
        std::vector<const HidColl *> r;
        for (const auto &c : colls)
            if (c.vid == vid && c.pid == pid && pred(c))
                r.push_back(&c);
        return r;
    }
};

// G-Wolves model names (shared by the Glorious- and Kysona-protocol drivers).
const wchar_t *GWolvesModelName(USHORT pid);
bool GWolvesIsCable(USHORT pid);

// Vendor drivers (run in this order).
void PollZebronics(PollContext& ctx);  // Zebronics / Compx 3554:FC0A mouse (Linux)
void PollSinowealthReceiver(PollContext& ctx);  // Redragon M602-KS receiver (258A:002F)
void PollDarkrim(PollContext& ctx);   // DARKRIM SHADOW receiver (3654:6901)
void PollGlorious(PollContext& ctx);  // also Hitscan
void PollGloriousModelI2(PollContext& ctx);  // Model I 2 on 093A:821A / 821D, passive status reports
void PollAula(PollContext& ctx);      // AULA F75 / F87 Pro / F99 / HERO 68 Air keyboards, SC580 mouse (before Kysona)
void PollEweadn(PollContext& ctx);    // EWEADN S7 / S9 mice (372E:103D receiver, 372E:1027 S9 Ultra cable)
void PollAulaMouse(PollContext& ctx); // AULA X9 Ultra on the shared AULA-8K receiver
void PollAulaHeadset(PollContext& ctx); // AULA A7Pro over its 2.4 GHz receiver
void PollDareu(PollContext& ctx);     // DAREU EM901X receiver / cable, A84 keyboard, A980 Pro, Sharkoon SGM50W (split feature/input collections)
void PollPiao(PollContext& ctx);      // IPI PIAO mouse (372E:1014 receiver / wired siblings), beiying protocol
void PollZenith(PollContext& ctx);    // Zenith Pro mouse (260D:1114 receiver, 1143 cable)
void PollDarmoshark(PollContext& ctx); // Darmoshark M3 over its 2.4 GHz receiver
void PollDarmosharkNrf(PollContext& ctx); // Darmoshark M3S Pro on the Nordic 1K receiver (1915:0723)
void PollAtkSerial(PollContext& ctx); // ATK N9 Pro / N9 Ultra headsets (virtual COM port)
void PollAtkMercury(PollContext& ctx); // ATK Mercury I / M1 headset (373B:107C receiver)
void PollKeychron(PollContext& ctx);  // Keychron mice (incl. Ultra-Link 8K receiver), Nape Pro trackball (Link-KM receiver), keyboards with battery firmware
void PollCherry(PollContext& ctx);    // CHERRY MX 3.0S Wireless (FF1C channel) and the Cherry XTRFY 1K mouse receiver (046A:0330, CompX protocol)
void PollNuphy(PollContext& ctx);     // NuPhy WH80 over USB cable (19F5:A011)
void PollWobkey(PollContext& ctx);    // Wobkey Rainy75 V2 keyboard (36B0:3071 receiver / 36B0:3075 cable)
void PollNuphyHalo96(PollContext& ctx); // Halo96 V2 with custom telemetry over the stock 2.4 GHz receiver
void PollCrociris(PollContext& ctx);  // CROCIRIS X7 series mice on the 36F9:AA20 "X7 Receiver 8K"
void PollArbiter(PollContext& ctx);   // Arbiter Studio Akitsu mice on 373E (before Lamzu, same vendor id)
void PollLamzu(PollContext& ctx);     // LAMZU mice on vendor ids 373E / 37B0
void PollEndgame(PollContext& ctx);   // Endgame Gear 4K mice (OP1w 4k, XM2w 4k) on the 4K dongle
void PollEndgameWe(PollContext& ctx);     // Endgame Gear XM2we on its original receiver (3367:1961)
void PollAtkKeyboard(PollContext& ctx);   // ATK V75X / VGN keyboards and ATK Z87
void PollRedragonEvision(PollContext& ctx);  // Redragon M999 on its Evision 2.4 GHz receiver (320F:226F)
void PollBarracudaPro(PollContext& ctx);  // Razer Barracuda Pro (1532:053A), before the generic Razer driver
void PollSteelSeriesElite(PollContext& ctx);  // SteelSeries Arctis Nova Elite base station (1038:2244)
void PollAudeze(PollContext& ctx);        // Audeze Maxwell (VID 3329): PC / Xbox / Maxwell 2 dongles or cable
void PollEndorfy(PollContext& ctx);       // ENDORFY Viro Wireless dongle (3738:0072), Airoha RACE battery query
void PollTurtleBeach(PollContext& ctx);   // Turtle Beach Stealth 700 Gen 3 (10F5:2251 / 2262 transmitters, 2253 cable)
void PollAkg(PollContext& ctx);           // AKG N9 Hybrid on its USB dongle (0ECB:20D9), Airoha RACE battery query
void PollNzxt(PollContext& ctx);          // NZXT Lift Elite Wireless (1E71:2101 / 2129)
void PollPlantronics(PollContext& ctx);   // Poly / Plantronics headsets behind the BT600 adapter (047F)
void PollVaxee(PollContext& ctx);     // VAXEE 4K receiver / NP-01S V2 cable, read-only battery commands
void PollWlmouse(PollContext& ctx);   // WLMouse Beast X family and Beast G (VID 36A7)
void PollFinalmouse(PollContext& ctx);  // Finalmouse UltralightX / Starlight X dongles (VID 361D), read-only GETs
void PollNinjutso(PollContext& ctx);  // Sora V2 (1915:AE1C 1K receiver, AE11 cable) and Ten / Ten Air (093A:EB01 / 093A:E020)
void PollSprime(PollContext& ctx);    // Sprime PM1 (1915:AC1C / AD8C receivers, AC11-AC13 cable)
void PollRawm(PollContext& ctx);      // RAWM mice (Leviathan V4, ES21, ...) and SADES Dagger (1915:23xx, RAWM HUB protocol)
void PollRongyuan(PollContext& ctx);  // Rongyuan receivers (Attack Shark, Ajazz keyboards, ...)
void PollVgnClient(PollContext& ctx);  // VGN / VXE mice and keyboards (2.4 GHz link only; runs after Kysona, AULA, ATK and Rongyuan, which claim parts of the same ids)

void PollSonyInzone(PollContext& ctx);  // Sony INZONE H9 II / H5 / Buds (HID), H9 / H7 (serial port)
void PollPlayStationLink(PollContext& ctx);  // PULSE Explore earbuds on the PlayStation Link adapter (054C:0ECC)
void PollSonix(PollContext& ctx);     // SONiX keyboards on their 2.4 GHz dongle (AJAZZ AK820 MAX, AK980 V2, AULA F108 Pro)
void PollViaDongle(PollContext& ctx);  // 36B0:3002 dongle (PMO Hub or VIA firmware) and PMO Hub keyboards on their cable (Wave 75, Aurora 80, P75, PMO 80)

void PollSemico(PollContext& ctx);    // Semico 1A2C keyboards (Attack Shark X98, AJAZZ AK820 Max Plus) on 8FFF / A034 / A036
void PollAjazzAk992(PollContext& ctx); // AJAZZ AK992 on the Telink 2.4 GHz receiver
void PollFractal(PollContext& ctx);   // Fractal Design Scape headset (dongle or cable)
void PollJabra(PollContext& ctx);     // Jabra GNP headsets, on USB or behind a Link dongle
void PollJblSpeaker(PollContext& ctx); // JBL Flip 7 over Bluetooth LE (PR #274; refreshed on its own thread)
void PollJbl(PollContext& ctx);       // JBL Quantum wireless headsets (HIDV1 and HIDV3)
void PollRog(PollContext& ctx);       // ASUS ROG keyboards, mice and the Pelta headset
void PollZowie(PollContext& ctx);     // BenQ ZOWIE EC-CW / DW / U2 mice
void PollRoyalKludge(PollContext& ctx);  // Royal Kludge MG6 Pro (VID 2E3C, QiWang protocol)
void PollMchoseG3(PollContext& ctx);  // MCHOSE G3 V2 wireless mouse (VID 3837)
void PollMsi(PollContext& ctx);       // MSI wireless mice: VERSA 300 ELITE, CLUTCH GM31 / GM41
void PollMchoseModern(PollContext& ctx);  // MCHOSE modern mice and K87 V3 keyboard (3837:1015)
void PollMchoseHeadset(PollContext& ctx);  // MCHOSE V9 Pro 2 headset (3837:6011, M HUB "MC" protocol)
void PollMchoseV9Pro(PollContext& ctx);  // MCHOSE V9 / V9 PRO / S9 PRO / G9 PRO headset dongle (291D:385D, "BLK" protocol)
void PollVgn(PollContext& ctx);        // VGN OMNISOUND (ZHOUTING) headset (372E:10BB, V HUB "ZHOU" protocol)
void PollMchoseKeyboard(PollContext& ctx);  // MCHOSE keyboards on the 3837:3033 receiver (GLW protocol)
void PollRapoo(PollContext& ctx);     // Rapoo VT gaming mice incl. VT1 Pro Max (passive battery reports)
void PollAreson(PollContext& ctx);    // Areson 25A7 receivers (Redragon M913 / M915)
void PollMchose(PollContext& ctx);    // MCHOSE mice (L7 / M7 / A7 / K7 families, VID 5253)
void PollMchoseA5(PollContext& ctx);  // MCHOSE A5 Pro / A5 Pro Max, first generation (VID 2023)
void PollMachenike(PollContext& ctx); // Machenike L7 Arctic / L7 Mix, receiver 248A:FF4C and cable FF4E
void PollNiceboy(PollContext& ctx);   // Niceboy MW400 on its 248A:FA02 receiver (before the other 248A pollers)
void PollMiscMice(PollContext& ctx);  // Motospeed X6, Delux M900 Pro / M800 Pro, Ajazz AJ179 and AJ159/AJ-series
void PollRazerHeadsets(PollContext& ctx);  // BlackShark V2 HyperSpeed / V2 Pro (2020/2023) / V3 variants, Barracuda X (2022)
void PollFantech(PollContext& ctx);   // Fantech Helios Go XD5 (wired 25A7:FA7B, dongle 25A7:FA7C)
void PollRaigor(PollContext& ctx);    // Fantech Raigor III / generic mice on the 1EA7:0066 dongle (FFB5)
std::wstring DumpXentaFrames();        // raw Xenta (1D57) status frames, for `--dump`
std::wstring DumpAstroFrames();        // raw ASTRO (9886) 0x54 / 0x7C exchanges, for `--dump`
void PollPulsar(PollContext& ctx);    // Pulsar X2 V3 / X2 V3 eS on 3710 (before Kysona, same vendor id)
void PollAtkBitmouse(PollContext& ctx);  // ATK / VXE NK NANO dongle 373B:1216, ATK 8K Nano dongle 373B:1178 (before Kysona, same vendor id)
void PollChimera(PollContext& ctx);      // Chimera V2 mouse (3554:F512, before Kysona, same vendor id)
void PollKysona(PollContext& ctx);    // also Pulsar, ATK / VXE, Hitscan Hyperlight
void PollHyperX(PollContext& ctx);
void PollLogitechG435(PollContext& ctx);  // G435 headset on the 046D:0ADF dongle (not HID++)
void PollAstro(PollContext& ctx);  // A50 X / Gen 5 vendor protocol (before generic Logitech)
void PollLogitech(PollContext& ctx);
void PollRazer(PollContext& ctx);
void PollSteelSeries(PollContext& ctx);
void PollCorsair(PollContext& ctx);
void PollCoolerMaster(PollContext& ctx);
void PollPlayStation(PollContext& ctx);
void PollSwitchPro(PollContext& ctx);  // Passive Switch Pro battery reports (057E:2009)
void PollSteamController(PollContext& ctx);
void PollGameSir(PollContext& ctx);    // Nova Lite / Nova 2 / Nova 2 Lite
void PollFlydigi(PollContext& ctx);    // Vader 5 Pro / Apex 5 vendor battery channel
void PollGameSirCyclone(PollContext& ctx);  // Cyclone 2 (XInput mode)
void PollGameSirSapjoy(PollContext& ctx);  // Tarantula Pro / NovaPro (Super Nova, T4NPro) on the SAPJoy 0xFFA0 frame
void PollGameSirBle(PollContext& ctx);  // Cyclone Pro on its dongle (3537:1015), battery over Bluetooth LE
void Poll8BitDo(PollContext& ctx);     // Ultimate 2 Wireless in D-input mode (2DC8:6012)
void PollBeitong(PollContext& ctx);    // BEITONG KP20 / KP40D on their 2.4 GHz receivers (20BC:5128, 20BC:515C, XInput mode)
void PollAirPods(PollContext& ctx);   // Apple AirPods / Beats (AAP L2CAP 1% via bridge driver, or BLE fallback)
void ShutdownAirPods();
std::wstring DumpAirPodsStatus();
void PollPhoneLink(PollContext& ctx);  // phone linked with Microsoft Phone Link (reads its Start menu card)
void PollGalaxyBuds(PollContext& ctx); // Samsung Galaxy Buds (Bluetooth RFCOMM, not HID)
void PollNothingEar(PollContext& ctx); // Nothing / CMF Ear earbuds (Bluetooth RFCOMM NT Link, not HID)
void PollSonyHeadphones(PollContext& ctx); // Sony WH / WF headphones and earbuds (Bluetooth RFCOMM, not HID)
void PollMarshall(PollContext& ctx);   // Marshall EMBERTON II speaker (unpaired LE Battery Service, address from Fast Pair stream)
void PollAkko(PollContext& ctx);       // Akko V9 Dash Pro / Dash V9 Ultra receivers (38EE:000C/000F)
void PollSoundcore(PollContext& ctx);  // Anker Soundcore Bluetooth headphones / earbuds (RFCOMM)
void PollMoondrop(PollContext& ctx);   // MOONDROP GM01 Pro headset (WalkPlay 2.4 GHz dongle)
void PollAlienware(PollContext& ctx);  // Alienware AW920H headset on its 2.4 GHz dongle (413C:A515)
void PollPressPlayIcarus(PollContext& ctx);  // ICARUS V2 Nearlink on its 7768:6434 receiver
void PollScyrox(PollContext& ctx);        // Scyrox V12M on its 2.4 GHz receiver (395A:0006), read-only profile read
// Fallbacks.
void PollHeadsetControl(PollContext &ctx); // only if headsetcontrol.exe is installed
std::wstring HidContainer(const std::wstring& path);  // container id of the physical device behind a HID path
void PollUnsupported(PollContext& ctx);  // last: wireless mice / keyboards no driver could read, flagged `unsupported`
void PollArmorX(PollContext& ctx);  // BigBigWon Armor X Pro: level over Bluetooth LE while on its 045E:0B12 dongle
void PollGenericHid(PollContext &ctx);
void PollBluetooth(PollContext &ctx);
void PollXInput(PollContext &ctx);
void PollMaono(PollContext &ctx);
