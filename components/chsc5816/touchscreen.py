import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c, touchscreen
from esphome.const import CONF_INTERRUPT_PIN, CONF_RESET_PIN

DEPENDENCIES = ["i2c"]
chsc5816_ns = cg.esphome_ns.namespace("chsc5816")
CHSC5816Touchscreen = chsc5816_ns.class_(
    "CHSC5816Touchscreen", touchscreen.Touchscreen, i2c.I2CDevice
)
CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(CHSC5816Touchscreen),
        cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    }
).extend(i2c.i2c_device_schema(0x2E))


async def to_code(config):
    var = cg.new_Pvariable(config["id"])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])))
    cg.add(var.set_reset_pin(await cg.gpio_pin_expression(config[CONF_RESET_PIN])))
