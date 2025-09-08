freeze("$(PORT_DIR)/modules")
include("$(MPY_DIR)/extmod/asyncio")
#新添加路径
#freeze("$(MPY_DIR)/examples/usercmodule/lv_binding_micropython/driver/esp32")
freeze("$(MPY_DIR)/examples/usercmodule/lv_binding_micropython/driver/generic")
freeze("$(MPY_DIR)/examples/usercmodule/lv_binding_micropython/lib")
# Useful networking-related packages.
require("bundle-networking")

# Require some micropython-lib modules.
require("aioespnow")
require("dht")
require("ds18x20")
require("neopixel")
require("onewire")
require("umqtt.robust")
require("umqtt.simple")
require("upysh")
