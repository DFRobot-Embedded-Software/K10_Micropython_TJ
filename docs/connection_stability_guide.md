# 连接稳定性指南

## 解决 ConnectionError: EOF 问题

### 问题分析

`ConnectionError: EOF` 通常表示：
1. **内存不足** - 系统崩溃导致连接中断
2. **CPU过载** - 多线程竞争导致系统不稳定
3. **资源泄漏** - 长时间运行导致内存泄漏

### 解决方案

#### 1. 使用轻量级版本

```python
# 使用轻量级事件驱动版本
from k10_base import Screen, Camera

screen = Screen()
screen.init()
camera = Camera()
camera.init()

# 使用更大的AI间隔减少负载
screen.show_camera_with_cat_detect_simple(camera, ai_interval=10)
```

#### 2. 内存管理

```python
import gc

# 检查内存
free_memory = gc.mem_free()
print(f"可用内存: {free_memory} bytes")

if free_memory < 30000:
    print("内存不足，执行垃圾回收")
    gc.collect()

# 强制垃圾回收
gc.collect()
```

#### 3. 分步测试

```python
# 步骤1：基础硬件测试
def test_hardware():
    screen = Screen()
    screen.init()
    camera = Camera()
    camera.init()
    
    # 测试摄像头捕获
    buf = camera.capture()
    if buf:
        print("硬件正常")
        return True
    return False

# 步骤2：轻量级测试
def test_lightweight():
    if test_hardware():
        screen = Screen()
        screen.init()
        camera = Camera()
        camera.init()
        
        # 使用轻量级设置
        screen.show_camera_with_cat_detect_simple(camera, ai_interval=15)
        time.sleep(5)  # 只运行5秒
        screen.stop_cat_detect_simple()
```

### 预防措施

#### 1. 设备重启
```python
# 在运行前重启设备
import machine
machine.reset()
```

#### 2. 内存监控
```python
def monitor_memory():
    import gc
    while True:
        free_memory = gc.mem_free()
        print(f"内存: {free_memory} bytes")
        
        if free_memory < 20000:
            print("内存不足，执行垃圾回收")
            gc.collect()
        
        time.sleep(1)
```

#### 3. 保守设置
```python
# 使用保守的设置
screen.show_camera_with_cat_detect_simple(
    camera, 
    ai_interval=20  # 更大的AI间隔
)
```

### 调试步骤

#### 步骤1：检查内存
```python
import gc
print(f"可用内存: {gc.mem_free()} bytes")
gc.collect()
print(f"垃圾回收后内存: {gc.mem_free()} bytes")
```

#### 步骤2：测试硬件
```python
from k10_base import Screen, Camera

# 测试屏幕
screen = Screen()
screen.init()
print("屏幕正常")

# 测试摄像头
camera = Camera()
camera.init()
buf = camera.capture()
if buf:
    print("摄像头正常")
else:
    print("摄像头异常")
```

#### 步骤3：轻量级测试
```python
# 使用轻量级测试脚本
import lightweight_event_test
lightweight_event_test.quick_test()
```

#### 步骤4：完整测试
```python
# 如果轻量级测试通过，尝试完整功能
import lightweight_event_test
lightweight_event_test.main()
```

### 性能优化建议

#### 1. 减少线程数量
- 使用更少的并发线程
- 增加线程间休眠时间
- 使用线程锁保护共享资源

#### 2. 优化内存使用
- 限制事件队列大小
- 及时清理不需要的对象
- 定期执行垃圾回收

#### 3. 降低处理频率
- 增加AI检测间隔
- 降低摄像头帧率
- 减少UI更新频率

### 错误恢复

#### 1. 自动重启
```python
def auto_restart_on_error():
    try:
        # 你的代码
        pass
    except Exception as e:
        print(f"错误: {e}")
        print("准备重启...")
        import machine
        machine.reset()
```

#### 2. 错误监控
```python
def error_monitor():
    import gc
    while True:
        try:
            free_memory = gc.mem_free()
            if free_memory < 15000:
                print("内存严重不足，重启设备")
                import machine
                machine.reset()
            time.sleep(5)
        except:
            pass
```

### 最佳实践

1. **启动前检查内存**
2. **使用轻量级设置**
3. **定期监控系统状态**
4. **及时停止和清理资源**
5. **使用分步测试验证功能**

### 联系支持

如果问题持续存在：
1. 提供完整的错误日志
2. 记录内存使用情况
3. 描述测试步骤
4. 提供设备型号和固件版本 