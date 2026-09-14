// The fixed set of Home Assistant entities, filled from whatever the configured meter profile reads.
//
// ESPHome entities exist from compile time, but the profile -- which registers, under which names,
// in which units -- is runtime data chosen in the setup wizard, and one image serves every variant.
// So the firmware declares a fixed list of well-known quantities (sensor.py) and this file maps the
// profile's values onto it on every 10 s tick; a quantity the profile lacks stays unavailable.
//
// The names come verbatim from the Tasmota scripts the presets were generated from, which do not
// agree with each other: voltage is V1..V3 on gPlugD/K but U1..U3 on gPlugM, per-phase power is
// P1i/P1o on gPlugD/K but Pi1/Po1 on gPlugM, and total power is kW while per-phase power is W. The
// aliases and unit rules below absorb that, and match what the live ring already does
// (GplugSmi::value_w_ and the per-phase sum in GplugSmi::loop).
//
// Header-only and free of ESPHome/IDF, so test/test_ha.cpp checks every preset family on the host.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace gplug_ha {

// Order is the contract with sensor.py's SENSORS list, which passes these indices to
// GplugSmi::set_ha_sensor(). Append only, and keep both in step.
enum HaKey : uint8_t {
  HA_POWER,           // net, import positive, W
  HA_POWER_IMPORT,    // W
  HA_POWER_EXPORT,    // W
  HA_ENERGY_IMPORT,   // kWh
  HA_ENERGY_EXPORT,
  HA_ENERGY_IMPORT_T1,
  HA_ENERGY_IMPORT_T2,
  HA_ENERGY_EXPORT_T1,
  HA_ENERGY_EXPORT_T2,
  HA_VOLTAGE_L1,      // V
  HA_VOLTAGE_L2,
  HA_VOLTAGE_L3,
  HA_CURRENT_L1,      // A
  HA_CURRENT_L2,
  HA_CURRENT_L3,
  HA_POWER_L1,        // net per phase, W
  HA_POWER_L2,
  HA_POWER_L3,
  HA_COUNT
};

struct HaSnapshot {
  float v[HA_COUNT];
  bool have[HA_COUNT];
};

// One view of the decoder's live state: parallel arrays, as GplugSmi keeps them (desc_.obis[i]
// names and units, values_[i], have_[i]). String registers (the meter ID) must come in with
// have=false or be left out; this only reads numbers.
struct HaInput {
  const char *const *names;
  const char *const *units;
  const float *values;
  const bool *have;
  uint8_t n;
  // Full-precision counters in Wh (GplugSmi::ei_wh_exact_ / eo_wh_exact_), negative when unknown.
  // A float kWh register past ~10 MWh has ~10 Wh steps; the exact value is preferred when present.
  double ei_wh_exact;
  double eo_wh_exact;
};

inline int ha_find_(const HaInput &in, const char *name) {
  for (uint8_t i = 0; i < in.n; i++)
    if (in.have[i] && strcmp(in.names[i], name) == 0) return i;
  return -1;
}

// Value in W (kW registers scaled), or false when absent.
inline bool ha_watts_(const HaInput &in, const char *name, float &out) {
  int i = ha_find_(in, name);
  if (i < 0) return false;
  out = in.values[i];
  if (strcmp(in.units[i], "kW") == 0) out *= 1000.0f;
  return true;
}

// Value in kWh (Wh registers scaled), or false when absent.
inline bool ha_kwh_(const HaInput &in, const char *name, float &out) {
  int i = ha_find_(in, name);
  if (i < 0) return false;
  out = in.values[i];
  if (strcmp(in.units[i], "Wh") == 0) out /= 1000.0f;
  return true;
}

// First present name among the aliases, unit as the register has it.
inline bool ha_plain_(const HaInput &in, const char *a, const char *b, float &out) {
  int i = ha_find_(in, a);
  if (i < 0 && b) i = ha_find_(in, b);
  if (i < 0) return false;
  out = in.values[i];
  return true;
}

inline void ha_snapshot(const HaInput &in, HaSnapshot &out) {
  for (int k = 0; k < HA_COUNT; k++) { out.v[k] = NAN; out.have[k] = false; }
  auto set = [&out](HaKey k, float v) { out.v[k] = v; out.have[k] = true; };
  float v;

  float pi = 0, po = 0;
  bool has_pi = ha_watts_(in, "Pi", pi), has_po = ha_watts_(in, "Po", po);
  if (has_pi) set(HA_POWER_IMPORT, pi);
  if (has_po) set(HA_POWER_EXPORT, po);
  if (has_pi || has_po) set(HA_POWER, pi - po);

  if (in.ei_wh_exact >= 0) set(HA_ENERGY_IMPORT, (float) (in.ei_wh_exact / 1000.0));
  else if (ha_kwh_(in, "Ei", v)) set(HA_ENERGY_IMPORT, v);
  if (in.eo_wh_exact >= 0) set(HA_ENERGY_EXPORT, (float) (in.eo_wh_exact / 1000.0));
  else if (ha_kwh_(in, "Eo", v)) set(HA_ENERGY_EXPORT, v);
  if (ha_kwh_(in, "Ei1", v)) set(HA_ENERGY_IMPORT_T1, v);
  if (ha_kwh_(in, "Ei2", v)) set(HA_ENERGY_IMPORT_T2, v);
  if (ha_kwh_(in, "Eo1", v)) set(HA_ENERGY_EXPORT_T1, v);
  if (ha_kwh_(in, "Eo2", v)) set(HA_ENERGY_EXPORT_T2, v);

  static const char *const volt[3][2] = {{"V1", "U1"}, {"V2", "U2"}, {"V3", "U3"}};
  static const char *const amp[3] = {"I1", "I2", "I3"};
  // Import and export per phase, in both spellings; a phase counts as present when any one is.
  static const char *const p_in[3][2] = {{"P1i", "Pi1"}, {"P2i", "Pi2"}, {"P3i", "Pi3"}};
  static const char *const p_out[3][2] = {{"P1o", "Po1"}, {"P2o", "Po2"}, {"P3o", "Po3"}};
  for (int ph = 0; ph < 3; ph++) {
    if (ha_plain_(in, volt[ph][0], volt[ph][1], v)) set((HaKey) (HA_VOLTAGE_L1 + ph), v);
    if (ha_plain_(in, amp[ph], nullptr, v)) set((HaKey) (HA_CURRENT_L1 + ph), v);
    float sum = 0;
    bool any = false;
    for (int s = 0; s < 2; s++) {
      if (ha_watts_(in, p_in[ph][s], v)) { sum += v; any = true; }
      if (ha_watts_(in, p_out[ph][s], v)) { sum -= v; any = true; }
    }
    if (any) set((HaKey) (HA_POWER_L1 + ph), sum);
  }
}

}  // namespace gplug_ha
