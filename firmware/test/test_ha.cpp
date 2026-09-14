// ha_values.h: the profile -> Home Assistant entity mapping, once per preset family, with the names
// and units exactly as presets.json has them. The families disagree on names (V1 vs U1, P1i vs Pi1)
// and units (kW totals, W per phase); every alias is pinned here so a renamed preset or a new one
// shows up as a failing case instead of an entity that silently stays unavailable.
#include "ha_values.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
using namespace gplug_ha;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECK_NEAR(a, b) do { double _a = (a), _b = (b); if (!(std::fabs(_a - _b) < 1e-3 * (1 + std::fabs(_b)))) { printf("FAIL %s:%d got %g want %g\n", __FILE__, __LINE__, _a, _b); fails++; } } while (0)

struct Reg { const char *name; const char *unit; float value; bool have = true; };

// Builds the parallel arrays GplugSmi passes in and runs the mapping.
static HaSnapshot run(const std::vector<Reg> &regs, double ei_exact = -1, double eo_exact = -1) {
  static std::vector<const char *> names, units;
  static std::vector<float> values;
  static bool have[64];
  names.clear(); units.clear(); values.clear();
  for (size_t i = 0; i < regs.size(); i++) {
    names.push_back(regs[i].name); units.push_back(regs[i].unit); values.push_back(regs[i].value);
    have[i] = regs[i].have;
  }
  HaInput in{names.data(), units.data(), values.data(), have, (uint8_t) regs.size(), ei_exact, eo_exact};
  HaSnapshot s;
  ha_snapshot(in, s);
  return s;
}

static void only(const HaSnapshot &s, std::initializer_list<HaKey> present, int line) {
  bool want[HA_COUNT] = {};
  for (HaKey k : present) want[k] = true;
  for (int k = 0; k < HA_COUNT; k++)
    if (s.have[k] != want[k]) { printf("FAIL %s:%d key %d have=%d want=%d\n", __FILE__, line, k, s.have[k], want[k]); fails++; }
}

int main() {
  // The contract with sensor.py: 18 numeric entities, in this order.
  CHECK(HA_COUNT == 18);
  CHECK(HA_POWER_L3 == 17);

  {  // gplugd/p1-dsmr (and gplugde/p1-dsmr): kW totals, per-phase W as P1i/P1o, V1, tariffs
    HaSnapshot s = run({{"SMid", "", 0, false}, {"Pi", "kW", 1.234f}, {"Po", "kW", 0.0f},
                        {"P1i", "W", 800}, {"P2i", "W", 300}, {"P3i", "W", 134}, {"P1o", "W", 0}, {"P2o", "W", 0}, {"P3o", "W", 0},
                        {"V1", "V", 231.4f}, {"V2", "V", 230.1f}, {"V3", "V", 229.6f}, {"I1", "A", 3}, {"I2", "A", 1}, {"I3", "A", 1},
                        {"Ei", "kWh", 19087.213f}, {"Ei1", "kWh", 12000.1f}, {"Ei2", "kWh", 7087.1f},
                        {"Eo", "kWh", 30836.881f}, {"Eo1", "kWh", 20000.5f}, {"Eo2", "kWh", 10836.3f}});
    only(s, {HA_POWER, HA_POWER_IMPORT, HA_POWER_EXPORT, HA_ENERGY_IMPORT, HA_ENERGY_EXPORT, HA_ENERGY_IMPORT_T1,
             HA_ENERGY_IMPORT_T2, HA_ENERGY_EXPORT_T1, HA_ENERGY_EXPORT_T2, HA_VOLTAGE_L1, HA_VOLTAGE_L2, HA_VOLTAGE_L3,
             HA_CURRENT_L1, HA_CURRENT_L2, HA_CURRENT_L3, HA_POWER_L1, HA_POWER_L2, HA_POWER_L3}, __LINE__);
    CHECK_NEAR(s.v[HA_POWER], 1234);
    CHECK_NEAR(s.v[HA_POWER_IMPORT], 1234);
    CHECK_NEAR(s.v[HA_POWER_EXPORT], 0);
    CHECK_NEAR(s.v[HA_POWER_L1], 800);
    CHECK_NEAR(s.v[HA_VOLTAGE_L2], 230.1);
    CHECK_NEAR(s.v[HA_ENERGY_IMPORT], 19087.213);
    CHECK_NEAR(s.v[HA_ENERGY_EXPORT_T2], 10836.3);
  }

  {  // gplugd/p1-hdlc_dlms: no per-phase power at all -> those stay unavailable
    HaSnapshot s = run({{"SMid", "", 0, false}, {"Pi", "kW", 0.5f}, {"Po", "kW", 0.1f}, {"V1", "V", 230}, {"V2", "V", 231}, {"V3", "V", 232},
                        {"I1", "A", 1}, {"I2", "A", 2}, {"I3", "A", 3}, {"Ei", "kWh", 1}, {"Ei1", "kWh", 1}, {"Ei2", "kWh", 0},
                        {"Eo", "kWh", 2}, {"Eo1", "kWh", 1}, {"Eo2", "kWh", 1}});
    CHECK(!s.have[HA_POWER_L1] && !s.have[HA_POWER_L2] && !s.have[HA_POWER_L3]);
    CHECK(std::isnan(s.v[HA_POWER_L1]));
    CHECK_NEAR(s.v[HA_POWER], 400);
  }

  {  // gplugk/dlms-push-1: reactive values and pf1 are not mapped, no tariff counters; exact Wh preferred
    HaSnapshot s = run({{"SMid", "", 32942200}, {"Pi", "kW", 0.719f}, {"Po", "kW", 0}, {"P1i", "W", 320}, {"P2i", "W", 220}, {"P3i", "W", 180},
                        {"P1o", "W", 0}, {"P2o", "W", 0}, {"P3o", "W", 0}, {"V1", "V", 238}, {"V2", "V", 238}, {"V3", "V", 238},
                        {"I1", "A", 2.3f}, {"I2", "A", 1.4f}, {"I3", "A", 1.5f}, {"rPi", "VAr", 100}, {"rPo", "VAr", 0}, {"pf1", "", 0.95f},
                        {"Ei", "kWh", 56021.53f}, {"Eo", "kWh", 42389.6f}, {"rEi", "kVArh", 5}, {"rEo", "kVArh", 6}},
                       56021535.0, 42389598.0);
    only(s, {HA_POWER, HA_POWER_IMPORT, HA_POWER_EXPORT, HA_ENERGY_IMPORT, HA_ENERGY_EXPORT, HA_VOLTAGE_L1, HA_VOLTAGE_L2,
             HA_VOLTAGE_L3, HA_CURRENT_L1, HA_CURRENT_L2, HA_CURRENT_L3, HA_POWER_L1, HA_POWER_L2, HA_POWER_L3}, __LINE__);
    CHECK_NEAR(s.v[HA_ENERGY_IMPORT], 56021.535);
    CHECK_NEAR(s.v[HA_ENERGY_EXPORT], 42389.598);
    CHECK_NEAR(s.v[HA_CURRENT_L1], 2.3);
    CHECK_NEAR(s.v[HA_POWER], 719);
  }

  {  // gplugm/romande-energie: U1..U3 for voltage, no per-phase power
    HaSnapshot s = run({{"SMid", "", 0, false}, {"Pi", "kW", 0}, {"Po", "kW", 2.5f}, {"I1", "A", 1}, {"I2", "A", 1}, {"I3", "A", 1},
                        {"U1", "V", 229.9f}, {"U2", "V", 230}, {"U3", "V", 231}, {"Ei", "kWh", 10}, {"Eo", "kWh", 20},
                        {"Ei1", "kWh", 5}, {"Ei2", "kWh", 5}, {"Eo1", "kWh", 10}, {"Eo2", "kWh", 10}});
    CHECK(s.have[HA_VOLTAGE_L1] && s.have[HA_VOLTAGE_L3]);
    CHECK_NEAR(s.v[HA_VOLTAGE_L1], 229.9);
    CHECK(!s.have[HA_POWER_L2]);
    CHECK_NEAR(s.v[HA_POWER], -2500);   // feeding in: net power is negative
    CHECK_NEAR(s.v[HA_POWER_EXPORT], 2500);
  }

  {  // gplugm/universal: Pi1/Po1 spelling, U1, Q5..Q82 ignored
    HaSnapshot s = run({{"SMid", "", 0, false}, {"Pi", "kW", 1}, {"Po", "kW", 0}, {"Pi1", "W", 500}, {"Pi2", "W", 0}, {"Pi3", "W", 0},
                        {"Po1", "W", 0}, {"Po2", "W", 200}, {"Po3", "W", 0}, {"U1", "V", 230}, {"U2", "V", 230}, {"U3", "V", 230},
                        {"I1", "A", 2}, {"I2", "A", 1}, {"I3", "A", 0}, {"Ei", "kWh", 1}, {"Eo", "kWh", 1}, {"Ei1", "kWh", 1},
                        {"Ei2", "kWh", 1}, {"Eo1", "kWh", 1}, {"Eo2", "kWh", 1}, {"Q5", "kVArh", 1}, {"Q82", "kVArh", 1}});
    CHECK_NEAR(s.v[HA_POWER_L1], 500);
    CHECK_NEAR(s.v[HA_POWER_L2], -200);
    CHECK(s.have[HA_POWER_L3]);
    CHECK_NEAR(s.v[HA_POWER_L3], 0);
  }

  {  // registers configured but not received yet (have=false) count as absent
    HaSnapshot s = run({{"Pi", "kW", 9, false}, {"Po", "kW", 9, false}, {"Ei", "kWh", 9, false}, {"V1", "V", 9, false}});
    only(s, {}, __LINE__);
  }

  {  // only one direction present still gives net power; Wh counters and kW phase power scale
    HaSnapshot s = run({{"Pi", "kW", 0.25f}, {"Ei", "Wh", 12345}, {"P1i", "kW", 0.1f}});
    CHECK_NEAR(s.v[HA_POWER], 250);
    CHECK(!s.have[HA_POWER_EXPORT]);
    CHECK_NEAR(s.v[HA_ENERGY_IMPORT], 12.345);
    CHECK_NEAR(s.v[HA_POWER_L1], 100);
  }

  {  // an exact counter wins over the float register, per direction
    HaSnapshot s = run({{"Ei", "kWh", 1.0f}, {"Eo", "kWh", 2.0f}}, 1500.0, -1);
    CHECK_NEAR(s.v[HA_ENERGY_IMPORT], 1.5);
    CHECK_NEAR(s.v[HA_ENERGY_EXPORT], 2.0);
  }

  if (fails) { printf("%d FAILED\n", fails); return 1; }
  printf("all ok\n");
  return 0;
}
