# 事件驱动摄像头猫脸识别

## 概述

`show_camera_with_cat_detect_simple` 方法已从轮询模式改为事件驱动模式，提供更好的性能和可扩展性。

## 主要改进

### 1. 事件驱动架构
- **事件队列**: 使用事件队列替代轮询，减少CPU占用
- **事件处理器**: 模块化的事件处理器，易于扩展和自定义
- **异步处理**: 摄像头数据获取、AI检测、UI更新异步进行

### 2. 事件类型
- `EVENT_CAMERA_FRAME`: 摄像头帧事件
- `EVENT_AI_DETECT`: AI检测事件
- `EVENT_UI_UPDATE`: UI更新事件
- `EVENT_STOP`: 停止事件

### 3. 线程安全
- 使用线程锁保护共享资源
- 事件队列线程安全操作
- 帧数据访问线程安全

## 使用方法

### 基本使用
```python
from k10_base import Screen, Camera

# 初始化
screen = Screen()
screen.init()
camera = Camera()
camera.init()

# 启动事件驱动猫脸识别
screen.show_camera_with_cat_detect_simple(camera, ai_interval=5)

# 停止
screen.stop_cat_detect_simple()
```

### 自定义事件处理器
```python
# 自定义AI检测处理器
def custom_ai_handler(data):
    print("自定义AI检测逻辑")
    # 调用原始处理器
    screen._handle_ai_detect(data)
    # 添加自定义逻辑

# 替换事件处理器
screen._event_handlers[screen.EVENT_AI_DETECT] = custom_ai_handler
```

### 发送自定义事件
```python
# 发送自定义事件
screen._post_event("custom_event", {
    'message': '自定义事件数据',
    'timestamp': time.time()
})

# 添加自定义事件处理器
def handle_custom_event(data):
    print(f"处理自定义事件: {data}")

screen._event_handlers["custom_event"] = handle_custom_event
```

## 架构对比

### 轮询模式（旧版本）
```
摄像头线程 → 轮询摄像头数据 → 处理数据 → 更新UI
    ↓
定时器驱动，固定频率检查
```

### 事件驱动模式（新版本）
```
摄像头事件生成器 → 事件队列 → 事件循环 → 事件处理器
    ↓                    ↓           ↓           ↓
摄像头帧事件        线程安全队列   异步处理    模块化处理器
AI检测事件
UI更新事件
停止事件
```

## 性能优势

1. **降低CPU占用**: 事件驱动减少不必要的轮询
2. **更好的响应性**: 事件立即处理，无需等待轮询周期
3. **可扩展性**: 易于添加新的事件类型和处理器
4. **模块化**: 各功能模块独立，便于维护和调试

## 事件处理器方法

### `_handle_camera_frame(data)`
处理摄像头帧事件
- `data['frame_data']`: 摄像头帧数据
- `data['frame_count']`: 帧计数

### `_handle_ai_detect(data)`
处理AI检测事件
- `data['frame_data']`: 待检测的帧数据
- `data['ai_frame_count']`: AI检测计数

### `_handle_ui_update(data)`
处理UI更新事件
- 更新屏幕显示
- 显示检测状态

### `_handle_stop(data)`
处理停止事件
- 停止所有处理线程
- 清理资源

## 兼容性

- 保持原有的 `close_cat_detect_simple()` 方法兼容性
- 新增 `stop_cat_detect_simple()` 方法用于事件驱动版本
- 原有API调用方式不变

## 示例代码

完整示例请参考 `examples/event_driven_camera_demo.py`

## 注意事项

1. 确保 `_thread` 模块可用
2. 事件处理器中的异常会被捕获并记录
3. 停止时使用 `stop_cat_detect_simple()` 方法
4. 自定义事件处理器应保持线程安全 