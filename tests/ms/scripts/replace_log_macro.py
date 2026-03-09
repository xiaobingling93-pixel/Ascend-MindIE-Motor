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
import stat
from pathlib import Path


CURRENT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = CURRENT_DIR.parent.parent.parent


# 定义需要查找的日志关键字
log_keywords = [
    'LOG_C', 'LOG_E', 'LOG_W', 'LOG_I', 'LOG_D', 'LOG_P', 'LOG_M',
    'DIGS_LOG_ERROR', 'DIGS_LOG_WARNING', 'DIGS_LOG_INFO', 'DIGS_LOG_DEBUG'
]


# 定义需要排除的文件路径关键字
exclude_keywords = ['open_source', 'build', 'output', 'hsec', '3rdparty']


def should_exclude(file_path):
    """检查文件路径是否包含排除关键字"""
    return any(keyword in file_path for keyword in exclude_keywords)


def process_file(file_path):
    """处理单个文件，添加注释"""
    with open(file_path, 'r', encoding='utf-8') as file:
        lines = file.readlines()

    # 修改行内容
    modified_lines = []
    for line in lines:
        # 检查行中是否包含任意一个日志关键字
        if any(keyword in line for keyword in log_keywords):
            # 去除行尾的换行符等空白字符
            stripped_line = line.rstrip()
            # 添加注释
            new_line = f'{stripped_line} // LCOV_EXCL_BR_LINE\n'
            modified_lines.append(new_line)
        else:
            modified_lines.append(line)

    # 如果文件内容有变化，则写回文件
    if lines != modified_lines:
        flags = os.O_WRONLY | os.O_CREAT
        modes = stat.S_IWUSR | stat.S_IRUSR
        with os.fdopen(os.open(file_path, flags, modes), 'w') as fout:
            fout.writelines(modified_lines)


def walk_directory(directory):
    """递归遍历目录并处理文件"""
    for root, _, files in os.walk(directory):
        for name in files:
            if name.endswith('.cpp'):
                file_path = os.path.join(root, name)
                if not should_exclude(file_path):
                    process_file(file_path)

ms_path = PROJECT_ROOT / 'mindie_motor/src'
util_path = PROJECT_ROOT / 'utils'

walk_directory(ms_path)
walk_directory(util_path)