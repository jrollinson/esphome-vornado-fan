"""ESPHome Vornado Controller Component."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import button
from esphome.const import CONF_ID

DEPENDENCIES = []
AUTO_LOAD = []

vornado_controller_ns = cg.esphome_ns.namespace("vornado_controller")
VornadoController = vornado_controller_ns.class_("VornadoController", cg.Component)

# Actions
SendCommandAction = vornado_controller_ns.class_("SendCommandAction", automation.Action)
SendSequenceAction = vornado_controller_ns.class_("SendSequenceAction", automation.Action)

# Button ID constants (must match C++ enum values)
BUTTON_POWER_ON = 0
BUTTON_POWER_OFF = 1
BUTTON_DIRECTION = 2
BUTTON_SPEED_INCREASE = 3
BUTTON_SPEED_DECREASE = 4
BUTTON_ENSURE_ON = 5

# Configuration keys
CONF_POWER_BUTTON = "power_button"
CONF_DIRECTION_BUTTON = "direction_button"
CONF_INCREASE_BUTTON = "increase_button"
CONF_DECREASE_BUTTON = "decrease_button"
CONF_MIN_SPACING_MS = "min_spacing_ms"
CONF_SCREEN_TIMEOUT_MS = "screen_timeout_ms"
CONF_ENSURE_DELAY_MS = "ensure_delay_ms"
CONF_BUTTON_ID = "button_id"
CONF_COMMANDS = "commands"

# Schema for button ID (accepts both int and named constant)
BUTTON_ID_SCHEMA = cv.Any(
    cv.int_range(min=BUTTON_POWER_ON, max=BUTTON_ENSURE_ON),
    cv.enum({
        "POWER_ON": BUTTON_POWER_ON,
        "POWER_OFF": BUTTON_POWER_OFF,
        "DIRECTION": BUTTON_DIRECTION,
        "SPEED_INCREASE": BUTTON_SPEED_INCREASE,
        "SPEED_DECREASE": BUTTON_SPEED_DECREASE,
        "ENSURE_ON": BUTTON_ENSURE_ON,
    })
)

# Component configuration schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(VornadoController),
    cv.Required(CONF_POWER_BUTTON): cv.use_id(button.Button),
    cv.Required(CONF_DIRECTION_BUTTON): cv.use_id(button.Button),
    cv.Required(CONF_INCREASE_BUTTON): cv.use_id(button.Button),
    cv.Required(CONF_DECREASE_BUTTON): cv.use_id(button.Button),
    cv.Optional(CONF_MIN_SPACING_MS, default=400): cv.positive_int,
    cv.Optional(CONF_SCREEN_TIMEOUT_MS, default=10000): cv.positive_int,
    cv.Optional(CONF_ENSURE_DELAY_MS, default=15000): cv.positive_int,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate C++ code for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set button references
    power_btn = await cg.get_variable(config[CONF_POWER_BUTTON])
    direction_btn = await cg.get_variable(config[CONF_DIRECTION_BUTTON])
    increase_btn = await cg.get_variable(config[CONF_INCREASE_BUTTON])
    decrease_btn = await cg.get_variable(config[CONF_DECREASE_BUTTON])
    
    cg.add(var.set_power_button(power_btn))
    cg.add(var.set_direction_button(direction_btn))
    cg.add(var.set_increase_button(increase_btn))
    cg.add(var.set_decrease_button(decrease_btn))

    # Set timing parameters
    cg.add(var.set_min_spacing_ms(config[CONF_MIN_SPACING_MS]))
    cg.add(var.set_screen_timeout_ms(config[CONF_SCREEN_TIMEOUT_MS]))
    cg.add(var.set_ensure_delay_ms(config[CONF_ENSURE_DELAY_MS]))


# Send Command Action
@automation.register_action(
    "vornado_controller.send_command",
    SendCommandAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoController),
        cv.Required(CONF_BUTTON_ID): cv.templatable(BUTTON_ID_SCHEMA),
    }),
    synchronous=True,
)
async def send_command_action_to_code(config, action_id, template_arg, args):
    """Generate code for send_command action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    template_ = await cg.templatable(config[CONF_BUTTON_ID], args, int)
    cg.add(var.set_button_id(template_))
    
    return var


# Send Sequence Action
@automation.register_action(
    "vornado_controller.send_sequence",
    SendSequenceAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(VornadoController),
        cv.Required(CONF_COMMANDS): cv.All(
            cv.ensure_list(BUTTON_ID_SCHEMA),
            cv.Length(min=1)
        ),
    }),
    synchronous=True,
)
async def send_sequence_action_to_code(config, action_id, template_arg, args):
    """Generate code for send_sequence action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    cg.add(var.set_commands(config[CONF_COMMANDS]))
    
    return var
