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


class CircularShareMemory(AbstractShareMemoryUtil):
    def read_data(self):
        read_idx = self.cb.read_idx
        write_idx = self.cb.write_idx
        if read_idx == write_idx:
            return ""
        if write_idx > read_idx:
            chunk = self.cb.data[read_idx: write_idx]
        else:
            chunk = self.cb.data[read_idx:] + self.cb.data[:write_idx]
        self.cb.read_idx = write_idx
        return bytes(chunk).decode("utf-8", errors="ignore")

    def write_data(self, chunk: str):
        byte_chunk = chunk.encode()
        write_idx = self.cb.write_idx
        for i, byte in enumerate(byte_chunk):
            self.cb.data[(write_idx + i) % self.shm_size] = byte
        self.cb.write_idx = (write_idx + len(byte_chunk)) % self.shm_size
