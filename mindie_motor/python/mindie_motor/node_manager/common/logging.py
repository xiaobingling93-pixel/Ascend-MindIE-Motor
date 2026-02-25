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
import stat
import time
import re
import sys
from datetime import datetime, timedelta, timezone
from dataclasses import dataclass, field
from pathlib import Path
import logging
from logging.handlers import RotatingFileHandler
from node_manager.common.utils import PathCheckBase


MODULE_NAME = 'mindie-node_manager'
MINDIE_PREFIX = "MINDIE_"
UNSET_LOGGER = 'NULL'
FILE_SIZE = 'fs'
FILE_COUNT = 'fc'
FILE_PER_PROCESS = 'r'
BOOL_ENV_CHOICES = ["True", "False", "true", "false", "0", "1"]
LOG_ENV_LEVEL = ['critical', 'error', 'warn', 'info', 'debug', 'null']
TRUE_STR = "true"


logger_screen = logging.getLogger("node_manager_screen")
logger_screen.setLevel(logging.INFO)
ch = logging.StreamHandler(sys.stdout)
logger_screen.addHandler(ch)


def print_value_warn(value_type, env_value, default_value, support_choices=None):
    msg = f"[WARNING] Invalid value of node_manager in {value_type}: {env_value}. "
    if support_choices:
        msg += f"Only {support_choices} support. "
    msg += f"{value_type} will be set default to {default_value} "
    logger_screen.warning(msg)


def is_true_value(value):
    return value == TRUE_STR or value == "1"


def recursive_chmod(cur_path, mode=0o750):
    cur_path = os.path.realpath(cur_path)
    while True:
        parent_path = os.path.dirname(cur_path)
        if parent_path == cur_path:
            break
        os.chmod(cur_path, mode)
        cur_path = parent_path


@dataclass
class LogParams:
    path: str = '~/mindie/log/'
    level: str = 'INFO'
    to_file: bool = True
    to_console: bool = False
    verbose: bool = True
    rotate_options: dict = field(default_factory=dict)


class Singleton(type):
    _instances = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwargs)
        return cls._instances[cls]


class NoNewlineFormatter(logging.Formatter):
    def format(self, record):
        special_chars = [
            '\n', '\r', '\f', '\t', '\v', '\b',
            '\u000A', '\u000D', '\u000C',
            '\u000B', '\u0008', '\u007F',
            '\u0009', '    ',
        ]
        for c in special_chars:
            record.msg = str(record.msg).replace(c, ' ')
        if record.levelname == "WARNING":
            record.levelname = "WARN"
        return super(NoNewlineFormatter, self).format(record)

    def formatTime(self, record, datefmt=None):
        timezone_offset = time.timezone
        offset_hours = -timezone_offset // 3600
        dt = datetime.fromtimestamp(record.created, timezone(timedelta(hours=offset_hours)))
        timestamp = dt.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        offset = dt.strftime("%z")
        offset = f"{offset[:3]}:{offset[3:]}"
        return f"{timestamp}{offset} DST" if time.daylight else f"{timestamp}{offset}"


def _change_to_readonly(file_name):
    current_permissions = os.stat(file_name).st_mode
    new_permissions = current_permissions & ~(stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH)
    os.chmod(file_name, new_permissions)


def _create_log_file(log_file):
    mode = 0o640
    if not os.path.exists(log_file):
        with os.fdopen(os.open(log_file, os.O_CREAT, mode), "w"):
            pass
    else:
        clean_path = os.path.normpath(log_file)
        if os.path.islink(clean_path):
            err_msg = f"Check log file path failed because it's a symbolic."
            raise ValueError(err_msg)
        if len(clean_path) > 1024:
            err_msg = f"Path of log file is too long, it should not exceed 1024 character."
            raise ValueError(err_msg)
    os.chmod(log_file, mode)


def init_logger() -> LogParams:
    def load_log_env(env_type: str, default_value: str):
        env_value = f'{MINDIE_PREFIX}{env_type}'
        log_env = None
        if env_value in os.environ:
            log_env = os.getenv(env_value)
        env_values = dict(env_value=log_env, default_value=default_value)
        return env_values
    log_params = LogParams()
    # set log path
    log_path_env = load_log_env("LOG_PATH", "~/mindie/log")
    log_params.path = Log.get_log_string_from_env(log_path_env, "LOG_PATH")
    # set to file
    log_to_file_env = load_log_env("LOG_TO_FILE", TRUE_STR)
    log_params.to_file = Log.get_log_bool_from_env(log_to_file_env, "LOG_TO_FILE")
    # set to stdout
    log_to_stdout_env = load_log_env("LOG_TO_STDOUT", TRUE_STR)
    log_params.to_console = Log.get_log_bool_from_env(log_to_stdout_env, "LOG_TO_STDOUT")
    # set log level
    log_level_env = load_log_env("LOG_LEVEL", "INFO")
    log_params.level = Log.get_log_string_from_env(log_level_env, "LOG_LEVEL")
    # set log verbose option
    log_verbose_env = load_log_env("LOG_VERBOSE", TRUE_STR)
    log_params.verbose = Log.get_log_bool_from_env(log_verbose_env, "LOG_VERBOSE")
    # set log rotate options
    log_rotate_env = load_log_env("LOG_ROTATE", "-fs 20 -r 10")
    log_params.rotate_options = Log.get_log_rotate_from_env(log_rotate_env, "LOG_ROTATE")
    return log_params


def _filter_files(directory, prefix, max_num):
    all_files = [f for f in os.listdir(directory) if f.startswith(prefix)]
    file_num = len(all_files)
    delete_file_num = file_num - max_num
    if delete_file_num <= 0:
        return

    files_with_mtime = [(f, os.path.getmtime(os.path.join(directory, f))) for f in all_files]
    sorted_files = sorted(files_with_mtime, key=lambda x: x[1])

    files_to_delete = sorted_files[:delete_file_num]
    for file in files_to_delete:
        file_path = os.path.join(directory, file[0])
        if os.path.exists(file_path):
            os.remove(file_path)


def _complete_relative_path(cur_path: str, base_dir: str):
    if os.path.isabs(cur_path):
        return cur_path
    base_directory = Path(base_dir)
    relative_path = cur_path
    combined_path = base_directory / relative_path
    return str(combined_path.resolve())


def _close_logger(parent_directory: str, base_filename, ts):
    pid = os.getpid()
    new_filename = os.path.join(parent_directory, f'{MODULE_NAME}_{pid}_{ts}.log')
    if os.path.exists(base_filename):
        os.rename(base_filename, new_filename)


class CustomRotatingFileHandler(RotatingFileHandler):
    def __init__(self, filename, file_per_process, mode="a", maxBytes=0, backupCount=0,
                 encoding=None, delay=False, errors=None):
        super().__init__(filename, mode, maxBytes, backupCount, encoding, delay, errors)
        self.file_per_process = file_per_process
        self.backupCount = backupCount - 1
        self.log_id = 0
        current_time = time.time()
        local_time = time.localtime(current_time)
        ts = time.strftime("%Y%m%d%H%M%S", local_time)
        milliseconds = int((current_time - int(current_time)) * 1000)
        self.ts = f"{ts}{milliseconds:03d}"
        self.first_log = True

    def rotate(self, source, dest):
        pid = os.getpid()
        parent_directory = os.path.dirname(source)
        self.log_id = (self.log_id % self.backupCount) + 1
        if self.log_id == 1 and not self.first_log:
            current_time = time.time()
            local_time = time.localtime(current_time)
            ts = time.strftime("%Y%m%d%H%M%S", local_time)
            milliseconds = int((current_time - int(current_time)) * 1000)
            self.ts = f"{ts}{milliseconds:03d}"
        self.first_log = False
        new_filename = f'{parent_directory}/{MODULE_NAME}_{pid}_{self.ts}.{self.log_id:02d}log'

        super().rotate(source, new_filename)
        _change_to_readonly(new_filename)
        _create_log_file(source)

        # limit file nums in the same process
        prefix = f'{MODULE_NAME}_{pid}'
        _filter_files(parent_directory, prefix, self.file_per_process)
        # limit total file nums
        if self.backupCount > 0:
            prefix = f'{MODULE_NAME}'
            _filter_files(parent_directory, prefix, self.backupCount)

    def close(self):
        if self.stream:
            self.stream.close()
            parent_directory = os.path.dirname(self.baseFilename)
            _close_logger(parent_directory=parent_directory, base_filename=self.baseFilename, ts=self.ts)
            self.stream = None
            # limit total file nums
            if self.backupCount > 0:
                prefix = f'{MODULE_NAME}'
                _filter_files(parent_directory, prefix, self.backupCount)


class Log(metaclass=Singleton):
    MODULE_KEY_NAME = 'node_manager'
    MIN_FILE_SIZE = 1 * 1024 * 1024
    MAX_FILE_SIZE = 500 * 1024 * 1024
    MIN_FILE_PER_PROCESS = 1
    MAX_FILE_PER_PROCESS = 64

    def __init__(self, logger=None):
        self._logger = logging.getLogger('node_manager')
        # Get log path and level from environment variables
        log_params = init_logger()
        self.log_level = log_params.level
        self.log_to_file = log_params.to_file
        self.log_file = ""
        self.verbose = log_params.verbose
        self.rotate_options = log_params.rotate_options
        self.to_console = log_params.to_console

        levels = {
            'DEBUG': logging.DEBUG,
            'INFO': logging.INFO,
            'WARN': logging.WARNING,
            'ERROR': logging.ERROR,
            'CRITICAL': logging.CRITICAL,
            UNSET_LOGGER: logging.CRITICAL + 1
        }
        # Set logger level based on configured log level
        self._logger.setLevel(levels.get(self.log_level.upper(), logging.INFO))

        # Set log format
        if self.verbose:
            file_logging_format = NoNewlineFormatter(
                '[%(asctime)s] [%(process)d] [%(thread)d] [%(name)s] [%(levelname)s] '
                '[%(filename)s:%(lineno)s] %(message)s'
            )
        else:
            file_logging_format = NoNewlineFormatter(
                '[%(asctime)s] [%(levelname)s] %(message)s'
            )
        # Output log file
        if log_params.to_file and self.log_level.upper() != UNSET_LOGGER:
            # Create file handler and write logs to the specified file
            base_dir = os.path.expanduser('~/mindie/log')
            log_path = os.path.expanduser(log_params.path)
            log_path = _complete_relative_path(log_path, base_dir)
            if base_dir == log_path:
                log_path = os.path.join(log_path, 'debug')
            else:
                log_path = os.path.join(log_path, 'log/debug')
            # Create the log directory if it does not exist
            os.makedirs(log_path, exist_ok=True)
            recursive_chmod(log_path)
            pid = os.getpid()
            base_filename = f'{MODULE_NAME}_{pid}.log'
            self.log_file = os.path.join(log_path, base_filename)
            if self.log_level.upper() != UNSET_LOGGER:
                _create_log_file(self.log_file)
                os.chmod(self.log_file, 0o640)

            file_handler = CustomRotatingFileHandler(filename=self.log_file,
                                                     file_per_process=self.rotate_options.get(FILE_PER_PROCESS),
                                                     mode='a',
                                                     maxBytes=self.rotate_options.get(FILE_SIZE),
                                                     backupCount=self.rotate_options.get(FILE_COUNT, 64))
            file_handler.setLevel(levels.get(self.log_level.upper(), logging.INFO))
            file_handler.setFormatter(file_logging_format)

            # Add file handler to logger
            self._logger.addHandler(file_handler)
        # Add console output
        if self.to_console and self.log_level.upper() != UNSET_LOGGER:
            stream_handler = logging.StreamHandler()
            stream_handler.setFormatter(file_logging_format)
            self._logger.addHandler(stream_handler)

    @property
    def logger(self):
        return self._logger
    
    @staticmethod
    def make_dirs_if_not_exist(input_dir):
        if not os.path.exists(input_dir):
            os.makedirs(input_dir)
        os.chmod(input_dir, 0o750)

    @classmethod
    def get_log_string_from_env(cls, env_values: dict, parse_type: str) -> str:
        """
        解析字符串类环境变量
        1、解析日志路径环境变量
        export MINDIE_LOG_PATH="llm: /home/working; client: /path"
        默认 ~/mindie/log
        2、解析日志等级环境变量
        export MINDIE_LOG_LEVEL="llm: error; client: debug"
        默认 info
        """
        def check_path(path_value, default_value):
            if not isinstance(path_value, str) or not PathCheckBase.check_path_full(os.path.expanduser(path_value)):
                print_value_warn(parse_type, path_value, default_value)
                return default_value
            return path_value
        
        def check_level(level_value, default_value):
            if level_value.lower() not in LOG_ENV_LEVEL:
                print_value_warn(parse_type, level_value, default_value, LOG_ENV_LEVEL)
                return default_value.lower()
            return level_value.lower()
        check_fun = {
            'LOG_PATH': check_path,
            'LOG_LEVEL': check_level
        }
        default_values = {
            'LOG_PATH': '~/mindie/log',
            'LOG_LEVEL': 'INFO'
        }
        env_value = env_values.get("env_value")
        default_value = check_fun.get(parse_type)(env_values.get("default_value"), default_values.get(parse_type))
        if not env_value:
            return default_value
        if ':' not in env_value:
            if ';' in env_value:
                print_value_warn(MINDIE_PREFIX + parse_type, env_value, default_value)
                return default_value

        log_string_dict = cls._split_env_value(env_value=env_value)

        if cls.MODULE_KEY_NAME in log_string_dict:
            return check_fun.get(parse_type)(log_string_dict[cls.MODULE_KEY_NAME], default_value)
        return default_value

    @classmethod
    def get_log_bool_from_env(cls, env_values: dict, parse_type: str) -> bool:
        """
        解析日志bool类型环境变量
        export MINDIE_LOG_VERBOSE取值范围[false/true]或[0/1]
        默认 false
        """
        true_value = 'true'
        env_value = env_values.get("env_value")
        default_value = true_value
        if env_values.get("default_value") not in BOOL_ENV_CHOICES:
            print_value_warn(parse_type, default_value, true_value, BOOL_ENV_CHOICES)
        else:
            default_value = env_values.get("default_value")
        if not env_value:
            return is_true_value(default_value)

        if ':' not in env_value:
            if ';' in env_value:
                print_value_warn(MINDIE_PREFIX + parse_type, env_value, default_value)
                return is_true_value(default_value)

        log_bool_dict = cls._split_env_value(env_value=env_value)

        if cls.MODULE_KEY_NAME in log_bool_dict:
            if log_bool_dict.get(cls.MODULE_KEY_NAME) not in BOOL_ENV_CHOICES:
                print_value_warn(MINDIE_PREFIX + parse_type, env_value, default_value, BOOL_ENV_CHOICES)
                return is_true_value(default_value)
            return (log_bool_dict[cls.MODULE_KEY_NAME].lower() == true_value
                    or log_bool_dict[cls.MODULE_KEY_NAME] == '1')

        return is_true_value(default_value)

    @classmethod
    def get_log_rotate_from_env(cls, env_values: dict, parse_type: str) -> dict:
        """
        Parse log rotation environment variable
        export MINDIE_LOG_ROTATE="llm: -fs 20; client: -fc 4"
        Parameters:
         fs fileSize: Integer, default 20, unit MB
         r fileCountPerProcess: Integer, count, each process has fc logs
        """
        # Default log file size is 20MB, and each process has 1 log file by default
        option_dict = {
            FILE_SIZE: 20 * 1024 * 1024,
            FILE_COUNT: 64,
            FILE_PER_PROCESS: 10
        }

        def parse_single_rotate_env(input_str: str, option_dict: dict, value_type: str):
            if not input_str:
                return option_dict
            matches = re.findall(r'-([a-z]+)\s+([0-9]+)', input_str)
            if not matches:
                print_value_warn(value_type, input_str, option_dict)
                return option_dict
            for key, value in matches:
                if key in option_dict and value.isdigit():
                    option_dict[key] = int(value)
                    if key == FILE_SIZE:
                        option_dict[key] = option_dict[key] * 1024 * 1024
            return option_dict
        env_value = env_values.get("env_value")
        default_value = parse_single_rotate_env(env_values.get("default_value"), option_dict, parse_type)
        if not env_value:
            return default_value

        if ':' not in env_value:
            if ';' in env_value:
                print_value_warn(MINDIE_PREFIX + parse_type, env_value, option_dict) 
                return option_dict
            option_dict = parse_single_rotate_env(env_value, option_dict, MINDIE_PREFIX + parse_type)
            return option_dict
        log_rotate_dict = cls._split_env_value(env_value=env_value)

        if cls.MODULE_KEY_NAME in log_rotate_dict:
            input_str = log_rotate_dict[cls.MODULE_KEY_NAME]
            option_dict = parse_single_rotate_env(input_str, option_dict, MINDIE_PREFIX + parse_type)

        option_dict[FILE_SIZE] = option_dict[FILE_SIZE]
        if (option_dict.get(FILE_SIZE) < cls.MIN_FILE_SIZE or
                option_dict.get(FILE_SIZE) > cls.MAX_FILE_SIZE):
            raise ValueError('Invalid MINDIE_LOG_ROTATE, file size should be in range [1, 500]MB. '
                             'Please check environment value')
        if (option_dict.get(FILE_PER_PROCESS) < cls.MIN_FILE_PER_PROCESS or
                option_dict.get(FILE_PER_PROCESS) > cls.MAX_FILE_PER_PROCESS):
            raise ValueError('Invalid MINDIE_LOG_ROTATE, file count per process should be in range [1, 64]. '
                             'Please check environment value')
        return option_dict
    
    @classmethod
    def _split_env_value(cls, env_value: str) -> dict:
        sub_strs = [sub_str.strip() for sub_str in env_value.split(";")]
        log_verbose = [sub_str.strip().split(":") for sub_str in sub_strs]
        log_verbose_dict = dict()
        for pair in log_verbose:
            if len(pair) == 1:
                log_verbose_dict.update({cls.MODULE_KEY_NAME: pair[0].strip()})
            else:
                log_verbose_dict.update({pair[0].strip().lower(): pair[1].strip()})
        return log_verbose_dict
    
    def getlog(self):
        return self._logger

    def set_log_file_permission(self, perm=0o440):
        # 结束后修改日志权限
        if self.log_level.upper() == UNSET_LOGGER or not self.log_to_file:
            return
        os.chmod(self.log_file, perm)
