# K10 系列模块函数接口清单（公开接口）

> 规则：过滤以下划线 `_` 开头的名称，仅保留公开接口。
> 范围：`k10_base`、`k10_box`、`mpython_ble`、`unihiker_k10` 目录。

## k10_base

### `k10_base/_ble.py`

- 类 `keycode` 方法：
  - `（无公开方法定义）`

- 类 `Mouse` 方法：
  - `（无公开方法定义）`

- 类 `hid` 方法：
  - `def isconnected(self)`
  - `def keyboard_send(self, key)`
  - `def mouse_key(self, key)`

### `k10_base/_k10_base.py`

- 模块函数：
  - `def dfrobot_global_irq_handler_timer()`
  - `def dfrobot_global_extio_timer_handler(_)`
  - `def smart_sd_mount()`
  - `def test_sd_mount_simple()`

- 类 `PinMode` 方法：
  - `（无公开方法定义）`

- 类 `extIO` 方法：
  - `def value(self, value=None)`
  - `def readIO(self)`
  - `def writeIO(self, value)`

- 类 `k10_pin` 方法：
  - `def irq(self, handler=None, trigger=Pin.IRQ_RISING)`
  - `def read_digital(self)`
  - `def write_digital(self, value)`
  - `def read_analog(self)`
  - `def write_analog(self, duty=0, freq=1000)`

- 类 `pin` 方法：
  - `def read_digital(self)`
  - `def write_digital(self, value)`
  - `def read_analog(self)`
  - `def write_analog(self, value=0, freq=5000)`
  - `def irq(self, handler=None, trigger=Pin.IRQ_FALLING)`

- 类 `Button` 方法：
  - `def check_state(self)`
  - `def is_pressed(self)`
  - `def was_pressed(self)`
  - `def get_presses(self)`
  - `def value(self)`
  - `def status(self)`
  - `def irq(self, *args, **kwargs)`

- 类 `button` 方法：
  - `def func(self, _)`
  - `def func_released(self, _)`
  - `def event_pressed(self)`
  - `def event_pressed(self, new_event_change)`
  - `def event_released(self)`
  - `def event_released(self, new_event_released)`
  - `def status(self)`

- 类 `AHT20` 方法：
  - `def init(self)`
  - `def reset(self)`
  - `def temperature(self)`
  - `def humidity(self)`
  - `def measure(self)`

- 类 `aht20` 方法：
  - `def measure(self)`
  - `def read(self)`
  - `def read_temp(self)`
  - `def read_temp_f(self)`
  - `def read_humi(self)`

- 类 `Light` 方法：
  - `def read(self)`

- 类 `Es7243e` 方法：
  - `def write_cmd(self, addr, reg, cmd)`
  - `def ctrl_state(self, addr, state)`
  - `def config(self, addr)`

- 类 `Mic` 方法：
  - `def reinit(self, bits=16, sample_rate=16000, channels=1)`
  - `def deinit(self)`
  - `def mount_sd_card(self)`
  - `def force_remount_sd(self)`
  - `def fix_sd_card_state(self)`
  - `def write_wav_header(self, file, num_samples)`
  - `def recode_to_wav(self, path, time)`
  - `def recode_sys(self, name='', time=10)`
  - `def recode_tf(self, name='', time=10)`

- 类 `Speaker` 方法：
  - `def deinit(self)`
  - `def reinit(self, bits=16, sample_rate=16000, channels=1)`
  - `def parse_wav_header(self, wav_file)`
  - `def play_tone(self, freq, beat)`
  - `def play_tone_music(self, tone_music)`
  - `def play_next_note(self, tone)`
  - `def play_sys_music(self, path)`
  - `def play_tf_music(self, path)`
  - `def play_music(self, path)`
  - `def stop_music(self)`

- 类 `TF_card` 方法：
  - `def deinit(self)`

- 类 `Camera` 方法：
  - `def init(self)`
  - `def camera_capture(self)`
  - `def save(self)`

- 类 `Screen` 方法：
  - `def init(self, dir=2)`
  - `def show_bg(self, color=16777215)`
  - `def show_draw(self)`
  - `def clear(self, line=0, font=None, color=None)`
  - `def draw_text(self, text='', line=None, x=0, y=0, font_size=16, color=255)`
  - `def draw_point(self, x=0, y=0, color=255)`
  - `def set_width(self, width=1)`
  - `def draw_line(self, x0=0, y0=0, x1=0, y1=0, color=0)`
  - `def draw_circle(self, x=0, y=0, r=0, bcolor=0, fcolor=None)`
  - `def draw_rect(self, x=0, y=0, w=0, h=0, bcolor=0, fcolor=None)`
  - `def draw_sys_img(self, image='/q.bmp', x=0, y=0, debug=False)`
  - `def show_camera_feed(self, buf)`
  - `def show_camera_img(self, buf)`
  - `def show_camera(self, camera)`
  - `def stop_camera(self)`
  - `def show_camera_img_safe(self, buf)`
  - `def deinit(self, full=False, backlight_off=False)`

- 类 `Wifibase` 方法：
  - `def connectWiFi(self, ssid, passwd, timeout=10)`
  - `def disconnectWiFi(self)`
  - `def enable_APWiFi(self, essid, password=b'', channel=10)`
  - `def disable_APWiFi(self)`

- 类 `WiFi` 方法：
  - `def connect(self, ssid, psd, timeout=10000)`
  - `def status(self)`
  - `def info(self)`

- 类 `MqttClient` 方法：
  - `def connect(self, **kwargs)`
  - `def connected(self)`
  - `def publish(self, topic, content, _qos=1)`
  - `def message(self, topic)`
  - `def received(self, topic, callback)`
  - `def subscribe(self, topic, callback)`
  - `def on_message(self, topic, msg)`
  - `def default_callbak(self)`
  - `def mqtt_check_msg(self)`
  - `def mqtt_heartbeat(self, _)`

## k10_box

### `k10_box/_k10_box.py`

- 类 `Voice` 方法：
  - `def read(self)`

- 类 `Knob` 方法：
  - `def read(self)`

- 类 `Ir` 方法：
  - `def data(self)`

- 类 `Sr04` 方法：
  - `def distance(self)`

- 类 `Buzzer` 方法：
  - `def play(self, freq)`
  - `def stop(self)`

- 类 `Led` 方法：
  - `def red_digital(self, state)`
  - `def yellow_digital(self, state)`
  - `def green_digital(self, state)`
  - `def red_analog(self, value)`
  - `def yellow_analog(self, value)`
  - `def green_analog(self, value)`

- 类 `Motor` 方法：
  - `def set_m1_speed(self, m1a, m1b)`
  - `def set_m2_speed(self, m2a, m2b)`
  - `def M1_run(self, dir, speed)`
  - `def M2_run(self, dir, speed)`
  - `def M1_stop(self)`
  - `def M2_stop(self)`

- 类 `Line` 方法：
  - `def begin(self)`
  - `def set_threshod(self, num, value)`
  - `def get_threshod(self)`
  - `def get_adc_all(self)`
  - `def get_status_all(self)`
  - `def get_status(self, num)`

- 类 `qmi8658` 方法：
  - `def read_acc_x(self)`
  - `def read_acc_y(self)`
  - `def read_acc_z(self)`
  - `def read_gyro_x(self)`
  - `def read_gyro_y(self)`
  - `def read_gyro_z(self)`
  - `def read_acc_streng(self)`

- 类 `Acc` 方法：
  - `def read_x(self)`
  - `def read_y(self)`
  - `def read_z(self)`
  - `def read_strength(self)`

- 类 `Gyro` 方法：
  - `def read_x(self)`
  - `def read_y(self)`
  - `def read_z(self)`

## mpython_ble

### `mpython_ble/advertising/__init__.py`

- 模块函数：
  - `def advertising_payload(limited_disc=False, br_edr=False, name=None, services=None, appearance=None, ad_structure=None)`
  - `def decode_field(payload, adv_type)`
  - `def decode_name(payload)`
  - `def decode_services(payload)`
  - `def demo()`

- 类 `BLEError` 方法：
  - `（无公开方法定义）`

- 类 `AD_Structure` 方法：
  - `（无公开方法定义）`

### `mpython_ble/application/beacon.py`

- 类 `iBeacon` 方法：
  - `def advertise(self, toggle=True, interval_us=500000)`

### `mpython_ble/application/centeral.py`

- 类 `Centeral` 方法：
  - `def connect(self, name=b'', addr=None)`
  - `def is_connected(self)`
  - `def connected_info(self)`
  - `def disconnect(self)`
  - `def characteristic_read(self, value_handle)`
  - `def characteristic_write(self, value_handle, data)`
  - `def notify_callback(self, callback)`

### `mpython_ble/application/hid.py`

- 类 `HID` 方法：
  - `def battery_level(self)`
  - `def battery_level(self, level)`
  - `def advertise(self, toggle=True)`
  - `def disconnect(self)`
  - `def mouse_click(self, buttons)`
  - `def mouse_press(self, buttons)`
  - `def mouse_release(self, buttons)`
  - `def mouse_release_all(self)`
  - `def mouse_move(self, x=0, y=0, wheel=0)`
  - `def keyboard_press(self, *keycodes)`
  - `def keyboard_release(self, *keycodes)`
  - `def keyboard_release_all(self)`
  - `def keyboard_send(self, *keycodes)`
  - `def consumer_send(self, consumer_code)`

### `mpython_ble/application/peripheral.py`

- 类 `Peripheral` 方法：
  - `def mac(self)`
  - `def write_callback(self, callback)`
  - `def connection_callback(self, callback)`
  - `def advertise(self, toggle=True)`
  - `def attrubute_write(self, value_handle, data, notify=False)`
  - `def attrubute_read(self, value_handle)`
  - `def disconnect(self)`

### `mpython_ble/application/uart.py`

- 类 `BLEUART` 方法：
  - `def is_connected(self)`
  - `def irq(self, handler)`
  - `def any(self)`
  - `def read(self, size=None)`
  - `def write(self, data)`
  - `def close(self)`

### `mpython_ble/characteristics/__init__.py`

- 类 `Characteristic` 方法：
  - `def add_descriptors(self, *descriptors)`
  - `def definition(self)`
  - `def handles(self)`
  - `def handles(self, value_handles)`

### `mpython_ble/const.py`

- 类 `IRQ` 方法：
  - `（无公开方法定义）`

- 类 `ADType` 方法：
  - `（无公开方法定义）`

- 类 `AdvType` 方法：
  - `（无公开方法定义）`

### `mpython_ble/descriptors/__init__.py`

- 类 `Descriptor` 方法：
  - `def definition(self)`

### `mpython_ble/gatts/__init__.py`

- 类 `Profile` 方法：
  - `def add_services(self, *services)`
  - `def definition(self)`
  - `def services_uuid(self)`
  - `def handles(self)`
  - `def handles(self, value_handles)`

### `mpython_ble/hidcode.py`

- 类 `Mouse` 方法：
  - `（无公开方法定义）`

- 类 `KeyboardCode` 方法：
  - `（无公开方法定义）`

- 类 `ConsumerCode` 方法：
  - `（无公开方法定义）`

### `mpython_ble/services/__init__.py`

- 类 `Service` 方法：
  - `def add_characteristics(self, *characteristics)`
  - `def definition(self)`
  - `def handles(self)`
  - `def handles(self, value_handles)`

## unihiker_k10

### `unihiker_k10/_unihiker_k10.py`

- 模块函数：
  - `def unihiker_k10_collect()`

- 类 `Accelerometer` 方法：
  - `def x(self)`
  - `def y(self)`
  - `def z(self)`
  - `def gesture(self)`
  - `def strength(self)`

- 类 `K10BoxAccelAdapter` 方法：
  - `def x(self)`
  - `def y(self)`
  - `def z(self)`
  - `def shake(self)`
  - `def strength(self)`

- 类 `accelerometer` 方法：
  - `def timer_callback(self, _)`
  - `def accelerometer_callback(self)`
  - `def X(self)`
  - `def Y(self)`
  - `def Z(self)`
  - `def read_x(self)`
  - `def read_y(self)`
  - `def read_z(self)`
  - `def read_strength(self)`
  - `def shake(self)`
  - `def gesture(self)`
  - `def status(self, status='')`

- 类 `rgb_board` 方法：
  - `def write(self, num=-1, R=0, G=0, B=0, color=None)`
  - `def brightness(self, bright=9)`
  - `def get_brightness(self, level)`
  - `def hsv_to_rgb(self, h, s, v)`
  - `def rainbow_cycle(self, start_led=0, end_led=None, hue_start=0, hue_end=360)`
  - `def shift(self, shift)`
  - `def interpolate_color(self, ratio)`
  - `def bargraph(self, start_led, end_led, color_level, max_level)`
  - `def clear(self)`

- 类 `servo` 方法：
  - `def angle(self, value)`

- 类 `ds18b20` 方法：
  - `def read(self)`

- 类 `hx711` 方法：
  - `def peel(self)`
  - `def peel_flag(self)`
  - `def set_calibration(self, value)`
  - `def get_calibration(self)`
  - `def average(self, times)`
  - `def get_value(self)`
  - `def read_weight(self, times)`
  - `def common_measure(self)`

- 类 `force` 方法：
  - `def zero(self)`
  - `def read(self, mass=True)`

- 类 `sen0388` 方法：
  - `def distance_mm(self)`
  - `def distance_cm(self)`

- 类 `ultrasonic` 方法：
  - `def distance(self)`

- 类 `dht` 方法：
  - `def read(self, max_retries=8, retry_delay=0.5)`
  - `def get_sensor_type(self)`

- 类 `neopixel` 方法：
  - `def brightness(self, bright=9)`
  - `def write(self, begin, end, R=0, G=0, B=0)`
  - `def clear(self)`

