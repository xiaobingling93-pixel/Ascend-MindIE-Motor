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
import stat
import ssl
import ctypes
from ctypes import c_char_p
from OpenSSL import crypto

from node_manager.common.utils import PathCheck, safe_open
from node_manager.common.logging import Log

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


class CertUtil:
    logger = None

    @staticmethod
    def secure_delete_password(password: str) -> None:
        if not password:
            return
        try:
            password_len = len(password)
            # Calculate the offset to the actual string data in memory
            # sys.getsizeof includes object overhead, we need to subtract it to get to the string data
            password_offset = sys.getsizeof(password) - password_len - 1
            # Overwrite the password string in memory with zeros
            ctypes.memset(id(password) + password_offset, 0, password_len)
        except Exception as e:
            logger = Log(__name__).getlog()
            logger.warning(f"Failed to securely clear password from memory: {e}")
        finally:
            del password

    @staticmethod
    def validate_ca_crl(ca_path: str, crl_path: str):
        try:
            with open(crl_path, 'rb') as crl_path_file:
                ca_crl = crypto.load_crl(crypto.FILETYPE_PEM, crl_path_file.read())
            with open(ca_path, "rb") as ca_crt_file:
                ca_cert = crypto.load_certificate(crypto.FILETYPE_PEM, ca_crt_file.read())
            
            ca_pub_key = ca_cert.get_pubkey().to_cryptography_key()
            crl_crypto = ca_crl.to_cryptography()
            valid_signature = crl_crypto.is_signature_valid(ca_pub_key)
            if valid_signature:
                return True
            else:
                return False
        except Exception as e:
            raise RuntimeError(f"Failed to create SSL context: {e}") from e

    @staticmethod
    def create_ssl_context(tls_config: dict, password: str) -> ssl.SSLContext:
        try:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.check_hostname = True
            context.verify_mode = ssl.CERT_REQUIRED

            if CA_CERTS in tls_config and tls_config[CA_CERTS]:
                context.load_verify_locations(cafile=tls_config[CA_CERTS])

            if TLS_CERT in tls_config and TLS_KEY in tls_config:
                password_bytes = password.encode('utf-8') if password else None
                try:
                    context.load_cert_chain(
                        certfile=tls_config[TLS_CERT],
                        keyfile=tls_config[TLS_KEY],
                        password=password_bytes
                    )
                finally:
                    if password_bytes:
                        ctypes.memset(id(password_bytes), 0, len(password_bytes))
                        del password_bytes

            if TLS_CRL in tls_config and tls_config.get(TLS_CRL):
                # 使用 Certvalidator验证CRL
                ca_path = tls_config.get(CA_CERTS)
                crl_path = tls_config[TLS_CRL]
                if not CertUtil.validate_ca_crl(ca_path, crl_path):
                    raise RuntimeError(f"CRL validation failed for {crl_path}")
                context.verify_flags |= ssl.VERIFY_CRL_CHECK_CHAIN

            context.set_ciphers('ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20:!aNULL:!MD5:!DSS')
            return context
        except Exception as e:
            raise RuntimeError(f"Failed to create SSL context: {e}") from e

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