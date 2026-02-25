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
import re

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

from om_adapter.common.logging import Log
from om_adapter.common.util import PathCheck
from om_adapter.backends.log_collect.log_processor import LogDataProcessor
from om_adapter.backends.log_collect.data_class import LogFile

DEFAULT_COLLECT_PATH = os.path.realpath(os.environ.get("MINDIE_LOG_PATH", "/root/mindie/log"))
MONITOR_INTERVAL_MILLION_SECONDS = 3
LOG_PATTERNS = "^.*_(controller|coordinator)_[a-zA-Z0-9]+_[a-zA-Z0-9]+.log$"


class CollectHandler(FileSystemEventHandler):
    def __init__(self, dir_path):
        self.logger = Log(__name__).getlog()
        self.dir_path = dir_path
        self.log_processor = LogDataProcessor()

    @staticmethod
    def _check_valid_file(event):
        if event.is_directory:
            return False
        if not bool(re.match(LOG_PATTERNS, os.path.basename(event.src_path))):
            return False
        return PathCheck.check_path_full(event.src_path)

    def on_created(self, event):
        if self._check_valid_file(event):
            self.logger.info(f"[OM Adapter] File %s is created" % event.src_path)
            self.log_processor.watch_files[event.src_path] = LogFile(file_path=event.src_path)
            self.log_processor.modified_log_files.add(event.src_path)

    def on_modified(self, event):
        if self._check_valid_file(event):
            self.logger.debug(f"[OM Adapter] File %s is modified", event.src_path)  # 文件内容更新频率高
            if event.src_path not in self.log_processor.watch_files:
                self.log_processor.watch_files[event.src_path] = LogFile(file_path=event.src_path)
            self.log_processor.modified_log_files.add(event.src_path)

    def on_deleted(self, event):
        if self._check_valid_file(event):
            self.logger.info("[OM Adapter] File %s is deleted" % event.src_path)
            self.log_processor.watch_files.pop(event.src_path)
            self.log_processor.modified_log_files.pop(event.src_path)

    def on_moved(self, event):
        if self._check_valid_file(event):
            self.logger.info(f"[OM Adapter] File %s is changed to %s" % (event.src_path, event.dest_path))
            src_log_file = self.log_processor.watch_files.pop(event.src_path, LogFile(file_path=event.dest_path))
            src_log_file.file_path = event.dest_path
            src_log_file.last_read_position = 0  # 文件轮转后，更新读取位置
            self.log_processor.watch_files[event.dest_path] = src_log_file


class Collector:
    def __init__(self, collect_path=DEFAULT_COLLECT_PATH):
        self.logger = Log(__name__).getlog()
        if not collect_path:
            err_msg = f"[OM Adapter] Init log monitor failed, the collect_path is empty from config.json"
            self.logger.error(err_msg)
            raise Exception(err_msg)
        self.logger.info(f"[OM Adapter] Log monitor path is %s" % collect_path)

        self.collect_handler = CollectHandler(collect_path)
        self.collect_observer = Observer()
        self.collect_observer.schedule(self.collect_handler, collect_path, recursive=True)
        try:
            self.collect_observer.start()
        except Exception as e:
            self.logger.error(f"[OM Adapter] Failed to start collect_observer: {e}")
            self.running = False
            raise RuntimeError(f"Observer startup failed: {e}") from e

    def stop(self):
        self.collect_observer.stop()
        self.collect_observer.join()
