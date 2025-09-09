#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Test script to verify ili9XXX.py imports work correctly

print("Testing ili9XXX.py imports...")

try:
    from ili9XXX import ili9341, ili9488, gc9a01, st7789, st7735
    print("✓ Successfully imported all display classes:")
    print("  - ili9341")
    print("  - ili9488") 
    print("  - gc9a01")
    print("  - st7789")
    print("  - st7735")
    
    # Test creating an instance
    print("\nTesting ili9341 instance creation...")
    display = ili9341()
    print("✓ Successfully created ili9341 instance")
    
except ImportError as e:
    print(f"✗ ImportError: {e}")
except Exception as e:
    print(f"✗ Error: {e}")
    import traceback
    traceback.print_exc() 