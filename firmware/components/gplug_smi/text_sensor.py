"""Home Assistant text sensors for gplug_smi: the setup diagnosis and the meter's own ID."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_COUNTER

from . import CONF_GPLUG_SMI_ID, GplugSmi

DEPENDENCIES = ["gplug_smi"]

# The same verdict the app's diagnosis card and the LED use: ok, waiting, key, no_match, protocol,
# garbled, silent, unconfigured (firmware/README.md, "Setup diagnosis").
CONF_METER_STATUS = "meter_status"
CONF_METER_ID = "meter_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GPLUG_SMI_ID): cv.use_id(GplugSmi),
        cv.Optional(CONF_METER_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:meter-electric-outline", entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
        cv.Optional(CONF_METER_ID): text_sensor.text_sensor_schema(
            icon=ICON_COUNTER, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GPLUG_SMI_ID])
    if CONF_METER_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_METER_STATUS])
        cg.add(parent.set_meter_status_text(sens))
    if CONF_METER_ID in config:
        sens = await text_sensor.new_text_sensor(config[CONF_METER_ID])
        cg.add(parent.set_meter_id_text(sens))
