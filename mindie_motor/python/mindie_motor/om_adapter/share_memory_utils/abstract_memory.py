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
import sys
from abc import ABC, abstractmethod
import mmap
import ctypes
import posix_ipc

from om_adapter.common.logging import Log


class AbstractShareMemoryUtil(ABC):
    def __init__(self, semaphore_name: str, shm_name: str, shm_size: int = 100):
        class BufferClass(ctypes.Structure):
            _fields_ = [
                ("read_idx", ctypes.c_uint32),
                ("write_idx", ctypes.c_uint32),
                ("data", ctypes.c_uint8 * shm_size)
            ]
        shm = posix_ipc.SharedMemory(f"/{shm_name}", posix_ipc.O_CREAT, mode=0o600,
                                     size=ctypes.sizeof(BufferClass))
        mmap_file = mmap.mmap(shm.fd, ctypes.sizeof(BufferClass))
        self.logger = Log(__name__).getlog()
        self.cb = BufferClass.from_buffer(mmap_file)
        self.sem = posix_ipc.Semaphore(f"/{semaphore_name}", posix_ipc.O_CREAT,
                                       mode=0o600, initial_value=1)
        self.shm_size = shm_size
        
        # 检查共享存储与命名信号量的属主, 属主正确时同步显式设置权限为0600
        target_mode = 0o600

        shm_path = f"/dev/shm/{shm_name}"
        sem_path = f"/dev/shm/sem.{semaphore_name}"        

        st_shm = os.stat(shm_path)
        if st_shm.st_uid != os.getuid():
            raise PermissionError(f"SharedMemory owner mismatch: {shm_name}")
        try:
            os.chmod(shm_path, target_mode)            
        except OSError as e:
            raise RuntimeError(f"Failed to chmod {shm_path}: {e}") from e

        st_sem = os.stat(sem_path)
        if st_sem.st_uid != os.getuid():
            raise PermissionError(f"Semaphore owner mismatch: {semaphore_name}")
        try:
            os.chmod(sem_path, target_mode)            
        except OSError as e:
            raise RuntimeError(f"Failed to chmod {sem_path}: {e}") from e

    @abstractmethod
    def read_data(self) -> str:
        pass

    @abstractmethod
    def write_data(self, chunk: str):
        pass

    def read(self) -> str:
        try:
            self.sem.acquire()
            return self.read_data()
        except Exception as e:
            self.logger.error(e)
            return ""
        finally:
            self.sem.release()

    def write(self, chunk: str):
        try:
            self.sem.acquire()
            self.write_data(chunk)
        except Exception as e:
            self.logger.error(e)
        finally:
            self.sem.release()
