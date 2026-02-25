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

import subprocess
from dataclasses import dataclass
from typing import Optional


@dataclass
class ProcessInfo:
    process: subprocess.Popen
    name: str
    config_file: Optional[str]
    command: str
    has_exited: bool = False

    def __str__(self) -> str:
        return f"ProcessInfo(name={self.name}, pid={self.pid})"

    @property
    def pid(self) -> Optional[int]:
        return self.process.pid if self.process else None
    
    @property
    def is_alive(self) -> bool:
        if not self.process:
            return False
        return self.process.poll() is None

    @property
    def exit_code(self) -> Optional[int]:
        if not self.process:
            return None
        return self.process.poll()
    
