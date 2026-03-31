"""ESPHome Vornado Stateful Controller Component - Button-Based Control."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, text_sensor
from esphome.components.vornado_controller import vornado_controller_ns, VornadoController
from esphome.const import (
    CONF_ID,
    CONF_ICON,
    CONF_INTERNAL,
    CONF_NAME,
)

DEPENDENCIES = ["vornado_controller"]
AUTO_LOAD = ["sensor", "text_sensor"]

# Declare the controller class (just Component, not fan::Fan)
VornadoStatefulController = vornado_controller_ns.class_(
    "VornadoStatefulController", cg.Component
)

# Actions
TurnOnAction = vornado_controller_ns.class_("TurnOnAction", automation.Action)
TurnOffAction = vornado_controller_ns.class_("TurnOffAction", automation.Action)
SetSpeedAction = vornado_controller_ns.class_("SetSpeedAction", automation.Action)
ToggleDirectionAction = vornado_controller_ns.class_("ToggleDirectionAction", automation.Action)
ResetStateAction = vornado_controller_ns.class_("ResetStateAction", automation.Action)

# Configuration keys
CONF_CONTROLLER = "controller"
CONF_SPEED = "speed"

# Component configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VornadoStatefulController),
        cv.Required(CONF_CONTROLLER): cv.use_id(VornadoController),
        cv.Optional(CONF_NAME, default="Vornado"): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate C++ code for the stateful controller component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set controller reference
    controller = await cg.get_variable(config[CONF_CONTROLLER])
    cg.add(var.set_controller(controller))

    # Auto-create power state text sensor (NOT diagnostic)
    power_sens_config = text_sensor.text_sensor_schema(
        icon="mdi:power",
    ).extend({
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
    })
    power_sens_id = f"{config[CONF_ID].id}_power_state"
    power_config = power_sens_config({
        CONF_ID: power_sens_id,
        CONF_NAME: f"{config.get(CONF_NAME, 'Vornado')} Power State",
        CONF_ICON: "mdi:power",
        CONF_INTERNAL: False,
    })
    power_sensor = cg.new_Pvariable(power_config[CONF_ID])
    await text_sensor.register_text_sensor(power_sensor, power_config)
    cg.add(var.set_power_state_sensor(power_sensor))
    
    # Auto-create speed state numeric sensor (NOT diagnostic)
    speed_sens_config = sensor.sensor_schema(
        icon="mdi:fan",
        accuracy_decimals=0,
    ).extend({
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
    })
    speed_sens_id = f"{config[CONF_ID].id}_speed_state"
    speed_config = speed_sens_config({
        CONF_ID: speed_sens_id,
        CONF_NAME: f"{config.get(CONF_NAME, 'Vornado')} Speed",
        CONF_ICON: "mdi:fan",
        CONF_INTERNAL: False,
    })
    speed_sensor = cg.new_Pvariable(speed_config[CONF_ID])
    await sensor.register_sensor(speed_sensor, speed_config)
    cg.add(var.set_speed_state_sensor(speed_sensor))


# ============================================================================
# Actions
# ============================================================================

@automation.register_action(
    "vornado_stateful.turn_on",
    TurnOnAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoStatefulController),
    }),
    synchronous=True,
)
async def turn_on_action_to_code(config, action_id, template_arg, args):
    """Generate code for turn_on action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "vornado_stateful.turn_off",
    TurnOffAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoStatefulController),
    }),
    synchronous=True,
)
async def turn_off_action_to_code(config, action_id, template_arg, args):
    """Generate code for turn_off action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "vornado_stateful.set_speed",
    SetSpeedAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoStatefulController),
        cv.Required(CONF_SPEED): cv.templatable(cv.int_range(min=1, max=4)),
    }),
    synchronous=True,
)
async def set_speed_action_to_code(config, action_id, template_arg, args):
    """Generate code for set_speed action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_SPEED], args, int)
    cg.add(var.set_speed(template_))
    return var


@automation.register_action(
    "vornado_stateful.toggle_direction",
    ToggleDirectionAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoStatefulController),
    }),
    synchronous=True,
)
async def toggle_direction_action_to_code(config, action_id, template_arg, args):
    """Generate code for toggle_direction action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "vornado_stateful.reset_state",
    ResetStateAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoStatefulController),
    }),
    synchronous=True,
)
async def reset_state_action_to_code(config, action_id, template_arg, args):
    """Generate code for reset_state action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
