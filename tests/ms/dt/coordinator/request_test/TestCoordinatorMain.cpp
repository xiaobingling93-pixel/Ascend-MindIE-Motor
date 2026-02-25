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
#include <fstream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <string>
#include <memory>
#include <map>
#include <random>
#include <unistd.h>
#include <variant>
#include <vector>
#include <stub.h>
#include <pthread.h>
#include "gtest/gtest.h"
#include "Configure.h"
#define main __main_coor__
#include "coordinator/main.cpp"
#include "Helper.h"
#include "JsonFileManager.h"
using namespace MINDIE::MS;

using CommandLine = std::vector<std::string>;
using CommandExp = std::tuple<CommandLine, int>;

static int CallMain(CommandLine command)
{
    // Ensure there's a dummy argv[0]
    command.insert(command.begin(), "dummy_program");

    // Set argc to the size of the command vector
    auto argc = command.size();
    
    // Convert std::vector<std::string> to char* array
    std::vector<const char*> argv(argc);
    std::transform(command.begin(), command.end(), argv.begin(), [](const std::string& str) {
        return str.c_str();
    });

    // Call the main function
    auto ret = __main_coor__(argc, const_cast<char**>(argv.data()));
    return ret;
}

class TestCoordinatorMain : public ::testing::Test {
protected:
    void SetUp()
    {
        // Set up the test environment
        CopyDefaultConfig();
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
    }
};

class TestCoordinatorMainCommand : public TestCoordinatorMain,
    public ::testing::WithParamInterface<CommandExp> {};

TEST_F(TestCoordinatorMain, TestMainTCa)
{
    Stub stub;
    stub.set(ADDR(MINDIE::MS::Coordinator, Run), ReturnZeroStub);
    std::string predIp = "127.0.0.1";
    std::string managementIp = "127.0.0.1";

    CommandLine command = {predIp, managementIp};
    CallMain(command);

    EXPECT_EQ(Configure::Singleton()->httpConfig.predIp, predIp);
    EXPECT_EQ(Configure::Singleton()->httpConfig.managementIp, managementIp);
}

TEST_P(TestCoordinatorMainCommand, TestMainTCa)
{
    Stub stub;
    stub.set(ADDR(MINDIE::MS::Coordinator, Run), ReturnZeroStub);

    auto command = std::get<0>(GetParam());
    auto exp = std::get<1>(GetParam());
    auto ret = CallMain(command);
    EXPECT_EQ(ret, exp);
}

INSTANTIATE_TEST_SUITE_P(,
    TestCoordinatorMainCommand,
    ::testing::Values(
        CommandExp{{"192.168.1.1.5"}, -1},
        CommandExp{{"256.100.50.25"}, -1},
        CommandExp{{"192.168.1"}, -1},
        CommandExp{{"192.abc.1.5"}, -1},
        CommandExp{{"..1.1"}, -1},
        CommandExp{{"192.168.0.1", "70000"}, -1},
        CommandExp{{"192.168.0.1", "-1"}, -1},
        CommandExp{{"192.168.0.1", "abc"}, -1},
        CommandExp{{"192.168.0.1", "80.5"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.1.1.5"}, -1},
        CommandExp{{"192.168.0.1", "8080", "256.100.50.25"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.1"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.abc.1.5"}, -1},
        CommandExp{{"192.168.0.1", "8080", "..1.1"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.0.2", "70000"}, -1},
        CommandExp{{"192.168.0.1", "abc", "192.168.0.2", "8081"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.0.2", "-1"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.0.2", "abc"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.0.2", "80.5"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.0.2", "8080", "192.168.1.5"}, -1},
        CommandExp{{"192.168.0.1", "8080", "192.168.0.2", "8080", "192.168.1.5", "8080"}, -1}
    )
);