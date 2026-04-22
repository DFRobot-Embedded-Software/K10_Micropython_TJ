from k10_base import pin,button,temp_humi
from k10_base import Light,Mic,Speaker,TF_card,Screen,Camera,WiFi,MqttClient,Timer,time,Pin,hid,keycode
from k10_base import k10_i2c, pins_remap_k10
from neopixel import NeoPixel
from machine import Servo,I2C
import machine,onewire, struct,gc,math
from ds18x20 import DS18X20
from hcsr04 import HCSR04
from dht import DHT11, DHT22
gc.collect()  

'''
六轴的驱动类
'''
class Accelerometer(object):
    SHANK = 0          # Shake gesture
    SCREEN_UP = 1       # Screen facing up
    SCREEN_DOWN = 2     # Screen facing down
    TILT_LEFT = 3       # Tilt left
    TILT_RIGHT = 4      # Tilt right
    TILT_FORWARD = 5    # Tilt forward
    TILT_BACK = 6       # Tilt backward
    GESTURE_NONE = 7     # No gesture detected.
    def __init__(self) :
        self.shake_status = False
        self._i2c = k10_i2c
        self._addr = 0x19
        self._gesture = self.GESTURE_NONE
        self.X = 0.0
        self.Y = 0.0
        self.Z = 0.0
        self._begin()
        self._measure()
        self.ssvtA = 1 << 12
        self.available = True

    def _begin(self):
        buf = self._read_bytes(0x24, 1)
        data = buf[0]
        data |= 0x08
        self._writeReg(0x24, data)

        data = 0x00
        data |= 0x40
        data |= 0x03
        data |= 0x0C
        data |= 0x38
        self._writeReg(0x30, data)

        buf = self._read_bytes(0x21, 1)
        data = buf[0]
        data |= 0x81
        self._writeReg(0x21, data)
        self._writeReg(0x32, 0x60)
        self._writeReg(0x33, 0x02)

        buf = self._read_bytes(0x22, 1)
        data = buf[0]
        data |= 0x40
        self._writeReg(0x21, data)

        buf = self._read_bytes(0x24, 1)
        data = buf[0]
        data |= 0x02
        self._writeReg(0x24, data)

        buf = self._read_bytes(0x25, 1)
        data = buf[0]
        data |= 0x02
        self._writeReg(0x25, data)

        data = 0x00
        data |= 0xc0
        data |= 0x3f
        self._writeReg(0x34, data)

        buf = self._read_bytes(0x21, 1)
        data = buf[0]
        data |= 0xfd
        self._writeReg(0x21, data)

        self._writeReg(0x36, 0x18)
        self._writeReg(0x37, 0x02)

        buf = self._read_bytes(0x25, 1)
        data = buf[0]
        data |= 0x20
        self._writeReg(0x21, data)

        self._read_bytes(0x23, 1)
        self._writeReg(0x23, 0x88)
        self._writeReg(0x20, 0x27)


    def _read_bytes(self, reg, size):
        rslt = self._i2c.readfrom_mem(self._addr, reg, size)
        return rslt
    def _writeReg(self, reg, value):
        self._i2c.writeto_mem(self._addr, reg, value.to_bytes(1, 'little'))

    def _measure(self):
        tempbuf = self._read_bytes(0x27,1)
        if (tempbuf[0] & 0x0F) == 0x0F:
            accbuf = self._read_bytes(0xA8, 6)
    
            # 原始加速度 raw 12-bit（整数）
            rawX = (accbuf[1] << 8 | accbuf[0]) >> 4
            rawY = (accbuf[3] << 8 | accbuf[2]) >> 4
            rawZ = (accbuf[5] << 8 | accbuf[4]) >> 4
    
            # 补码处理，保持整数
            if (rawX & 0x800):
                rawX -= 4096
            if (rawY & 0x800):
                rawY -= 4096
            if (rawZ & 0x800):
                rawZ -= 4096
    
            # 保存原始整数值（位运算用）
            self.rawX = rawX
            self.rawY = rawY
            self.rawZ = rawZ
    
            # g 单位浮点（显示用）
            self.X = rawX
            self.Y = rawY
            self.Z = rawZ
    
        # 手势部分保持不变
        tempbuf = self._read_bytes(0x35,1)
        if   (tempbuf[0] & 0x60) == 0x60:
            self._gesture = self.SCREEN_DOWN
        elif (tempbuf[0] & 0x50) == 0x50:
            self._gesture = self.SCREEN_UP
        elif (tempbuf[0] & 0x41) == 0x41:
            self._gesture = self.TILT_LEFT
        elif (tempbuf[0] & 0x42) == 0x42:
            self._gesture = self.TILT_RIGHT
        elif (tempbuf[0] & 0x44) == 0x44:
            self._gesture = self.TILT_FORWARD
        elif (tempbuf[0] & 0x48) == 0x48:
            self._gesture = self.TILT_BACK
        elif (tempbuf[0] != 0):
            self._gesture = self.SHANK
    

    def x(self):
        #self.X = _accelerometer.get_x()
        return self.X

    def y(self):
        #self.Y = _accelerometer.get_y()
        return self.Y

    def z(self):
        #self.Z = _accelerometer.get_z()
        return self.Z
    
    def gesture(self):
        return self._gesture

    def strength(self):
        x = self.X 
        y = self.Y
        z = self.Z
        return math.sqrt(x*x + y*y + z*z)

'''
K10box加速度计适配器类，使k10_box.acc接口与Accelerometer兼容
'''
class K10BoxAccelAdapter(object):
    # 手势常量，与Accelerometer类保持一致
    SHANK = 0
    SCREEN_UP = 1
    SCREEN_DOWN = 2
    TILT_LEFT = 3
    TILT_RIGHT = 4
    TILT_FORWARD = 5
    TILT_BACK = 6
    GESTURE_NONE = 7
    
    def __init__(self):
        try:
            import k10_box
            self._acc = k10_box.acc
            self.shake_status = False
            self._gesture = self.GESTURE_NONE
            self.X = 0.0
            self.Y = 0.0
            self.Z = 0.0
            self.ssvtA = 1 << 12
            self.available = True
        except Exception:
            self._acc = None
    
    def _measure(self):
        if self._acc is None or not self._acc.available:
            self.rawX = 0
            self.rawY = 0
            self.rawZ = 0
            self.X = 0.0
            self.Y = 0.0
            self.Z = 0.0
            return

        # mg 原始整数
        self.rawX = self._acc.read_x()
        self.rawY = self._acc.read_y()
        self.rawZ = self._acc.read_z()

        # 转成 g 浮点
        self.X = self.rawX
        self.Y = self.rawY
        self.Z = self.rawZ
    
    def x(self):
        return self.X
    
    def y(self):
        return self.Y
    
    def z(self):
        return self.Z
    
    def shake(self):
        return self.shake_status

    def strength(self):
        x = self.X 
        y = self.Y
        z = self.Z
        return math.sqrt(x*x + y*y + z*z)

'''
为了兼容上层API使用做的类
'''
class accelerometer(object):
    def __init__(self):
        self.tim_count = 0
        self._is_shaked = False
        self._last_x = 0
        self._last_y = 0
        self._last_z = 0
        self._count_shaked = 0
        self._i2c = k10_i2c
        devices = self._i2c.scan()
        #0x19是K10的加速度计地址
        if 0x19 in devices:
            #K10
            self.tim = Timer(2)
            self.tim.init(period=100, mode=Timer.PERIODIC, callback=self.timer_callback)
            self.accel_sensor = Accelerometer()
        else:
            #K10box
            self.tim = None
            self.accel_sensor = K10BoxAccelAdapter()

    def timer_callback(self,_):
        self.tim_count += 1 
        try:
            self.accelerometer_callback()
        except Exception as e:
            print(str(e))
        if(self.tim_count==200):
            self.tim_count = 0

    def accelerometer_callback(self):
        '''加速度计'''
        if self.accel_sensor is None:
            return
        if self._is_shaked:
            self._count_shaked += 1
            if self._count_shaked == 5: 
                self._count_shaked = 0
                self.accel_sensor.shake_status = False
        self.accel_sensor._measure()
        x=self.accel_sensor.x(); y=self.accel_sensor.y();z=self.accel_sensor.z()
        if self._last_x == 0 and self._last_y == 0 and self._last_z == 0:
            self._last_x = x; 
            self._last_y = y; 
            self._last_z = z; 
            self.accel_sensor.shake_status = False
            return
        diff_x = x - self._last_x; diff_y = y - self._last_y; diff_z = z - self._last_z
        self._last_x = x; self.last_y = y; self._last_z = z
        if self._count_shaked > 0: 
            return
        self._is_shaked = (diff_x * diff_x + diff_y * diff_y + diff_z * diff_z > 1)
        if self._is_shaked: 
            self.accel_sensor.shake_status = True
        
    def X(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.x()
    
    def Y(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.y()
    
    def Z(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.z()
    
    def read_x(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.x()
    
    def read_y(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.y()
    
    def read_z(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.z()

    def read_strength(self):
        if self.accel_sensor is None:
            return 0.0
        self.accel_sensor._measure()
        return self.accel_sensor.strength()
    
    def shake(self):
        if self.accel_sensor is None:
            return False
        return self.accel_sensor.shake()
    
    def gesture(self):
        if self.accel_sensor is None:
            return 7  # GESTURE_NONE
        return self.accel_sensor._gesture
    def status(self,status=""):
        if self.accel_sensor is None:
            return False
        # 使用常量值，与Accelerometer类保持一致
        TILT_FORWARD = 5
        TILT_BACK = 6
        TILT_LEFT = 3
        TILT_RIGHT = 4
        SCREEN_UP = 1
        SCREEN_DOWN = 2
        if status is "forward":
            return self.gesture() == TILT_FORWARD
        elif status is "back":
            return self.gesture() == TILT_BACK
        elif status is "left":
            return self.gesture() == TILT_LEFT
        elif status is "right":
            return self.gesture() == TILT_RIGHT
        elif status is "up":
            return self.gesture() == SCREEN_UP
        elif status is "down":
            return self.gesture() == SCREEN_DOWN
        else:
            return False

'''
k10的板载2812灯的控制类(灯的定义顺序是反的)
'''
class rgb_board():
    '''
    def __init__(self,pin=None):
        self.my_rgb = NeoPixel(Pin(46, Pin.OUT), n=10, bpp=3, timing=1)
        self.bright = 9
    '''
    def __init__(self,pin=46,number=10):
        self.my_rgb = NeoPixel(Pin(pin, Pin.OUT), n=number, bpp=3, timing=1)
        self.bright = 9
        self._number = number
        self.clear()

    def write(self,num=-1,R=0,G=0,B=0,color=None):
        #如果传入了color，则听color的
        if num > 2:
            num = 2
        if color == None:
            pass
        else:
            R = (color >> 16) & 0xFF
            G = (color >> 8) & 0xFF
            B = (color ) & 0xFF
        self.r = int(R/(10-self.bright))
        self.g = int(G/(10-self.bright))
        self.b = int(B/(10-self.bright))
        if num == -1:
            for i in range(3):
                self.my_rgb[i] = (self.r, self.g, self.b)
                self.my_rgb.write()
                time.sleep(0.001)
        else:
            #K10的灯顺序需要调一下
            self.my_rgb[2-num] = (self.r, self.g, self.b)
            self.my_rgb.write()
            time.sleep(0.001)
    def brightness(self,bright=9):
        if bright <= 9 and bright >= 0:
            self.bright = bright
            
    def get_brightness(self, level):
        """
        根据段号（0到9）返回对应的亮度值（0到255）
        :param level: 段号（0到9）
        :return: 亮度值（0到255）
        """
        if level < 0 or level > 9:
            raise ValueError("段号必须在0到9之间")
        # 亮度分段：0, 28, 56, 85, 113, 141, 170, 198, 226, 255
        brightness_levels = [0, 28, 56, 85, 113, 141, 170, 198, 226, 255]
        return brightness_levels[level]

    def hsv_to_rgb(self, h, s, v):
        """将HSV颜色空间转换为RGB颜色空间"""
        if s == 0.0:
            return (v, v, v)
        i = int(h * 6.0)
        f = (h * 6.0) - i
        p = v * (1.0 - s)
        q = v * (1.0 - s * f)
        t = v * (1.0 - s * (1.0 - f))
        i = i % 6
        if i == 0:
            return (v, t, p)
        if i == 1:
            return (q, v, p)
        if i == 2:
            return (p, v, t)
        if i == 3:
            return (p, q, v)
        if i == 4:
            return (t, p, v)
        if i == 5:
            return (v, p, q)
    
    def rainbow_cycle(self, start_led=0, end_led=None, hue_start=0, hue_end=360):
        """
        在NeoPixel灯带上生成彩虹色效果，支持配置显示的灯号和颜色范围
        :param np: NeoPixel对象
        :param wait: 每次颜色变化后的延迟时间（秒）
        :param start_led: 起始LED索引（包含）
        :param end_led: 结束LED索引（不包含），如果为None，则使用所有LED
        :param hue_start: 起始色相值（1到360）
        :param hue_end: 结束色相值（1到360）
        """
        end_led += 1
        if end_led is None:
            end_led = len(self.my_rgb)
        num_leds = end_led - start_led

        # 将色相值从1-360映射到0-1
        hue_start_norm = hue_start / 360.0
        hue_end_norm = hue_end / 360.0
        brightness = self.get_brightness(self.bright) / 255.0 
        
        for i in range(start_led, end_led):
            # 计算当前LED的色相值
            hue = hue_start_norm + (i - start_led) / num_leds * (hue_end_norm - hue_start_norm) 
            hue = hue - int(hue)  # 确保hue在0-1之间
            rgb = self.hsv_to_rgb(hue, 1.0, brightness)
            # 将RGB值从0-1范围转换为0-255范围
            self.my_rgb[i] = tuple(int(c * 255) for c in rgb)
        self.my_rgb.write()
        time.sleep(0.001)

    def shift(self, shift):
        """
        让灯带上的像素整体向前移动一定步长。

        :param np: NeoPixel对象
        :param step: 移动的步长（默认为1个像素）
        """
        num_leds = len(self.my_rgb)

        # 复制当前像素颜色
        prev_colors = [self.my_rgb[i] for i in range(num_leds)]

        # 移动像素
        for i in range(num_leds):
            src_index = i - shift
            if src_index >= 0:
                self.my_rgb[i] = prev_colors[src_index]
            else:
                self.my_rgb[i] = (0, 0, 0)  # 移出范围的像素熄灭

        self.my_rgb.write()

    def interpolate_color(self,ratio):
        """
        计算蓝色到红色的渐变颜色。
        :param ratio: 颜色插值比例（0.0 - 1.0）
        :return: (R, G, B) 颜色元组
        """
        #brightness = self.get_brightness(self.bright) / 255.0 
        r = int(255/(10-self.bright) * ratio)
        g = 0
        b = int(255/(10-self.bright) * (1 - ratio))
        return (r, g, b)

    def bargraph(self, start_led, end_led, color_level, max_level):
        """
        在指定范围的RGB灯上绘制蓝色到红色渐变的条形图。

        :param start_led: 起始灯号
        :param end_led: 结束灯号
        :param color_level: 需要点亮的灯数量（0到max_level）
        :param max_level: 最高亮灯数量（用于映射color_level到灯号范围）
        """
        num_leds = end_led - start_led + 1

        # 将color_level映射到灯带范围
        mapped_leds = int((color_level / max_level) * num_leds)
        mapped_leds = min(mapped_leds, num_leds)  # 确保不会超出范围

        for i in range(start_led, end_led + 1):
            ratio = (i - start_led) / num_leds  # 计算渐变比例
            color = self.interpolate_color(ratio)
            if i < start_led + mapped_leds:
                 self.my_rgb[i] = color
            else:
                 self.my_rgb[i] = (0, 0, 0)  # 关闭LED

        self.my_rgb.write()
        
    def clear(self):
        self.my_rgb.fill((0,0,0))
        self.my_rgb.write()
        time.sleep(0.001)

'''
外置舵机驱动类
'''
class servo(Servo):
    def __init__(self,pin):
        super().__init__(pin)
    
    def angle(self, value):
        value = int(value)
        if(value<0):
            value = 0
        if(value>180):
            value = 180
        self.write_angle(value)

'''
外置18B20温度传感器驱动类
'''
class ds18b20(object):
    def __init__(self, pin):
        self._pin = pins_remap_k10[pin]
        self.dat = Pin(self._pin)
   
    def read(self):
        # create the onewire object
        self.ds = DS18X20(onewire.OneWire(self.dat))
        # scan for devices on the bus
        roms = self.ds.scan()
        # print('found devices:', roms)
        self.ds.convert_temp()
        time.sleep(0.75)
        temp = self.ds.read_temp(roms[0])
        # print(temp,end='℃\n ')
        return temp

'''
外置重量传感器hx711驱动类
'''
class hx711(object):
    REG_DATA_GET_RAM_DATA      = 0x66  #Get sensor raw data
    REG_DATA_GET_CALIBRATION   = 0x67  #Gets the automatic calibration value
    REG_DATA_SET_CALIBRATION   = 0x68  #Obtain peeling position
    REG_DATA_GET_PEEL_FLAG     = 0x69  #Module initialization
    REG_SET_CAL_THRESHOLD      = 0x71  #Set the calibration trigger threshold
    REG_SET_TRIGGER_WEIGHT     = 0x72  #Set calibration weight
    _calibration = 2210.0
    _offset = 0
    def __init__(self, address = 0x64, i2c=k10_i2c):
        self._i2c = i2c
        self._address = address
        self._i2c.writeto(self._address, bytearray([0x70,0x65]))
        self._offset = self.average(10)

    def peel(self):
        self._offset = self.average(10)
        self._i2c.writeto(self._address, bytearray([0x73,0x00]))
        return self._offset

    def peel_flag(self):
        self._i2c.writeto(self._address, bytearray([self.REG_DATA_GET_PEEL_FLAG]))
        data = self._i2c.readfrom(self._address,1)
        if(data[0] == 0x01 or data[0] == 129):
            return 1
        elif data[0] == 0x02:
            return 2
        else:
            return 0
    def set_calibration(self ,value):
      '''!
        @fn set_calibration
        @brief Set calibration value
        @param value the calibration value
      '''
      self._offset = self.average(15)
      self._calibration = value

    def get_calibration(self):
        '''!
            @fn get_calibration
            @brief get calibration value 
            @return return the read calibration value
        '''
        self._i2c.writeto(self._address, bytearray([self.REG_DATA_GET_CALIBRATION]))
        data = self._i2c.read_reg(self._address,4)
        aa= bytearray(data) 
        return struct.unpack('>f', aa)
    
    def average(self,times):
        sum = 0
        for i in range(times):
            #
            data = self.get_value()
            if data == 0 :
                times = times -1
            else:
                sum = sum + data
        if(times == 0):
            times =1
        return  sum/times
    def get_value(self):
        self._i2c.writeto(self._address, bytearray([0x66]))
        time.sleep(0.022)
        data = self._i2c.readfrom(0x64, 4)
        value = 0
        if(data[0] == 0x12):
            value = (data[1]<<16) | (data[2]<<8) | data[3]
        else:
            return 0
        return value^0x800000
    
    def read_weight(self,times):
        '''!
        @fn read_weight
        @brief Get the weight of the object
        @param times Take the average several times
        @return return the read weight value, unit: g
        '''
        value = self.average(times)
        time.sleep(0.05)
        ppFlag = self.peel_flag()
        if ppFlag == 1:
            self._offset = self.average(times)
        elif ppFlag == 2:
            b = self.get_calibration()
            self._calibration = b[0]
        return ((value - self._offset)/self._calibration) 
    
    def common_measure(self):
        return self.read_weight(10)    

'''
重力传感器
'''
class force(object):
    def __init__(self, sda=20, scl=19):
        self._zero_scale = 0
        _sda = pins_remap_k10[sda]
        _scl = pins_remap_k10[scl]
        if(sda==20 or scl==19):
            self.i2c = k10_i2c
            self.dev = hx711(address=0x64, i2c=self.i2c)
        else:
            self.i2c = I2C(scl=Pin(_scl), sda=Pin(_sda), freq=400000)
            time.sleep_ms(100)
            self.dev = hx711(address=0x64, i2c=self.i2c)

    def zero(self):
        self._zero_scale = self.dev.peel()

    def read(self,mass=True):
        tmp = self.dev.common_measure()
        if(tmp!=None):
            if(mass):
                # g  
                return round(tmp,2)
            else:
                #1000克(g)受到9.80665牛顿(N)
                m = round((tmp*9.80665)/1000,2)
                return round(m,2)
        else:
            return None

'''
外置超声波驱动类
'''
class sen0388(object):
    def __init__(self, d_pin, echo_timeout_us=500*2*30):
        """
        trigger_pin: Output pin to send pulses
        echo_pin: Readonly pin to measure the distance. The pin should be protected with 1k resistor
        echo_timeout_us: Timeout in microseconds to listen to echo pin. 
        By default is based in sensor limit range (4m)
        """
        self.echo_timeout_us = echo_timeout_us
        self._pin = d_pin
        # Init trigger pin (out)
        self.trigger = Pin(self._pin, mode=Pin.OUT, pull=None)
        self.trigger.value(0)

        # Init echo pin (in)
        #self.echo = Pin(echo_pin, mode=Pin.IN, pull=None)
        self.__limit = 500    #cm

    def _send_pulse_and_wait(self):
        self.trigger = Pin(self._pin, mode=Pin.OUT, pull=None)
        self.trigger.value(0) # Stabilize the sensor
        time.sleep_us(5)
        self.trigger.value(1)
        # Send a 10us pulse.
        time.sleep_us(10)
        self.trigger.value(0)
        # try:
        self.echo = Pin(self._pin, mode=Pin.IN, pull=None)
        pulse_time = machine.time_pulse_us(self.echo, 1, self.echo_timeout_us)
        return pulse_time

    def distance_mm(self):
        """
        Get the distance in milimeters without floating point operations.
        """
        pulse_time = self._send_pulse_and_wait()

        # To calculate the distance we get the pulse_time and divide it by 2 
        # (the pulse walk the distance twice) and by 29.1 becasue
        # the sound speed on air (343.2 m/s), that It's equivalent to
        # 0.34320 mm/us that is 1mm each 2.91us
        # pulse_time // 2 // 2.91 -> pulse_time // 5.82 -> pulse_time * 100 // 582 
        if pulse_time != (-1 or -2):
            mm = pulse_time * 100 // 582
            return mm
        else:
            return self.__limit*10

    def distance_cm(self):
        """
        Get the distance in centimeters with floating point operations.
        It returns a float
        """
        pulse_time = self._send_pulse_and_wait()

        # To calculate the distance we get the pulse_time and divide it by 2 
        # (the pulse walk the distance twice) and by 29.1 becasue
        # the sound speed on air (343.2 m/s), that It's equivalent to
        # 0.034320 cm/us that is 1cm each 29.1us
        if pulse_time != (-1 or -2):
            cms = (pulse_time / 2) / 29.1
            return cms
        else:
            return self.__limit

class ultrasonic(HCSR04):
    def __init__(self, trig=1, echo=0):
        self._trig = pins_remap_k10[trig]
        self._echo = pins_remap_k10[echo]
        if(trig != echo):
            self.dev = HCSR04(trigger_pin=self._trig, echo_pin=self._echo)
        else:
            #使用SEN0388
            self.dev = sen0388(d_pin=self._trig)


    def distance(self):
        return self.dev.distance_cm()

'''
外置DHT11/DHT22温湿度传感器驱动类
'''
class dht(object):
    def __init__(self, pin, sensor_type='AUTO'):
        """初始化DHT传感器

        Args:
            pin: 传感器连接的引脚
            sensor_type: 'AUTO' / 'DHT11' / 'DHT22'
        """
        self._pin = pins_remap_k10[pin]
        self._sensor_type = sensor_type.upper()
        if self._sensor_type not in ('AUTO', 'DHT11', 'DHT22'):
            raise ValueError("sensor_type must be 'AUTO', 'DHT11' or 'DHT22'")

        if self._sensor_type == 'AUTO':
            # AUTO 模式优先探测 DHT22，失败再回退 DHT11。
            self._sensor_candidates = ['DHT22', 'DHT11']
        elif self._sensor_type == 'DHT11':
            self._sensor_candidates = ['DHT11', 'DHT22']
        else:
            self._sensor_candidates = ['DHT22', 'DHT11']

        self._active_index = 0
        self._active_type = self._sensor_candidates[self._active_index]
        self._dht = self._create_sensor(self._active_type)
        self._last_measure_ts = 0.0
        self._auto_locked = (self._sensor_type != 'AUTO')
        if self._sensor_type == 'AUTO':
            self._auto_probe_sensor()

    def _create_sensor(self, sensor_type):
        pin_obj = Pin(self._pin)
        if sensor_type == 'DHT11':
            return DHT11(pin_obj)
        return DHT22(pin_obj)

    def _switch_sensor(self):
        self._active_index = (self._active_index + 1) % len(self._sensor_candidates)
        self._active_type = self._sensor_candidates[self._active_index]
        self._dht = self._create_sensor(self._active_type)
        # 切换型号后重置节流计时，避免沿用上一型号的时间窗。
        self._last_measure_ts = 0.0

    def _auto_probe_sensor(self):
        """启动时探测传感器型号，成功后锁定。"""
        import time
        # DHT22 在上电后通常需要更长稳定时间，避免首次误判为 DHT11。
        time.sleep(2.2)
        for idx, sensor_type in enumerate(self._sensor_candidates):
            self._active_index = idx
            self._active_type = sensor_type
            self._dht = self._create_sensor(sensor_type)
            self._last_measure_ts = 0.0
            min_interval = 2.1 if sensor_type == 'DHT22' else 1.1
            # 每个型号尝试三次，并严格满足该型号的最小采样间隔。
            for _ in range(3):
                try:
                    temp, hum = self._measure_with_interval(0)
                    if not self._validate_reading(temp, hum):
                        raise OSError(116)
                    self._auto_locked = True
                    return
                except OSError:
                    time.sleep(min_interval)
                    continue
        # 探测失败则保持未锁定，后续 read() 继续自动切换重试。
        self._auto_locked = False

    def _measure_with_interval(self, retry_delay):
        import time
        min_interval = 2.0 if self._active_type == 'DHT22' else 1.0
        now = time.time()
        wait_s = min_interval - (now - self._last_measure_ts)
        if wait_s > 0:
            time.sleep(wait_s)
        if retry_delay > 0:
            time.sleep(retry_delay)
        self._dht.measure()
        self._last_measure_ts = time.time()
        temp = self._dht.temperature()
        hum = self._dht.humidity()
        if hum is None or temp is None:
            raise OSError(116)
        return temp, hum

    def _validate_reading(self, temp, hum):
        """校验读数是否在合理范围内，过滤错误型号解析造成的异常值。"""
        if hum < 0 or hum > 100:
            return False
        if temp < -40 or temp > 85:
            return False
        # DHT11 为整数分辨率，AUTO 探测阶段遇到小数值通常表示型号不匹配。
        if self._active_type == 'DHT11' and self._sensor_type == 'AUTO':
            if temp != int(temp) or hum != int(hum):
                return False
        return True

    def read(self, max_retries=8, retry_delay=0.5):
        """读取温湿度数据，带重试和自动传感器类型切换。"""
        last_error = None
        for attempt in range(max_retries):
            try:
                # 仅在重试时等待 retry_delay，避免每次成功读数都多睡一轮
                delay = retry_delay if attempt else 0
                temp, hum = self._measure_with_interval(delay)
                if self._sensor_type == 'AUTO' and not self._validate_reading(temp, hum):
                    raise OSError(116)
                # AUTO 模式首次成功后锁定传感器类型，避免每次失败时来回切换。
                if self._sensor_type == 'AUTO':
                    self._auto_locked = True
                return temp, hum
            except OSError as e:
                last_error = e
                # 超时或读取失败时，自动尝试另一种型号（AUTO 且尚未锁定时）
                if (not self._auto_locked) and len(self._sensor_candidates) > 1 and (attempt % 2 == 0):
                    self._switch_sensor()
                if attempt < max_retries - 1:
                    continue

        raise last_error

    def get_sensor_type(self):
        """返回当前生效的传感器类型。"""
        return self._active_type

light = Light()

speaker = Speaker()
screen = Screen()
acce = accelerometer()
rgb = rgb_board()
mic = Mic()
#camera = Camera()
#wifi = WiFi()
#mqttclient = MqttClient()
#tf_card = TF_card()

#th = task_handler.TaskHandler()

def unihiker_k10_collect():
    global tf_card,speaker
    speaker.deinit()
    tf_card.deinit()
    del mic,speaker,tf_card
    gc.collect()
    print("unihiker_k10_collect")