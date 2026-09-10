import gzip
import logging
from pathlib import Path

import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_COMPRESSION,
    CONF_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
    PlatformFramework,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# gPlug fork of ESPHome's stock captive_portal (based on esphome 2026.6.5). Only functional
# change vs. upstream: the page served by handleRequest() is a runtime byte array loaded from a
# real, editable captive.html at codegen time (same _gz_bytes()/static_const_array pattern
# gplug_smi/__init__.py already uses for the SPA) instead of the vendored captive_index.h, so the
# WiFi-setup page phones see during AP-fallback carries gPlug's branding. See firmware/README.md
# and intent/intent.md (2026-09-10) for why. Diff this file and captive_portal.{h,cpp} against a
# fresh copy of upstream before every ESPHome version bump.
CONF_HTML = "html"
CONF_HTML_RAW_ID = "html_raw_id"


def _gz_bytes(path: Path) -> bytes:
    data = path.read_bytes()
    return data if path.suffix == ".gz" else gzip.compress(data, compresslevel=9, mtime=0)


def AUTO_LOAD() -> list[str]:
    auto_load = ["web_server_base", "ota.web_server"]
    if CORE.is_esp32:
        auto_load.append("socket")
    return auto_load


DEPENDENCIES = ["wifi"]
CODEOWNERS = ["@gplug"]

captive_portal_ns = cg.esphome_ns.namespace("captive_portal")
CaptivePortal = captive_portal_ns.class_("CaptivePortal", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CaptivePortal),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            # gPlug: the fork's byte source (captive.html) is always produced via _gz_bytes(),
            # i.e. always gzip. "br" is upstream-only (a separate pre-brotli'd vendored array);
            # not meaningful here, so narrowed rather than silently mismatching headers.
            cv.Optional(CONF_COMPRESSION, default="gzip"): cv.one_of("gzip"),
            cv.Optional(CONF_HTML): cv.string,
            cv.GenerateID(CONF_HTML_RAW_ID): cv.declare_id(cg.uint8),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_BK72XX,
            PLATFORM_LN882X,
            PLATFORM_RP2040,
            PLATFORM_RTL87XX,
        ]
    ),
)


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    wifi_conf = full_config.get("wifi")

    if wifi_conf is None:
        # This shouldn't happen due to DEPENDENCIES = ["wifi"], but check anyway
        raise cv.Invalid("Captive portal requires the wifi component to be configured")

    if CONF_AP not in wifi_conf:
        _LOGGER.warning(
            "Captive portal is enabled but no WiFi AP is configured. "
            "The captive portal will not be accessible. "
            "Add 'ap:' to your WiFi configuration to enable the captive portal."
        )

    # Register socket needs for DNS server and additional HTTP connections
    # - 1 UDP socket for DNS server
    # - 3 TCP sockets for captive portal detection probes + configuration requests
    #   OS captive portal detection makes multiple probe requests that stay in TIME_WAIT.
    #   Need headroom for actual user configuration requests.
    #   LRU purging will reclaim idle sockets to prevent exhaustion from repeated attempts.
    # The listening socket is registered by web_server_base (shared HTTP server).
    from esphome.components import socket

    socket.consume_sockets(3, "captive_portal")(config)
    socket.consume_sockets(1, "captive_portal", socket.SocketType.UDP)(config)

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.CAPTIVE_PORTAL)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
    cg.add_define("USE_CAPTIVE_PORTAL")

    if config[CONF_COMPRESSION] == "gzip":
        cg.add_define("USE_CAPTIVE_PORTAL_GZIP")

    html_path = (
        Path(CORE.relative_config_path(config[CONF_HTML]))
        if CONF_HTML in config
        else Path(__file__).parent / "captive.html"
    )
    if not html_path.is_file():
        raise cv.Invalid(f"File not found: {html_path}")
    index_gz = _gz_bytes(html_path)
    index_arr = cg.static_const_array(config[CONF_HTML_RAW_ID], cg.ArrayInitializer(*index_gz))
    cg.add(var.set_index(index_arr, len(index_gz)))

    if CORE.using_arduino and (CORE.is_esp8266 or CORE.is_libretiny or CORE.is_rp2040):
        cg.add_library("DNSServer", None)


# Only compile the ESP-IDF DNS server when using ESP-IDF framework
FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "dns_server_esp32_idf.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
    }
)
