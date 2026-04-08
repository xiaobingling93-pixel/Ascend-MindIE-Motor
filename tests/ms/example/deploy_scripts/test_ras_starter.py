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
import sys
import tempfile
import unittest

# Add deploy_scripts to path for ras_starter's utils import
_script_dir = os.path.dirname(os.path.abspath(__file__))
_project_root = os.path.normpath(os.path.join(_script_dir, "..", "..", "..", ".."))
_deploy_scripts_dir = os.path.join(_project_root, "mindie_motor", "src", "example", "deploy_scripts")
if _deploy_scripts_dir not in sys.path:
    sys.path.insert(0, _deploy_scripts_dir)

import ras_starter


def _write_json_file(path: str, obj: dict) -> None:
    """Write JSON with mode 0o640 so ras_starter.safe_open permission check passes."""
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f)
    os.chmod(path, 0o640)


class TestParseBootArgs(unittest.TestCase):
    """Test parse_boot_args; unknown tokens raise ValueError."""

    def test_parse_boot_args_empty(self):
        """Empty args returns defaults."""
        result = ras_starter.parse_boot_args([])
        self.assertEqual(result["--user_config_path"], "./user_config.json")
        self.assertEqual(result["--conf_path"], "./conf")
        self.assertEqual(result["--deploy_yaml_path"], "./deployment")
        self.assertEqual(result["--output_path"], "./output")

    def test_parse_boot_args_full(self):
        """Full args override defaults."""
        args = [
            "--user_config_path", "/path/to/user_config.json",
            "--conf_path", "/path/to/conf",
            "--deploy_yaml_path", "/path/to/deployment",
            "--output_path", "/path/to/output",
        ]
        result = ras_starter.parse_boot_args(args)
        self.assertEqual(result["--user_config_path"], "/path/to/user_config.json")
        self.assertEqual(result["--conf_path"], "/path/to/conf")
        self.assertEqual(result["--deploy_yaml_path"], "/path/to/deployment")
        self.assertEqual(result["--output_path"], "/path/to/output")

    def test_parse_boot_args_partial(self):
        """Partial args override only specified values."""
        args = ["--conf_path", "/custom/conf"]
        result = ras_starter.parse_boot_args(args)
        self.assertEqual(result["--user_config_path"], "./user_config.json")
        self.assertEqual(result["--conf_path"], "/custom/conf")
        self.assertEqual(result["--deploy_yaml_path"], "./deployment")
        self.assertEqual(result["--output_path"], "./output")

    def test_parse_boot_args_unknown_raises(self):
        """Unknown boot key raises ValueError."""
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(["--unknown_arg", "value"])
        self.assertIn("Unknown boot argument", str(ctx.exception))

    def test_parse_boot_args_missing_value_raises(self):
        """Boot arg without value raises ValueError."""
        args = ["--user_config_path"]
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(args)
        self.assertIn("Invalid input args", str(ctx.exception))

    def test_parse_boot_args_orphan_token_raises(self):
        """Non-boot token is not skipped; raises ValueError."""
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(["orphan", "--conf_path", "/c"])
        self.assertIn("Unknown boot argument", str(ctx.exception))

    def test_parse_boot_args_short_flag_raises(self):
        """Short flags are not boot keys."""
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(["-x", "y"])
        self.assertIn("Unknown boot argument", str(ctx.exception))

    def test_parse_boot_args_prefix_match_unique(self):
        """Unique prefix resolves to full boot arg name."""
        args = ["--user", "/u.json"]
        result = ras_starter.parse_boot_args(args)
        self.assertEqual(result["--user_config_path"], "/u.json")

    def test_parse_boot_args_ambiguous_prefix_raises(self):
        """Prefix matching more than one key raises ValueError."""
        args = ["--", "/x"]
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(args)
        self.assertIn("mismatch", str(ctx.exception))

    def test_parse_boot_args_standalone_attach_raises(self):
        """--attach is not a boot arg; parse_boot_args rejects it if passed here."""
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(["--attach"])
        self.assertIn("Unknown boot argument", str(ctx.exception))

    def test_parse_boot_args_attach_prefixed_token_raises(self):
        """--attach=1 is not a boot arg (main uses parse_known_args to strip --attach first)."""
        with self.assertRaises(ValueError) as ctx:
            ras_starter.parse_boot_args(["--attach=1", "--conf_path", "/c"])
        self.assertIn("Unknown boot argument", str(ctx.exception))

    def test_parse_boot_args_conf_without_attach_tokens(self):
        """Typical boot_args after argparse only contain known pairs."""
        result = ras_starter.parse_boot_args(["--conf_path", "/c"])
        self.assertEqual(result["--conf_path"], "/c")
        self.assertEqual(result["--output_path"], "./output")

    def test_parse_boot_args_output_path_prefix(self):
        """--out prefix uniquely resolves to --output_path."""
        result = ras_starter.parse_boot_args(["--out", "/custom/out"])
        self.assertEqual(result["--output_path"], "/custom/out")


if __name__ == '__main__':
    unittest.main()
