"""Home Assistant sensors for gplug_smi.

A fixed list of well-known quantities, filled at runtime from whatever the configured meter profile
reads (ha_values.h); a quantity the profile lacks stays unknown in Home Assistant. Every key is
optional, so an adopting config can drop or rename entities.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import CONF_GPLUG_SMI_ID, GplugSmi

DEPENDENCIES = ["gplug_smi"]

CONF_FRAME_AGE = "frame_age"
CONF_FREE_HEAP = "free_heap"
CONF_LARGEST_BLOCK = "largest_block"


def _heap_kb():
    # Sampled every 5 min with the heap trend (heap_monitor.h); kB, like /api/heap.
    return sensor.sensor_schema(unit_of_measurement="kB", accuracy_decimals=0, icon="mdi:memory",
                                state_class=STATE_CLASS_MEASUREMENT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC)


def _power():
    return sensor.sensor_schema(unit_of_measurement=UNIT_WATT, accuracy_decimals=0,
                                device_class=DEVICE_CLASS_POWER, state_class=STATE_CLASS_MEASUREMENT)


def _energy():
    return sensor.sensor_schema(unit_of_measurement=UNIT_KILOWATT_HOURS, accuracy_decimals=3,
                                device_class=DEVICE_CLASS_ENERGY, state_class=STATE_CLASS_TOTAL_INCREASING)


def _voltage():
    return sensor.sensor_schema(unit_of_measurement=UNIT_VOLT, accuracy_decimals=1,
                                device_class=DEVICE_CLASS_VOLTAGE, state_class=STATE_CLASS_MEASUREMENT)


def _current():
    return sensor.sensor_schema(unit_of_measurement=UNIT_AMPERE, accuracy_decimals=2,
                                device_class=DEVICE_CLASS_CURRENT, state_class=STATE_CLASS_MEASUREMENT)


# In gplug_ha::HaKey order: the position is the index passed to GplugSmi::set_ha_sensor().
# Append only, together with ha_values.h (test/test_ha.cpp pins HA_COUNT).
SENSORS = [
    ("power", _power),
    ("power_import", _power),
    ("power_export", _power),
    ("energy_import", _energy),
    ("energy_export", _energy),
    ("energy_import_t1", _energy),
    ("energy_import_t2", _energy),
    ("energy_export_t1", _energy),
    ("energy_export_t2", _energy),
    ("voltage_l1", _voltage),
    ("voltage_l2", _voltage),
    ("voltage_l3", _voltage),
    ("current_l1", _current),
    ("current_l2", _current),
    ("current_l3", _current),
    ("power_l1", _power),
    ("power_l2", _power),
    ("power_l3", _power),
]
assert len(SENSORS) == 18, "keep SENSORS in step with gplug_ha::HA_COUNT"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GPLUG_SMI_ID): cv.use_id(GplugSmi),
        **{cv.Optional(key): schema() for key, schema in SENSORS},
        cv.Optional(CONF_FRAME_AGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND, accuracy_decimals=0, device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
        cv.Optional(CONF_FREE_HEAP): _heap_kb(),
        cv.Optional(CONF_LARGEST_BLOCK): _heap_kb(),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GPLUG_SMI_ID])
    for index, (key, _) in enumerate(SENSORS):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(parent.set_ha_sensor(index, sens))
    if CONF_FRAME_AGE in config:
        sens = await sensor.new_sensor(config[CONF_FRAME_AGE])
        cg.add(parent.set_frame_age_sensor(sens))
    if CONF_FREE_HEAP in config:
        sens = await sensor.new_sensor(config[CONF_FREE_HEAP])
        cg.add(parent.set_free_heap_sensor(sens))
    if CONF_LARGEST_BLOCK in config:
        sens = await sensor.new_sensor(config[CONF_LARGEST_BLOCK])
        cg.add(parent.set_largest_block_sensor(sens))
