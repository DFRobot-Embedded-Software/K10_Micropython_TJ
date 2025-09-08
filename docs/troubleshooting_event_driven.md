# 事件驱动摄像头故障排除指南

## 常见问题及解决方案

### 1. SD卡检测错误

**问题描述：**
```
SD card not detected
```

**可能原因：**
- SD卡未正确插入
- SD卡损坏
- 硬件连接问题

**解决方案：**
1. 检查SD卡是否正确插入
2. 尝试重新格式化SD卡
3. 使用简化测试脚本避免SD卡相关功能

**临时解决方案：**
```python
# 使用简化测试脚本
import simple_event_driven_test
simple_event_driven_test.main()
```

### 2. 连接错误 (ConnectionError: EOF)

**问题描述：**
```
PROBLEM IN THONNY'S BACK-END: Exception while handling 'Run' (ConnectionError: EOF).
```

**可能原因：**
- 设备重启或连接中断
- 内存不足导致系统崩溃
- 多线程冲突

**解决方案：**
1. 重启MicroPython设备
2. 检查可用内存
3. 使用更保守的线程设置

**内存检查：**
```python
import gc
print(f"可用内存: {gc.mem_free()} bytes")
if gc.mem_free() < 50000:
    print("内存不足，建议重启设备")
```

### 3. 摄像头初始化失败

**问题描述：**
```
摄像头初始化失败
```

**解决方案：**
1. 检查摄像头硬件连接
2. 重新初始化摄像头
3. 使用基本摄像头测试

**基本测试：**
```python
from k10_base import Camera

camera = Camera()
try:
    camera.init()
    print("摄像头初始化成功")
    
    # 测试摄像头捕获
    buf = camera.capture()
    if buf:
        print(f"摄像头捕获成功，数据长度: {len(buf)}")
    else:
        print("摄像头捕获失败")
        
except Exception as e:
    print(f"摄像头初始化失败: {e}")
```

### 4. 事件驱动系统崩溃

**问题描述：**
```
事件循环错误
摄像头事件生成器错误
```

**解决方案：**
1. 增加错误处理
2. 减少线程数量
3. 增加休眠时间

**调试模式：**
```python
# 在事件驱动方法中添加更多调试信息
def show_camera_with_cat_detect_simple(self, camera, ai_interval=5):
    print("=== 调试模式 ===")
    print(f"摄像头对象: {camera}")
    print(f"AI间隔: {ai_interval}")
    
    # 检查内存
    import gc
    print(f"初始内存: {gc.mem_free()} bytes")
    
    # 原有代码...
```

### 5. 内存不足

**问题描述：**
```
内存不足，系统不稳定
```

**解决方案：**
1. 强制垃圾回收
2. 减少事件队列大小
3. 增加休眠时间

**内存优化：**
```python
import gc

# 强制垃圾回收
gc.collect()

# 检查内存
free_memory = gc.mem_free()
print(f"可用内存: {free_memory} bytes")

if free_memory < 30000:
    print("内存不足，建议重启设备")
```

### 6. 线程冲突

**问题描述：**
```
线程锁错误
线程创建失败
```

**解决方案：**
1. 检查线程锁使用
2. 减少并发线程数量
3. 增加线程间休眠时间

**线程安全检查：**
```python
import _thread

# 测试线程功能
def test_thread():
    print("测试线程运行")
    time.sleep(1)
    print("测试线程完成")

try:
    _thread.start_new_thread(test_thread, ())
    print("线程创建成功")
except Exception as e:
    print(f"线程创建失败: {e}")
```

## 调试步骤

### 步骤1：基础检查
```python
# 检查基本功能
import gc
import _thread
from k10_base import Screen, Camera

print(f"可用内存: {gc.mem_free()} bytes")
print("基础模块导入成功")
```

### 步骤2：硬件检查
```python
# 检查屏幕和摄像头
screen = Screen()
screen.init()
print("屏幕初始化成功")

camera = Camera()
camera.init()
print("摄像头初始化成功")
```

### 步骤3：简化测试
```python
# 使用简化测试脚本
import simple_event_driven_test
simple_event_driven_test.simple_test()
```

### 步骤4：完整测试
```python
# 如果简化测试通过，尝试完整功能
from k10_base import Screen, Camera

screen = Screen()
screen.init()
camera = Camera()
camera.init()

screen.show_camera_with_cat_detect_simple(camera, ai_interval=5)
time.sleep(10)
screen.stop_cat_detect_simple()
```

## 性能优化建议

### 1. 内存优化
- 定期执行垃圾回收
- 限制事件队列大小
- 及时清理不需要的对象

### 2. 线程优化
- 减少线程数量
- 增加线程间休眠时间
- 使用线程锁保护共享资源

### 3. 摄像头优化
- 降低摄像头帧率
- 增加AI检测间隔
- 优化图像处理算法

## 联系支持

如果问题仍然存在，请提供以下信息：
1. 错误信息完整日志
2. 设备型号和固件版本
3. 可用内存大小
4. 使用的测试脚本

## 预防措施

1. **定期重启设备**：避免内存泄漏
2. **监控内存使用**：及时发现问题
3. **使用简化测试**：先确保基础功能正常
4. **备份重要数据**：避免数据丢失 