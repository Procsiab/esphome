from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]

airton_ns = cg.esphome_ns.namespace("airton")
AirtonClimate = airton_ns.class_("AirtonClimate", climate_ir.ClimateIR)

CONF_AIRTON_ID = "airton_id"
CONF_SLEEP_MODE = "sleep_mode"

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AirtonClimate),
    }
)

DisplayOnAction = airton_ns.class_("DisplayOnAction", automation.Action)
DisplayOffAction = airton_ns.class_("DisplayOffAction", automation.Action)
SleepOnAction = airton_ns.class_("SleepOnAction", automation.Action)
SleepOffAction = airton_ns.class_("SleepOffAction", automation.Action)

AIRTON_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AirtonClimate),
    }
)


@automation.register_action(
    "climate_ir.airton.display_on", DisplayOnAction, AIRTON_ACTION_SCHEMA
)
@automation.register_action(
    "climate_ir.airton.display_off", DisplayOffAction, AIRTON_ACTION_SCHEMA
)
async def display_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_action(
    "climate_ir.airton.sleep_on", SleepOnAction, AIRTON_ACTION_SCHEMA
)
@automation.register_action(
    "climate_ir.airton.sleep_off", SleepOffAction, AIRTON_ACTION_SCHEMA
)
async def sleep_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)
