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
import ctypes
from ctypes import c_char_p

from om_adapter.common.util import PathCheck, safe_open
from om_adapter.common.logging import Log

CA_CERTS = "ca_cert"
TLS_CERT = "tls_cert"
TLS_KEY = "tls_key"
TLS_CRL = "tls_crl"
SSL_MUST_KEYS = [CA_CERTS, TLS_CERT, TLS_KEY]


def _check_directory_permissions(cur_path):
    cur_stat = os.stat(cur_path)
    cur_mode = stat.S_IMODE(cur_stat.st_mode)
    if cur_mode != 0o700:
        raise RuntimeError("The permission of ssl directory should be 700")


def _check_invalid_ssl_filesize(ssl_options):
    def check_size(path: str):
        size = os.path.getsize(path)
        if size > max_size:
            raise RuntimeError(f"SSL file should not exceed 10MB!")

    max_size = 10 * 1024 * 1024  # 最大文件大小为10MB
    for ssl_key in SSL_MUST_KEYS:
        check_size(ssl_options[ssl_key])


def _check_invalid_ssl_path(ssl_options):
    def check_single(key: str, path: str):
        if not PathCheck.check_path_full(path):
            raise RuntimeError(f"Enum {key} path is invalid")
        _check_directory_permissions(os.path.dirname(path))

    if not isinstance(ssl_options, dict):
        raise RuntimeError("ssl_options should be a dict!")
    for ssl_key in SSL_MUST_KEYS:
        if ssl_key not in ssl_options.keys():
            raise RuntimeError(f"{ssl_key} should be provided when ssl enables!")
        check_single(ssl_key, ssl_options[ssl_key])


class AdapterCertUtil:
    logger = None

    @classmethod
    def log_info(cls, msg):
        if not cls.logger:
            cls.logger = Log(__name__).getlog()
        cls.logger.info(msg)

    @classmethod
    def validate_cert_and_decrypt_password(cls, config: dict) -> str:
        if config[TLS_CRL]:
            SSL_MUST_KEYS.append(TLS_CRL)
        _check_invalid_ssl_path(config)
        _check_invalid_ssl_filesize(config)

        with safe_open(config["tls_passwd"]) as f:
            return f.read().strip()
        return None
