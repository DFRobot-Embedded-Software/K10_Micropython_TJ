      
# -*- coding: utf-8 -*
"""
    I2C PN7150近场通讯NFC模块
"""
import time
from machine import Pin, I2C

class Rfid(object):
    """!
    @brief Define rfid basic class
    """
    # MT=1 GID=0 OID=0 PL=1 ResetType=1 (Reset Configuration)
    NCI_CORE_RESET_CMD = b"\x20\x00\x01\x01"
    # MT=1 GID=0 OID=1 PL=0
    NCI_CORE_INIT_CMD = b"\x20\x01\x00"
    # MT=1 GID=f OID=2 PL=0
    NCI_PROP_ACT_CMD = b"\x2f\x02\x00"
    # MT=1 GID=1 OID=0
    NCI_RF_DISCOVER_MAP_RW = (
        b"\x21\x00\x10\x05\x01\x01\x01\x02\x01\x01\x03\x01\x01\x04\x01\x02\x80\x01\x80"
    )
    # MT=1 GID=1 OID=3
    NCI_RF_DISCOVER_CMD_RW = b"\x21\x03\x09\x04\x00\x01\x02\x01\x01\x01\x06\x01"
    # MODE_POLL | TECH_PASSIVE_NFCA,
    # MODE_POLL | TECH_PASSIVE_NFCF,
    # MODE_POLL | TECH_PASSIVE_NFCB,
    # MODE_POLL | TECH_PASSIVE_15693,
    NCI_RF_DEACTIVATE_CMD = b"\x21\x06\x01\x00"

    PROT_UNDETERMINED = 0x0
    PROT_T1T = 0x1
    PROT_T2T = 0x2
    PROT_T3T = 0x3
    PROT_ISODEP = 0x4
    PROT_NFCDEP = 0x5
    PROT_T5T = 0x6
    PROT_MIFARE = 0x80
    PN7150_I2C_ADDR = 0x28

    _STATUS_OK = 0x00
    def __init__(self, i2c,serial_number=0):
        """!
        @brief Module I2C communication init
        @param i2c_addr - I2C communication address
        @param bus_num - I2C bus
        """
        self._addr = self.PN7150_I2C_ADDR
        self._i2c = i2c
        self._debug = False
        self._buf = bytearray(3 + 255)
        self.fw_version = self._buf[64]
        self.nfc_uid = [0]
        self.nfc_serial_number = serial_number
        self.nfc_protocol = 0
        self.block_data = [0 for i in range(16)]

    def connect(self):
        """!
        @brief Function connect.
        @return Boolean type, the result of operation
        """
        try:
            self._connect()
            ret = self._mode_rw()
        finally:
            # print("finally connect")
            pass
        return ret

    def read_block(self, block, index=None):
        """!
        @brief Read a byte from a specified block of a MIFARE Classic NFC smart card/tag.
        @param block - The number of the block to read from.
        @param index - The offset of the block.
        @return Read from the card.
        """
        data = self._read_data(block)
        if (
            data == "no card!"
            or data == "read error!"
            or data == "read timeout!"
            or data == "wake up error!"
            or data == "false"
        ):
            return None
        if index is None:
            return data
        else:
            return self.block_data[index - 1]

    def write_block(self, block,data,index=0,):
        """!
        @brief Write a byte to a MIFARE Classic NFC smart card/tag.
        @param block - The number of pages you want to writes the data.
        @param index - The offset of the data.
        @param data - The byte to be written.
        @return Boolean type, the result of operation
        """
        if isinstance(data, str):
            real_val = []
            for i in data:
                real_val.append(int(ord(i)))
            if len(real_val) < 16:
                for i in range(16 - len(real_val)):
                    real_val.append(0)
            elif len(real_val) > 16:
                return False
        if isinstance(data, list):
            real_val = []
            if len(data) < 16:
                for i in range(16 - len(data)):
                    data.append(0)
            elif len(data) > 16:
                return False
            real_val = data
        index = max(min(index, 16), 1)
        self.read_block(block)
        if isinstance(data, int):
            self.block_data[index - 1] = data
            self.write_data(block, self.block_data)
        else:
            block_data = [0 for i in range(index - 1)]
            block_data[index:] = real_val
            self.write_data(block, block_data)
        return True

    def write_data(self, block, data):
        """!
        @brief Write a block to a MIFARE Classic NFC smart card/tag.
        @param block - The number of the block to write to.
        @param data - The buffer of the data to be written.
        @return Boolean type, the result of operation
        """
        if isinstance(data, tuple):
            data = list(data)
        if isinstance(data, str):
            real_val = []
            for i in data:
                real_val.append(int(ord(i)))
            if len(real_val) < 16:
                for i in range(16 - len(real_val)):
                    real_val.append(0)
            elif len(real_val) > 16:
                return False
        if isinstance(data, list):
            real_val = []
            if len(data) < 16:
                for i in range(16 - len(data)):
                    data.append(0)
            elif len(data) > 16:
                return False
            real_val = data
        if block < 128 and ((block + 1) % 4 == 0 or block == 0):
            return False
        if block > 127 and block < 256 and (block + 1) % 16 == 0:
            return False
        if block > 255:
            return False
        if not self._scan() or self.scan_serial_num!=self.nfc_serial_number:
            return False
        cmd_auth = [0x40, int(block / 4), 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]
        resp = self._tag_cmd(cmd_auth)
        if 0 == len(resp) or 0 != resp[-1]:
            return False
        cmd_write_1 = [0x10, 0xA0, block]
        resp = self._tag_cmd(cmd_write_1)
        if 0 == len(resp) or 0 != resp[-1]:
            return False
        cmd_write_2 = [0x10]
        cmd_write_2[1:] = real_val
        resp = self._tag_cmd(cmd_write_2)
        if 0 == len(resp) or 0 != resp[-1]:
            return False
        return True

    def read_protocol(self):
        if not self._scan():
            return "no card!"
        if self.nfc_protocol == self.PROT_T2T:
            return "T2T"
        elif self.nfc_protocol == self.PROT_UNDETERMINED:
            return "undetermined"
        elif self.nfc_protocol == self.PROT_T1T:
            return "T1T"
        elif self.nfc_protocol == self.PROT_T3T:
            return "T3T"
        elif self.nfc_protocol == self.PROT_ISODEP:
            return "isodep"
        elif self.nfc_protocol == self.PROT_NFCDEP:
            return "nfcdep"
        elif self.nfc_protocol == self.PROT_T5T:
            return "T5T"
        elif self.nfc_protocol == self.PROT_MIFARE:
            return "mifare"
        else:
            return "Unknow"
                
    def _mode_rw(self):
        """!
        @brief Function mode Read/Write.
        @return Boolean type, the result of operation
        """
        self._write_block(self.NCI_RF_DISCOVER_MAP_RW)
        end = self._read_wait(10)
        return (
            end >= 4
            and self._buf[0] == 0x41
            and self._buf[1] == 0x00
            and self._buf[3] == self._STATUS_OK
        )

    def _start_discovery_rw(self):
        """!
        @brief Function Start Discovery Read/Write.
        @return Boolean type, the result of operation
        """
        self._write_block(self.NCI_RF_DISCOVER_CMD_RW)
        end = self._read_wait()
        return (
            end >= 4
            and self._buf[0] == 0x41
            and self._buf[1] == 0x03
            and self._buf[3] == self._STATUS_OK
        )

    def _stop_discovery(self):
        """!
        @brief Function stop Discovery.
        @return Boolean type, the result of operation
        """
        self._write_block(self.NCI_RF_DEACTIVATE_CMD)
        end = self._read_wait()
        return (
            end >= 4
            and self._buf[0] == 0x41
            and self._buf[1] == 0x06
            and self._buf[3] == self._STATUS_OK
        )

    def _tag_cmd(self, cmd, conn_id=0):
        """!
        @brief Function tag cmd.
        @param cmd - tag cmd
        @param conn_id - conn_id
        @return Data of the nfc module
        """
        self._buf[0] = conn_id
        self._buf[1] = 0x00
        self._buf[2] = len(cmd)
        end = 3 + len(cmd)
        self._buf[3:end] = bytearray(cmd[0:len(cmd)])
        self._write_block(self._buf, end=end)
        base = time.time() * 100
        timeout = 10
        while (time.time() * 100 - base) < timeout:
            end = self._read_wait(30)
            if self._buf[0] & 0xE0 == 0x00:
                break
            time.sleep(0.001)

        return self._buf[3:end]
    
    def read_uid(self):
        """!
        @brief Obtain the UID of the card .
        @return UID of the card.
        """
        if not self._scan():
            return None
        if self.nfc_uid is None:
            return None
        else:
            return "".join([str(hex(u))[2:] for u in self.nfc_uid])
        
    def _scan(self):
        ret= False
        self._stop_discovery()
        try:
            if self._start_discovery_rw():
                end = self._read_wait(30)
                if end == 0 or self._buf[0] != 0x61 or self._buf[1] != 0x05:
                    return False
                self.nfc_uid = self._buf[13:17]
                self.scan_serial_num = int.from_bytes(bytes(self.nfc_uid),'big')
                self.nfc_protocol = self._buf[5]
                ret = True
        except Exception as e:
            print(e)
        return ret

    def _read_data(self, page):
        if page > 255:
            return "false"
        if not self._scan() or self.scan_serial_num!=self.nfc_serial_number:
            return "no card!"
        cmd_auth = [0x40, int(page / 4), 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]
        resp = self._tag_cmd(cmd_auth)
        #print(resp)
        if 0 == len(resp) or 0 != resp[-1]:
            return "read error!"
        cmd_read = [0x10, 0x30, page]
        resp = self._tag_cmd(cmd_read)
        if 0 == len(resp) or 0 != resp[-1]:
            return "read timeout!"
        dataStr = ""
        for i in range(len(resp) - 2):
            self.block_data[i] = resp[i + 1]
            if resp[i + 1] <= 0x0F:
                # dataStr += "0"
                # dataStr += str(hex(resp[i + 1]))
                dataStr += hex(resp[i + 1])[2:].upper()
            else:
                # dataStr += str(hex(resp[i + 1]))
                dataStr += hex(resp[i + 1])[2:].upper()
            if i < len(resp) - 3:
                dataStr += "."
        return dataStr

    def _connect(self):
        """!
        @brief Function connect.
        """
        self._write_block(self.NCI_CORE_RESET_CMD)
        end = self._read_wait(30)
        if (
            end < 6
            or self._buf[0] != 0x40
            or self._buf[1] != 0x00
            or self._buf[3] != self._STATUS_OK
            or self._buf[5] != 0x01
        ):
            return False
        self._write_block(self.NCI_CORE_INIT_CMD)
        end = self._read_wait()
        if (
            end < 20
            or self._buf[0] != 0x40
            or self._buf[1] != 0x01
            or self._buf[3] != self._STATUS_OK
        ):
            return False

        nrf_int = self._buf[8]
        self.fw_version = self._buf[17 + nrf_int : 20 + nrf_int]
        self._write_block(self.NCI_PROP_ACT_CMD)
        end = self._read_wait()
        if (
            end < 4
            or self._buf[0] != 0x4F
            or self._buf[1] != 0x02
            or self._buf[3] != self._STATUS_OK
        ):
            return False
        return True

    def _read_wait(self, timeout=10):
        """!
        @brief read the data from the register
        @param timeout - timeout
        @return read data
        """
        base = time.time() * 100
        while (time.time() * 100 - base) < timeout:
            count = self._read_block()
            #print(count)
            if 3 < count:
                return count
            time.sleep(0.001)
        return 0

    def _read_block(self):
        """!
        @brief read the data from the register
        @return read data
        """
        end = 0 
        try:
            read_msg = self._i2c.readfrom(self._addr, 3)
            #print(f"read_msg1 {read_msg}")
            if None != read_msg:
                self._buf[0:2] = bytearray(read_msg)
                end = 3
                if self._buf[2] > 0:
                    read_msg = self._i2c.readfrom(self._addr, self._buf[2])
                    #print(f"read_msg2 {read_msg}")
                    if len(bytearray(read_msg)) == self._buf[2]:
                        self._buf[3:] = bytearray(read_msg)
                        end = 3 + self._buf[2] 
        except OSError as e:
            #print(f"I/O error: {e}")
            pass
        except Exception as e:
            print("An unexpected error occurred: {}".format(e))
        return end

    def _write_block(self, cmd, end=0):
        """!
        @brief writes data to a register
        @param cmd - written data
        @param end - data len
        """
        cycle_count = 5
        self._read_block()
        if not end:
            end = len(cmd)
        while cycle_count:
            try:
                self._i2c.writeto(self._addr, bytearray(cmd[0:end]))
                break
            except OSError as e:
                #print(f"_write_block I/O error: {e}")
                cycle_count -= 1
                self._read_block()
                time.sleep(0.01)
            except Exception as e:
                print("An unexpected error occurred: {}".format(e))

class Rfid_Edu(Rfid):
    def __init__(self, i2c, serial_number):
        self._serial_number = serial_number
        super().__init__(i2c,serial_number)

    def _get_serNum(self, serial_number):
        serNumCheck = 0
        buf = serial_number.to_bytes(4, 'little')
        for i in range(4):
            serNumCheck ^= buf[i]
        serNum_list = [int(i) for i in buf]
        serNum_list.append(serNumCheck)
        return (tuple(serNum_list))

    def serial_number(self):
        """
        获取序列号
        """
        return str(self._serial_number)


class Scan_Rfid_Edu():
    """扫描Rfid卡类.
    """
    @classmethod
    def scanning(cls, i2c):
        """
        扫描RFID卡,返回Rfid对象

        :param obj i2c: I2C实例对象
        :return: 返回Rfid对象
        """
        rfid_instance = Rfid(i2c)
        if not rfid_instance.connect():
            return None
        if rfid_instance.read_uid():
            serial_tuple = rfid_instance.scan_serial_num
            if serial_tuple:
                return Rfid_Edu(i2c, serial_tuple)
        return None


class rfid(Scan_Rfid_Edu):
    def __init__(self,sda=1,scl=2):
            self.i2c_1 = I2C(0,scl=Pin(scl),sda=Pin(sda),freq=400000)
            time.sleep_ms(100)

    def scanning(self,wait=True):
        rf = None
        if(isinstance(wait, bool)):
            if(wait):
                while True:
                    rf = super().scanning(i2c=self.i2c_1)
                    if rf:
                        return rf
                    else:
                        time.sleep_ms(200)
            else:
                rf = super().scanning(i2c=self.i2c_1)
                if rf:
                    return rf
                else:
                    return None
        elif(isinstance(wait, int)):
            time_start = time.time()
            while True:
                if (int(time.time()-time_start) > wait):
                    return None
                rf = super().scanning(i2c=self.i2c_1)
                if rf:
                    return rf
                else:
                    time.sleep_ms(200)


    