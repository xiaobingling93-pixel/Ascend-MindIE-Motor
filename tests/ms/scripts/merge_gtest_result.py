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
import sys
import xml.etree.ElementTree as ET


def main():
    product = sys.argv[1]
    result_dir = sys.argv[2]
 
    merge_results(product, result_dir)


def merge_results(product, result_dir):
    failures = 0
    tests = 0
    errors = 0
    time = 0.0
    cases = []
    xml_files = []
    for _, _, files in os.walk(result_dir):
        for file in files:
            if file.endswith('.xml'):
                file_path = os.path.join(result_dir, file)
                xml_files.append(file_path)
        
    failures = 0
    disableds = 0
    tests = 0
    errors = 0
    time = 0.0
    for file_name in xml_files:
        tree = ET.parse(file_name)
        test_suite = tree.getroot()
        if 'failures' in test_suite.attrib.keys():
            failures += int(test_suite.attrib['failures'])
        if 'disabled' in test_suite.attrib.keys():
            disableds += int(test_suite.attrib['disabled'])
        if 'tests' in test_suite.attrib.keys():
            tests += int(test_suite.attrib['tests'])
        if 'errors' in test_suite.attrib.keys():
            errors += int(test_suite.attrib['errors'])
        if 'time' in test_suite.attrib.keys():
            time += float(test_suite.attrib['time'])
        cases.append(list(test_suite))
 
    merge_root = ET.Element('testsuites')
    # ʧ��������
    merge_root.attrib['failures'] = f'{failures}'
    merge_root.attrib['disabled'] = '0'
    merge_root.attrib['tests'] = f'{tests}'
    merge_root.attrib['errors'] = f'{errors}'
    merge_root.attrib['time'] = f'{time}'
    for case in cases:
        merge_root.extend(case)
    new_tree = ET.ElementTree(merge_root)
    new_tree.write(f"test_detail.xml", encoding='utf-8', xml_declaration=True, short_empty_elements=True)
 
if __name__ == '__main__':
    main()