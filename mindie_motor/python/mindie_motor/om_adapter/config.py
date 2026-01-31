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

import json
import os

from om_adapter.common.logging import Log
from om_adapter.common.util import safe_open
from om_adapter.common.util import PathCheck


class ConfigUtil:
    config = None
    logger = None

    @classmethod
    def log_warning(cls, msg: str):
        if not cls.logger:
            cls.logger = Log(__name__).getlog()
        cls.logger.warning(msg)

    @classmethod
    def get_config(cls, key: str):
        if not cls.config:
            server_dir = os.getenv('MIES_INSTALL_PATH')
            if server_dir is None:
                raise RuntimeError("Environment variable $MIES_INSTALL_PATH is not set.")
            mies_install_path = os.path.realpath(server_dir)
            if not PathCheck.check_path_full(mies_install_path):
                raise RuntimeError("Failed to check `MIES_INSTALL_PATH`")
            with safe_open(os.path.join(mies_install_path, "conf", "ms_controller.json")) as f:
                cls.config = json.loads(f.read())
        if key not in cls.config.keys():
            cls.log_warning(f"{key} is not in config of ms controller")
            return ""
        return cls.config[key]
