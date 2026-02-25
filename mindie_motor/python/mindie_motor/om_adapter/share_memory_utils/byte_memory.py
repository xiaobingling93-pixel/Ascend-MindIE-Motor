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

from .abstract_memory import AbstractShareMemoryUtil


class ByteShareMemory(AbstractShareMemoryUtil):
    def read_data(self):
        res = bytes(self.cb.data).decode("utf-8", errors="ignore")
        res = res.split("\x00")[0]
        return res if res else ""

    def write_data(self, chunk: str):
        self.cb.data = chunk.encode()
