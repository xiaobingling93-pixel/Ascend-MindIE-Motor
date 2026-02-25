/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <iostream>
#include "gtest/gtest.h"

int main(int argc, char *argv[])
{
    std::string timelimit("--timelimit");   // 该参数仅在fuzz时有效
    std::string fuzzcount("--fuzzcount");   // 该参数仅在fuzz时有效

    testing::GTEST_FLAG(output) = "xml:"; // 若要生成xml结果文件
    testing::InitGoogleTest(&argc, argv); // 初始化

    int32_t errcode = RUN_ALL_TESTS(); // 跑单元测试;
    if (errcode != 0) {
        std::cout << "Some Test Case Fail!" << std::endl;
    }
    return errcode;
}
