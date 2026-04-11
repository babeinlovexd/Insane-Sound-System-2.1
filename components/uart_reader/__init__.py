import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor, sensor, binary_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True

uart_reader_ns = cg.esphome_ns.namespace("custom_uart_reader")
CustomUARTReader = uart_reader_ns.class_(
    "CustomUARTReader", cg.Component, uart.UARTDevice
)

CONF_TRACK = "track_sensor"
CONF_ARTIST = "artist_sensor"
CONF_STATUS = "status_sensor"
CONF_TEMP = "temp_sensor"
CONF_FAN = "fan_sensor"
CONF_FAULT = "fault_sensor"
CONF_STREAM = "stream_status"
CONF_SYS_STAT = "sys_stat"
CONF_ALBUM = "album_sensor"
CONF_BT_CONN = "bt_conn_sensor"
CONF_BT_VOL = "bt_vol_sensor"
CONF_IS_FLASHING = "is_flashing"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomUARTReader),
        cv.Optional(CONF_TRACK): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_ARTIST): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_ALBUM): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_STATUS): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_TEMP): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_FAN): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_FAULT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_STREAM): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_SYS_STAT): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_BT_CONN): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_BT_VOL): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_IS_FLASHING): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    track = await cg.get_variable(config[CONF_TRACK]) if CONF_TRACK in config else cg.nullptr
    artist = await cg.get_variable(config[CONF_ARTIST]) if CONF_ARTIST in config else cg.nullptr
    album = await cg.get_variable(config[CONF_ALBUM]) if CONF_ALBUM in config else cg.nullptr
    status = await cg.get_variable(config[CONF_STATUS]) if CONF_STATUS in config else cg.nullptr
    temp = await cg.get_variable(config[CONF_TEMP]) if CONF_TEMP in config else cg.nullptr
    fan = await cg.get_variable(config[CONF_FAN]) if CONF_FAN in config else cg.nullptr
    fault = await cg.get_variable(config[CONF_FAULT]) if CONF_FAULT in config else cg.nullptr
    stream = await cg.get_variable(config[CONF_STREAM]) if CONF_STREAM in config else cg.nullptr
    sys_stat = await cg.get_variable(config[CONF_SYS_STAT]) if CONF_SYS_STAT in config else cg.nullptr
    bt_conn = await cg.get_variable(config[CONF_BT_CONN]) if CONF_BT_CONN in config else cg.nullptr
    bt_vol = await cg.get_variable(config[CONF_BT_VOL]) if CONF_BT_VOL in config else cg.nullptr

    is_flashing = cg.nullptr
    if CONF_IS_FLASHING in config:
        glob_id = config[CONF_IS_FLASHING]
        is_flashing = cg.RawExpression(f"&{glob_id}")

    parent = await cg.get_variable(config[uart.CONF_UART_ID])

    var = cg.new_Pvariable(
        config[CONF_ID],
        parent, track, artist, album, status, temp, fan, fault, stream, sys_stat, bt_conn, bt_vol, is_flashing
    )

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
