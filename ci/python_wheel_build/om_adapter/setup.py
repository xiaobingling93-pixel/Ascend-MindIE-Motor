#!/usr/bin/env python3
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
import os
from setuptools import find_packages, setup

_HERE = os.path.dirname(os.path.abspath(__file__))
_PKG_ROOT = os.path.normpath(
    os.path.join(_HERE, "..", "..", "..", "mindie_motor", "python", "mindie_motor")
)


def _read_requirements():
    req_path = os.path.join(_PKG_ROOT, "requirement.txt")
    if not os.path.isfile(req_path):
        return []
    out = []
    with open(req_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                out.append(line)
    return out


setup(
    name="om_adapter",
    version=os.environ.get("MINDIE_SERVICE_PY_WHEEL_VERSION", "3.0.0"),
    description="MindIE OM adapter service",
    packages=find_packages(where=_PKG_ROOT, include=("om_adapter", "om_adapter.*")),
    package_dir={"": _PKG_ROOT},
    python_requires=">=3.9",
    install_requires=_read_requirements(),
    entry_points={"console_scripts": ["om_adapter=om_adapter.adapter_run:main"]},
)
