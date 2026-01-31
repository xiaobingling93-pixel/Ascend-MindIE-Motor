#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import unittest
from unittest.mock import patch

from mindie_motor.src.example.deploy_scripts.utils.validate_config import (
    is_valid_path,
    is_valid_integer,
    is_valid_bool,
    is_valid_str,
    is_valid_mount,
    PathValidationConfig
)

FILE_PATH = "filepath"
VALUE = "value"
FLAG = "flag"
NAME = "name"
MOUNTS = "mounts"
TEST_PATH = "test_path"
TEST_LINK = "test_link"


class TestValidationFunctions(unittest.TestCase):
    def setUp(self):
        # Create a temporary directory and file for testing
        self.test_dir = "test_dir"
        self.test_file = "test_file.txt"
        os.makedirs(self.test_dir, exist_ok=True)
        with open(self.test_file, "w") as f:
            f.write("test content")

    def tearDown(self):
        # Clean up temporary files and directories
        if os.path.exists(self.test_file):
            os.remove(self.test_file)
        if os.path.exists(self.test_dir):
            os.rmdir(self.test_dir)

    def test_is_valid_path(self):
        # Test valid path
        test_path = os.path.abspath(self.test_file)
        self.assertTrue(is_valid_path(TEST_PATH, test_path))

        # Test empty path
        config = PathValidationConfig(allow_empty=True)
        self.assertTrue(is_valid_path(TEST_PATH, "", config))

        # Test empty path
        self.assertFalse(is_valid_path(TEST_PATH, ""))

        # Test non-string path, expecting TypeError
        self.assertFalse(is_valid_path(TEST_PATH, 123))

        # Test overly long path
        long_path = "/" + ("a" * 1025)
        self.assertFalse(is_valid_path(TEST_PATH, long_path))

        # Test path with invalid characters
        invalid_path = "test?file.txt"
        self.assertFalse(is_valid_path(TEST_PATH, invalid_path))

        # Create a symbolic link
        os.symlink(self.test_file, TEST_LINK)
        # Test symbolic link path
        config = PathValidationConfig(is_support_root=False)
        self.assertFalse(is_valid_path(TEST_LINK, TEST_LINK, config))
        # Clean up symbolic link
        if os.path.exists(TEST_LINK):
            os.remove(TEST_LINK)

        # Test non-existent path
        self.assertFalse(is_valid_path(TEST_PATH, "nonexistent_file.txt"))

        # Simulate mismatched file owner and group
        with patch("os.getuid", return_value=1000), patch("os.getgid", return_value=1000):
            with patch("os.stat") as mock_stat:
                mock_stat.return_value.st_uid = 2000
                mock_stat.return_value.st_gid = 2000
                config = PathValidationConfig(is_support_root=False)
                self.assertFalse(is_valid_path(TEST_PATH, self.test_file, config))

        # Create a large file
        large_file = "large_file.bin"
        with open(large_file, "wb") as f:
            f.write(b'\0' * (11 * 1024 * 1024))  # 11MB

        # Test file size exceeding limit
        config = PathValidationConfig(max_file_size=10 * 1024 * 1024)
        self.assertFalse(is_valid_path(TEST_PATH, large_file, config))

        # Clean up large file
        if os.path.exists(large_file):
            os.remove(large_file)

        # Test system path
        system_path = "/usr/bin/test_file"
        self.assertFalse(is_valid_path(TEST_PATH, system_path))

    def test_is_valid_integer(self):
        # Test integer within the valid range
        is_valid_integer(VALUE, 5, min_val=1, max_val=10)

        # Test boundary values
        is_valid_integer(VALUE, 0, min_val=0)
        is_valid_integer(VALUE, 10, max_val=10)

        # Test integer exceeding the valid range, expecting ValueError
        with self.assertRaises(ValueError):
            is_valid_integer(VALUE, 15, min_val=1, max_val=10)

        # Test non-integer input, expecting TypeError
        with self.assertRaises(TypeError):
            is_valid_integer(VALUE, 3.14, min_val=1, max_val=5)

    def test_is_valid_bool(self):
        # Test boolean values
        is_valid_bool(FLAG, True)
        is_valid_bool(FLAG, False)

        # Test non-boolean input, expecting TypeError
        with self.assertRaises(TypeError):
            is_valid_bool(FLAG, 1)
        with self.assertRaises(TypeError):
            is_valid_bool(FLAG, 'true')

    def test_is_valid_str(self):
        # Test valid string
        is_valid_str(NAME, 'valid_string')

        # Test minimum and maximum length
        is_valid_str(NAME, 'a', min_length=1)
        is_valid_str(NAME, 'a' * 64, max_length=64)

        # Test string exceeding the maximum length, expecting ValueError
        with self.assertRaises(ValueError):
            is_valid_str(NAME, 'a' * 65)

        # Test string containing invalid characters, expecting ValueError
        with self.assertRaises(ValueError):
            is_valid_str(NAME, 'invalid_char!')

    def test_is_valid_mount(self):
        # Test valid dictionary input
        is_valid_mount(MOUNTS, {'/path1': '/path2'})
        is_valid_mount(MOUNTS, {'$var': 'path'})

        # Test key starting with $ but invalid value, expecting ValueError
        with self.assertRaises(ValueError):
            is_valid_mount(MOUNTS, {'$var': '/invalid_value$'})

        # Test key not starting with $ but invalid path, expecting ValueError
        with self.assertRaises(ValueError):
            is_valid_mount(MOUNTS, {'invalid_key$': ''})

        # Test non-dictionary input, expecting TypeError
        with self.assertRaises(TypeError):
            is_valid_mount(MOUNTS, 'invalid_input')


if __name__ == '__main__':
    unittest.main()
