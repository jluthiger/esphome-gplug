"""gplug_smi – gPlug smart-meter interface for ESPHome.

Embeds the SPA (gzipped) and the preset list, wires UART + web server, and
exposes /api/* endpoints. Meter-specific behaviour is runtime data (descriptor).
"""
import gzip
import json
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32, uart, web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID
from esphome.core import CORE

AUTO_LOAD = ["web_server_base", "json"]
DEPENDENCIES = ["uart", "wifi", "esp32", "captive_portal"]
CODEOWNERS = ["@gplug"]

CONF_SPA = "spa"
CONF_PRESETS = "presets"
CONF_SPA_RAW_ID = "spa_raw_id"
CONF_PRESETS_RAW_ID = "presets_raw_id"

gplug_ns = cg.esphome_ns.namespace("gplug_smi")
GplugSmi = gplug_ns.class_("GplugSmi", cg.Component, uart.UARTDevice)


def _file(value):
    value = cv.string(value)
    path = Path(CORE.relative_config_path(value))
    if not path.is_file():
        raise cv.Invalid(f"File not found: {path}")
    return str(path)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GplugSmi),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
            cv.GenerateID(CONF_SPA_RAW_ID): cv.declare_id(cg.uint8),
            cv.GenerateID(CONF_PRESETS_RAW_ID): cv.declare_id(cg.uint8),
            cv.Required(CONF_SPA): _file,
            cv.Required(CONF_PRESETS): _file,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def _gz_bytes(path: str) -> bytes:
    data = Path(path).read_bytes()
    if path.endswith(".gz"):
        return data
    return gzip.compress(data, compresslevel=9, mtime=0)


async def to_code(config):
    if CORE.is_esp32:
        # Ensures ESP-IDF links mbedtls (needed for mbedtls_gcm_* AES-128-GCM decrypt in
        # aes_gcm.h). Without this, this component's flat "src" pseudo-component gets no
        # REQUIRES on at least the platformio-espressif32 (pioarduino) build ESPHome uses
        # here, and mbedtls_gcm_* comes back undefined at link time despite ESP-IDF already
        # building libmbedcrypto.a for WiFi/TLS elsewhere. See CMakeLists.txt.in for detail.
        esp32.add_extra_build_file("src/CMakeLists.txt", Path(__file__).parent / "CMakeLists.txt.in")

    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    spa = _gz_bytes(config[CONF_SPA])
    spa_arr = cg.static_const_array(config[CONF_SPA_RAW_ID], cg.ArrayInitializer(*spa))
    cg.add(var.set_spa(spa_arr, len(spa)))

    presets = json.loads(Path(config[CONF_PRESETS]).read_text(encoding="utf-8"))
    minified = json.dumps(presets, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    presets_gz = gzip.compress(minified, compresslevel=9, mtime=0)
    presets_arr = cg.static_const_array(config[CONF_PRESETS_RAW_ID], cg.ArrayInitializer(*presets_gz))
    cg.add(var.set_presets(presets_arr, len(presets_gz)))
