#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
import tempfile

from om_adapter.backends.log_collect.log_processor import LogDataProcessor
from om_adapter.backends.log_collect.data_class import LogFile
from om_adapter.common.util import PathCheck


def create_temp_file(filename, txt):
    temp_file = tempfile.NamedTemporaryFile(
        mode="w+",
        suffix=".log",
        prefix="filename",
        delete=True,
        encoding="utf-8"
    )
    # 写入内容
    temp_file.write(txt)
    temp_file.flush()
    return temp_file


class TestLogDataProcessor(unittest.TestCase):
    def setUp(self) -> None:
        self.filename = os.path.realpath("coordinator.txt")
        with open(self.filename, 'w') as f:
            f.write("test message")

    def test_get_log(self) -> None:
        component = "Coordinator"
        self.log_processor = LogDataProcessor()
        self.log_processor.watch_files[self.filename] = LogFile(self.filename)
        self.log_processor.modified_log_files.add(self.filename)
        with patch.object(PathCheck, 'check_path_full', return_value=(True, None)) as mock_fuc:
            # 首次读文件
            log_data = self.log_processor.get_log_data(component)
            self.assertEqual(log_data.component_type, component)
            self.assertEqual(log_data.meta_data_list[0].log_type, f"mindie-{component}")
            self.assertEqual(log_data.meta_data_list[0].meta_data, [])  # 第一次读文件时，文件内容为空

            # 读取增量日志文件
            with open(self.filename, 'w') as f:
                f.write("test new message")
            self.log_processor.modified_log_files.add(self.filename)
            log_data = self.log_processor.get_log_data(component)
            self.assertEqual(log_data.meta_data_list[0].meta_data, ["test new message"])

    def tearDown(self) -> None:
        os.remove(self.filename)

