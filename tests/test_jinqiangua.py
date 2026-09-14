#!/usr/bin/env python3
# tests/test_jinqiangua.py
import os
import re
import sys

def test_data_header():
    header_path = os.path.join(os.path.dirname(__file__), "..", "main", "jinqiangua_data.h")
    assert os.path.isfile(header_path), f"File not found: {header_path}"
    with open(header_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Verify 64 items are defined
    numbers = [int(m) for m in re.findall(r"\.number\s*=\s*(\d+)", content)]
    assert len(numbers) == 64, f"Expected 64 items, got {len(numbers)}"
    assert sorted(numbers) == list(range(1, 65)), "Hexagram numbers 1..64 mismatch"

    codes = [int(m, 16) for m in re.findall(r"\.code\s*=\s*0x([0-9A-Fa-f]+)", content)]
    assert len(codes) == 64, f"Expected 64 codes, got {len(codes)}"
    assert sorted(codes) == list(range(0, 64)), "Hexagram binary codes 0..63 mismatch"
    print("test_data_header: PASS (All 64 hexagrams verified with unique 0..63 binary codes and 1..64 numbers)")

def test_logic_files():
    logic_h = os.path.join(os.path.dirname(__file__), "..", "main", "jinqiangua_logic.h")
    logic_c = os.path.join(os.path.dirname(__file__), "..", "main", "jinqiangua_logic.c")
    assert os.path.isfile(logic_h), f"File not found: {logic_h}"
    assert os.path.isfile(logic_c), f"File not found: {logic_c}"
    print("test_logic_files: PASS")

if __name__ == "__main__":
    print("=== Jin Qian Gua Verification Tests ===")
    test_data_header()
    test_logic_files()
    print("All Python tests passed!")
