#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
事件驱动摄像头猫脸识别测试
测试事件驱动版本的功能是否正常
"""

import time
import gc
from k10_base import Screen, Camera

def test_event_driven_basic():
    """测试基本事件驱动功能"""
    print("=== 测试基本事件驱动功能 ===")
    
    try:
        # 初始化
        screen = Screen()
        screen.init()
        camera = Camera()
        camera.init()
        
        print("✅ 初始化成功")
        
        # 测试事件驱动启动
        screen.show_camera_with_cat_detect_simple(camera, ai_interval=3)
        print("✅ 事件驱动启动成功")
        
        # 运行一段时间
        time.sleep(5)
        
        # 测试停止功能
        screen.stop_cat_detect_simple()
        print("✅ 停止功能正常")
        
        return True
        
    except Exception as e:
        print(f"❌ 基本功能测试失败: {e}")
        return False

def test_event_handlers():
    """测试事件处理器"""
    print("=== 测试事件处理器 ===")
    
    try:
        screen = Screen()
        screen.init()
        camera = Camera()
        camera.init()
        
        # 测试事件处理器是否存在
        assert hasattr(screen, '_event_handlers'), "事件处理器字典不存在"
        assert hasattr(screen, '_handle_camera_frame'), "摄像头帧处理器不存在"
        assert hasattr(screen, '_handle_ai_detect'), "AI检测处理器不存在"
        assert hasattr(screen, '_handle_ui_update'), "UI更新处理器不存在"
        assert hasattr(screen, '_handle_stop'), "停止处理器不存在"
        
        print("✅ 事件处理器存在")
        
        # 测试事件类型定义
        assert hasattr(screen, 'EVENT_CAMERA_FRAME'), "摄像头帧事件类型不存在"
        assert hasattr(screen, 'EVENT_AI_DETECT'), "AI检测事件类型不存在"
        assert hasattr(screen, 'EVENT_UI_UPDATE'), "UI更新事件类型不存在"
        assert hasattr(screen, 'EVENT_STOP'), "停止事件类型不存在"
        
        print("✅ 事件类型定义正确")
        
        return True
        
    except Exception as e:
        print(f"❌ 事件处理器测试失败: {e}")
        return False

def test_event_queue():
    """测试事件队列"""
    print("=== 测试事件队列 ===")
    
    try:
        screen = Screen()
        screen.init()
        
        # 测试事件队列初始化
        assert hasattr(screen, '_event_queue'), "事件队列不存在"
        assert hasattr(screen, '_event_lock'), "事件锁不存在"
        assert hasattr(screen, '_post_event'), "事件发送方法不存在"
        
        print("✅ 事件队列初始化正确")
        
        # 测试事件发送
        test_data = {'test': 'data'}
        screen._post_event("test_event", test_data)
        
        # 检查事件是否在队列中
        with screen._event_lock:
            assert len(screen._event_queue) > 0, "事件未成功添加到队列"
            event = screen._event_queue[0]
            assert event['type'] == "test_event", "事件类型不正确"
            assert event['data'] == test_data, "事件数据不正确"
        
        print("✅ 事件发送功能正常")
        
        return True
        
    except Exception as e:
        print(f"❌ 事件队列测试失败: {e}")
        return False

def test_custom_event_handler():
    """测试自定义事件处理器"""
    print("=== 测试自定义事件处理器 ===")
    
    try:
        screen = Screen()
        screen.init()
        
        # 创建测试标志
        test_flag = {'called': False}
        
        # 自定义事件处理器
        def test_handler(data):
            test_flag['called'] = True
            test_flag['data'] = data
        
        # 注册自定义处理器
        screen._event_handlers["test_custom_event"] = test_handler
        
        # 发送测试事件
        test_data = {'message': 'test'}
        screen._post_event("test_custom_event", test_data)
        
        # 手动触发事件处理（模拟事件循环）
        with screen._event_lock:
            if screen._event_queue:
                event = screen._event_queue.pop(0)
                event_type = event['type']
                event_data = event['data']
                
                if event_type in screen._event_handlers:
                    screen._event_handlers[event_type](event_data)
        
        # 检查处理器是否被调用
        assert test_flag['called'], "自定义事件处理器未被调用"
        assert test_flag['data'] == test_data, "事件数据传递不正确"
        
        print("✅ 自定义事件处理器功能正常")
        
        return True
        
    except Exception as e:
        print(f"❌ 自定义事件处理器测试失败: {e}")
        return False

def test_thread_safety():
    """测试线程安全"""
    print("=== 测试线程安全 ===")
    
    try:
        screen = Screen()
        screen.init()
        
        # 测试锁的存在
        assert hasattr(screen, '_event_lock'), "事件锁不存在"
        assert hasattr(screen, '_frame_lock'), "帧锁不存在"
        
        # 测试锁的基本功能
        with screen._event_lock:
            # 在锁内执行操作
            screen._event_queue.append({'type': 'test', 'data': {}})
        
        print("✅ 线程锁功能正常")
        
        return True
        
    except Exception as e:
        print(f"❌ 线程安全测试失败: {e}")
        return False

def test_memory_cleanup():
    """测试内存清理"""
    print("=== 测试内存清理 ===")
    
    try:
        screen = Screen()
        screen.init()
        camera = Camera()
        camera.init()
        
        # 记录初始内存
        initial_memory = gc.mem_free()
        
        # 启动和停止多次
        for i in range(3):
            screen.show_camera_with_cat_detect_simple(camera, ai_interval=3)
            time.sleep(1)
            screen.stop_cat_detect_simple()
            time.sleep(0.5)
        
        # 强制垃圾回收
        gc.collect()
        
        # 检查内存是否合理
        final_memory = gc.mem_free()
        memory_diff = initial_memory - final_memory
        
        print(f"内存变化: {memory_diff} bytes")
        
        # 内存变化应该在合理范围内（不应该泄漏太多）
        if memory_diff < 10000:  # 允许10KB的内存差异
            print("✅ 内存清理正常")
            return True
        else:
            print(f"⚠️ 内存变化较大: {memory_diff} bytes")
            return False
        
    except Exception as e:
        print(f"❌ 内存清理测试失败: {e}")
        return False

def run_all_tests():
    """运行所有测试"""
    print("开始事件驱动摄像头功能测试...")
    
    tests = [
        test_event_handlers,
        test_event_queue,
        test_custom_event_handler,
        test_thread_safety,
        test_event_driven_basic,
        test_memory_cleanup
    ]
    
    passed = 0
    total = len(tests)
    
    for test in tests:
        try:
            if test():
                passed += 1
            print()  # 空行分隔
        except Exception as e:
            print(f"❌ 测试异常: {e}")
            print()
    
    print(f"测试结果: {passed}/{total} 通过")
    
    if passed == total:
        print("🎉 所有测试通过！")
    else:
        print("⚠️ 部分测试失败")
    
    return passed == total

if __name__ == "__main__":
    run_all_tests() 