// Runs every driver in order. Vendor drivers go first and claim the devices they handle; the
// generic fallbacks then only look at what is left.
#include "devices.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>

#include "battery_history.h"
#include "drivers/drivers.h"
#include "report.h"
#include "hid.h"

namespace {

// HKCU\Software\WirelessBattery\IgnoreDevices (REG_SZ): "VID:PID" pairs in hex, separated by spaces,
// commas or semicolons (e.g. "28BD:0947, 046D:C547"). No driver ever opens those devices (issue #232).
std::vector<std::pair<USHORT, USHORT>> IgnoredDevices() {
    std::vector<std::pair<USHORT, USHORT>> out;
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\WirelessBattery", L"IgnoreDevices", RRF_RT_REG_SZ, nullptr, buf, &size) !=
        ERROR_SUCCESS)
        return out;
    for (const wchar_t* p = buf; *p;) {
        unsigned vid = 0, pid = 0;
        int used = 0;
        if (swscanf(p, L" %x:%x%n", &vid, &pid, &used) == 2 && used > 0) {
            out.push_back({ (USHORT)vid, (USHORT)pid });
            p += used;
        } else {
            p++;
        }
        while (*p == L',' || *p == L';' || *p == L' ') p++;
    }
    return out;
}

}  // namespace

std::vector<DeviceStatus> PollAllDevices() {
    PollContext ctx;
    ctx.seenBefore = &battery_history::KeysAtStartup();
    ctx.colls = EnumHid();
    auto ignored = IgnoredDevices();
    if (!ignored.empty())
        ctx.colls.erase(std::remove_if(ctx.colls.begin(), ctx.colls.end(), [&](const HidColl& c) {
                            return std::find(ignored.begin(), ignored.end(), std::make_pair(c.vid, c.pid)) != ignored.end();
                        }),
                        ctx.colls.end());
    PollSinowealthReceiver(ctx);
    PollGlorious(ctx);
    PollGloriousModelI2(ctx);
    PollAula(ctx);       // before Kysona (shares the 3554 vendor id) and SONiX (shares 05AC:024F)
    PollAulaMouse(ctx);
    PollAulaHeadset(ctx);
    PollEweadn(ctx);     // BeiYing mice: EWEADN S7 / S9 on 372E:103D / 1027, RK M3 on 1019 / 102A (AULA keeps 106C)
    PollDareu(ctx);
    PollPiao(ctx);       // IPI PIAO mouse on 372E:1014 receiver (and wired 1015/1028/1056)
    PollZenith(ctx);
    PollDarmoshark(ctx);
    PollDarmosharkNrf(ctx);
    PollAtkSerial(ctx);  // before Kysona, which shares the ATK vendor id
    PollAtkMercury(ctx); // before Kysona, which must not send mouse queries to the Mercury receiver
    PollAtkKeyboard(ctx);
    PollRedragonEvision(ctx);
    PollPulsar(ctx);     // before Kysona, which also scans the 3710 vendor id
    PollAtkBitmouse(ctx);  // before Kysona, which would probe the NK dongle with the wrong protocol
    PollChimera(ctx);      // Chimera V2 mouse (3554:F512), before Kysona which shares the 3554 vendor id
    PollKysona(ctx);
    PollKeychron(ctx);
    PollCherry(ctx);
    PollNuphy(ctx);
    PollWobkey(ctx);
    PollNuphyHalo96(ctx);
    PollCrociris(ctx);
    PollArbiter(ctx);    // before Lamzu, which shares the 373E vendor id
    PollLamzu(ctx);
    PollEndgame(ctx);
    PollEndgameWe(ctx);
    PollVaxee(ctx);
    PollWlmouse(ctx);
    PollFinalmouse(ctx);
    PollScyrox(ctx);
    PollNinjutso(ctx);
    PollSprime(ctx);
    PollRawm(ctx);
    PollAkko(ctx);
    PollRongyuan(ctx);
    PollVgnClient(ctx);  // VGN: after the drivers that already claim VGN ids (320F ATK keyboards, 3554 Kysona/AULA, 3151 Rongyuan)
    PollSonix(ctx);
    PollViaDongle(ctx);
    PollSemico(ctx);
    PollAjazzAk992(ctx);
    PollSonyInzone(ctx);
    PollPlayStationLink(ctx);
    PollFractal(ctx);
    PollJabra(ctx);
    PollJbl(ctx);
    PollRog(ctx);
    PollZowie(ctx);
    PollRoyalKludge(ctx);
    PollMchose(ctx);
    PollMchoseA5(ctx);
    PollRapoo(ctx);
    PollAreson(ctx);
    PollMchoseG3(ctx);
    PollMsi(ctx);
    PollMachenike(ctx);  // before Ajazz in PollMiscMice, which shares the L7's 248A vendor id
    PollNiceboy(ctx);    // exact 248A:FA02 match, also before the Ajazz / Delux pollers
    PollMchoseModern(ctx);
    PollMchoseHeadset(ctx);
    PollMchoseV9Pro(ctx);
    PollVgn(ctx);
    PollMchoseKeyboard(ctx);  // 3837:3033 receiver, before the generic fallbacks
    PollMiscMice(ctx);
    PollFantech(ctx);
    PollRaigor(ctx);
    PollRazerHeadsets(ctx);  // before the generic Razer driver, which shares the vendor id
    PollBarracudaPro(ctx);   // before the generic Razer driver as well
    PollHyperX(ctx);
    PollLogitechG435(ctx);  // before the HID++ driver, which shares the vendor id
    PollAstro(ctx);
    PollLogitech(ctx);
    PollRazer(ctx);
    PollSteelSeriesElite(ctx);
    PollSteelSeries(ctx);
    PollAudeze(ctx);
    PollEndorfy(ctx);
    PollTurtleBeach(ctx);
    PollAkg(ctx);
    PollNzxt(ctx);
    PollPlantronics(ctx);
    PollCorsair(ctx);
    PollCoolerMaster(ctx);
    PollPlayStation(ctx);
    PollSwitchPro(ctx);
    PollSteamController(ctx);
    PollGameSir(ctx);
    PollFlydigi(ctx);
    PollGameSirCyclone(ctx);
    PollGameSirSapjoy(ctx);  // Tarantula Pro / NovaPro: SAPJoy device info on FF A0 (Nova and Cyclone 2 above keep their ids)
    PollGameSirBle(ctx);  // Cyclone Pro: battery over BLE; claims its dongle before the XInput fallback
    Poll8BitDo(ctx);
    PollBeitong(ctx);  // before the XInput fallback, which only sees the receiver as wired
    PollAirPods(ctx);  // before the Bluetooth fallback, which has no level for AirPods
    PollPhoneLink(ctx);
    PollGalaxyBuds(ctx);  // before the Bluetooth fallback, which skips devices already listed by name
    PollNothingEar(ctx);  // before the Bluetooth fallback, which skips devices already listed by name
    PollSonyHeadphones(ctx);  // before the Bluetooth fallback, which skips devices already listed by name
    PollMarshall(ctx);    // EMBERTON II: Windows has no level for it; before the Bluetooth fallback as well
    PollSoundcore(ctx);   // before the Bluetooth fallback: works even with Hands-Free AG disabled
    PollMoondrop(ctx);
    PollDarkrim(ctx);
    PollAlienware(ctx);
    PollMaono(ctx);  // MAONO PD200W receiver (352F:040E), experimental battery query
    PollPressPlayIcarus(ctx);
    PollHeadsetControl(ctx);
    PollJblSpeaker(ctx);  // before the Bluetooth fallback, which skips devices already listed by name
    PollBluetooth(ctx);   // before the generic HID check: Windows' own level wins for the same device
    PollGenericHid(ctx);
    PollArmorX(ctx);  // claims 045E:0B12 so the Xbox fallback skips the dongle's fake "Full" (PR #145)
    PollXInput(ctx);
    PollUnsupported(ctx);  // menu only: "No battery info"
    return ctx.out;
}

std::wstring DumpHidCollections() {
    std::wstring out = L"VID:PID    IF  UsagePg:Usage  In  Out Feat  Product\n";
    for (const auto& c : EnumHid()) {
        wchar_t line[160];
        swprintf(line, 160, L"%04X:%04X  %2d  %04X:%04X    %3u %3u %3u   ", c.vid, c.pid, c.iface, c.page(), c.usage(), c.inLen(), c.outLen(),
                 c.featLen());
        out += line + HidProduct(c) + L"\n";
    }
    out += report::DumpExtras();  // unopenable HID collections, non-HID USB devices, Bluetooth devices
    out += DumpXentaFrames();
    out += DumpAstroFrames();
    out += DumpAirPodsStatus();
    return out;
}

void ShutdownDevices() {
    ShutdownAirPods();
}
