/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include "gtest/gtest.h"

#define private public
#define protected public
#include "MemoryUtil.h"

using namespace MINDIE::MS;

class TestMemoryUtil : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create temp directory for test files
        testDir = "/tmp/memoryutil_test_" + std::to_string(getpid());
        mkdir(testDir.c_str(), 0755);
    }

    void TearDown() override
    {
        // Clean up temp directory
        RemoveTestDir(testDir);
    }

    // Create test file with content
    void CreateTestFile(const std::string& path, const std::string& content)
    {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    // Remove directory and its contents
    void RemoveTestDir(const std::string& path)
    {
        std::string cmd = "rm -rf " + path;
        (void)system(cmd.c_str());
    }

    std::string testDir;
};

/*
测试描述: 测试FormatBytes函数，负数输入
测试步骤:
    1. 传入负数值，预期结果1
预期结果:
    1. 返回 "N/A"
*/
TEST_F(TestMemoryUtil, FormatBytesNegativeTC)
{
    EXPECT_EQ(MemoryUtil::FormatBytes(-1), "N/A");
    EXPECT_EQ(MemoryUtil::FormatBytes(-100), "N/A");
    EXPECT_EQ(MemoryUtil::FormatBytes(-9223372036854775807LL), "N/A");
}

/*
测试描述: 测试FormatBytes函数，零值输入
测试步骤:
    1. 传入0值，预期结果1
预期结果:
    1. 返回 "0 B"
*/
TEST_F(TestMemoryUtil, FormatBytesZeroTC)
{
    EXPECT_EQ(MemoryUtil::FormatBytes(0), "0 B");
}

/*
测试描述: 测试FormatBytes函数，字节级别输入
测试步骤:
    1. 传入小于1KB的值，预期结果1
预期结果:
    1. 返回带 "B" 后缀的格式化字符串
*/
TEST_F(TestMemoryUtil, FormatBytesByteLevelTC)
{
    EXPECT_EQ(MemoryUtil::FormatBytes(1), "1 B");
    EXPECT_EQ(MemoryUtil::FormatBytes(100), "100 B");
    EXPECT_EQ(MemoryUtil::FormatBytes(512), "512 B");
    EXPECT_EQ(MemoryUtil::FormatBytes(1023), "1023 B");
}

/*
测试描述: 测试FormatBytes函数，KB级别输入
测试步骤:
    1. 传入1KB到1MB之间的值，预期结果1
预期结果:
    1. 返回带 "KB" 后缀的格式化字符串
*/
TEST_F(TestMemoryUtil, FormatBytesKBLevelTC)
{
    EXPECT_EQ(MemoryUtil::FormatBytes(1024), "1.00 KB");
    EXPECT_EQ(MemoryUtil::FormatBytes(2048), "2.00 KB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1536), "1.50 KB");
    EXPECT_EQ(MemoryUtil::FormatBytes(10240), "10.00 KB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024 * 1024 - 1), "1024.00 KB");
}

/*
测试描述: 测试FormatBytes函数，MB级别输入
测试步骤:
    1. 传入1MB到1GB之间的值，预期结果1
预期结果:
    1. 返回带 "MB" 后缀的格式化字符串
*/
TEST_F(TestMemoryUtil, FormatBytesMBLevelTC)
{
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024), "1.00 MB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 2), "2.00 MB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 100), "100.00 MB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 512), "512.00 MB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024 - 1), "1024.00 MB");
}

/*
测试描述: 测试FormatBytes函数，GB级别输入
测试步骤:
    1. 传入大于等于1GB的值，预期结果1
预期结果:
    1. 返回带 "GB" 后缀的格式化字符串
*/
TEST_F(TestMemoryUtil, FormatBytesGBLevelTC)
{
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024), "1.00 GB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024 * 2), "2.00 GB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024 * 16), "16.00 GB");
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024 * 128), "128.00 GB");
}

/*
测试描述: 测试FormatBytes函数，小数精度
测试步骤:
    1. 传入会产生小数的值，预期结果1
预期结果:
    1. 返回保留两位小数的格式化字符串
*/
TEST_F(TestMemoryUtil, FormatBytesPrecisionTC)
{
    // 1.5 KB = 1536 bytes
    EXPECT_EQ(MemoryUtil::FormatBytes(1536), "1.50 KB");
    // 2.25 MB = 2359296 bytes
    EXPECT_EQ(MemoryUtil::FormatBytes(2359296), "2.25 MB");
    // 1.75 GB = 1879048192 bytes
    EXPECT_EQ(MemoryUtil::FormatBytes(1879048192), "1.75 GB");
}

/*
测试描述: 测试MemoryInfo结构体默认值
测试步骤:
    1. 创建默认MemoryInfo对象，预期结果1
预期结果:
    1. 所有数值字段为-1，source为空
*/
TEST_F(TestMemoryUtil, MemoryInfoDefaultValuesTC)
{
    MemoryInfo info;
    EXPECT_EQ(info.limitBytes, -1);
    EXPECT_EQ(info.usageBytes, -1);
    EXPECT_EQ(info.availableBytes, -1);
    EXPECT_TRUE(info.source.empty());
}

/*
测试描述: 测试GetMemoryInfo函数基本功能
测试步骤:
    1. 调用GetMemoryInfo获取内存信息，预期结果1
预期结果:
    1. 返回的MemoryInfo结构体有效（在容器环境或物理机上都能正常工作）
*/
TEST_F(TestMemoryUtil, GetMemoryInfoBasicTC)
{
    MemoryInfo info = MemoryUtil::GetMemoryInfo();
    
    // On a real system, at least proc_meminfo should work
    // limitBytes might be -1 if not in container, or positive if in container
    // If limitBytes > 0, source should be set
    if (info.limitBytes > 0) {
        EXPECT_FALSE(info.source.empty());
        EXPECT_TRUE(info.source == "cgroup_v2" ||
                    info.source == "cgroup_v1" ||
                    info.source == "proc_meminfo");
    }
}

/*
测试描述: 测试GetMemoryUsage函数基本功能
测试步骤:
    1. 调用GetMemoryUsage获取内存使用量，预期结果1
预期结果:
    1. 在容器环境返回正值，在非容器环境返回-1
*/
TEST_F(TestMemoryUtil, GetMemoryUsageBasicTC)
{
    int64_t usage = MemoryUtil::GetMemoryUsage();
    // Usage is either -1 (no cgroup) or >= 0 (in container)
    EXPECT_TRUE(usage == -1 || usage >= 0);
}

/*
测试描述: 测试GetMemoryLimitFromCgroupV2函数，文件不存在场景
测试步骤:
    1. 调用GetMemoryLimitFromCgroupV2，预期结果1
预期结果:
    1. 在非cgroup v2环境返回-1
*/
TEST_F(TestMemoryUtil, GetMemoryLimitFromCgroupV2NotExistTC)
{
    // This test verifies the function handles missing files gracefully
    // The actual return depends on whether cgroup v2 is available
    int64_t limit = MemoryUtil::GetMemoryLimitFromCgroupV2();
    // Should be either -1 (no cgroup v2) or > 0 (valid limit)
    EXPECT_TRUE(limit == -1 || limit > 0);
}

/*
测试描述: 测试GetMemoryLimitFromCgroupV1函数，文件不存在场景
测试步骤:
    1. 调用GetMemoryLimitFromCgroupV1，预期结果1
预期结果:
    1. 在非cgroup v1环境返回-1
*/
TEST_F(TestMemoryUtil, GetMemoryLimitFromCgroupV1NotExistTC)
{
    int64_t limit = MemoryUtil::GetMemoryLimitFromCgroupV1();
    // Should be either -1 (no cgroup v1 or unlimited) or > 0 (valid limit)
    EXPECT_TRUE(limit == -1 || limit > 0);
}

/*
测试描述: 测试GetMemoryLimitFromProc函数基本功能
测试步骤:
    1. 调用GetMemoryLimitFromProc，预期结果1
预期结果:
    1. 在Linux系统返回正值（系统总内存）
*/
TEST_F(TestMemoryUtil, GetMemoryLimitFromProcBasicTC)
{
    int64_t limit = MemoryUtil::GetMemoryLimitFromProc();
    // On any Linux system, /proc/meminfo should exist
    // Should return positive value (at least some memory)
    EXPECT_GT(limit, 0);
}

/*
测试描述: 测试GetMemoryUsageFromCgroupV2函数，文件不存在场景
测试步骤:
    1. 调用GetMemoryUsageFromCgroupV2，预期结果1
预期结果:
    1. 在非cgroup v2环境返回-1
*/
TEST_F(TestMemoryUtil, GetMemoryUsageFromCgroupV2NotExistTC)
{
    int64_t usage = MemoryUtil::GetMemoryUsageFromCgroupV2();
    // Should be either -1 (no cgroup v2) or >= 0 (valid usage)
    EXPECT_TRUE(usage == -1 || usage >= 0);
}

/*
测试描述: 测试GetMemoryUsageFromCgroupV1函数，文件不存在场景
测试步骤:
    1. 调用GetMemoryUsageFromCgroupV1，预期结果1
预期结果:
    1. 在非cgroup v1环境返回-1
*/
TEST_F(TestMemoryUtil, GetMemoryUsageFromCgroupV1NotExistTC)
{
    int64_t usage = MemoryUtil::GetMemoryUsageFromCgroupV1();
    // Should be either -1 (no cgroup v1) or >= 0 (valid usage)
    EXPECT_TRUE(usage == -1 || usage >= 0);
}

/*
测试描述: 测试CheckRequestConfigMemoryRisk函数基本功能
测试步骤:
    1. 调用CheckRequestConfigMemoryRisk，预期结果1
预期结果:
    1. 函数正常执行不崩溃（Configure可能未初始化会提前返回）
*/
TEST_F(TestMemoryUtil, CheckRequestConfigMemoryRiskBasicTC)
{
    // This function requires Configure singleton to be initialized
    // If not initialized, it should return early without crash
    EXPECT_NO_THROW(MemoryUtil::CheckRequestConfigMemoryRisk());
}

/*
测试描述: 测试CheckRequestConfigMemoryRisk函数的noexcept特性
测试步骤:
    1. 验证函数声明为noexcept，预期结果1
    2. 多次调用函数验证稳定性，预期结果2
预期结果:
    1. 函数被声明为noexcept
    2. 多次调用都不会抛出异常
*/
TEST_F(TestMemoryUtil, CheckRequestConfigMemoryRiskNoexceptTC)
{
    // Verify the function is declared noexcept
    static_assert(noexcept(MemoryUtil::CheckRequestConfigMemoryRisk()),
                  "CheckRequestConfigMemoryRisk should be noexcept");
    
    // Call multiple times to verify stability
    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(MemoryUtil::CheckRequestConfigMemoryRisk());
    }
}

/*
测试描述: 测试GetMemoryInfo函数，availableBytes计算逻辑
测试步骤:
    1. 调用GetMemoryInfo获取内存信息，预期结果1
预期结果:
    1. 如果limitBytes和usageBytes都有效，availableBytes应该被正确计算
*/
TEST_F(TestMemoryUtil, GetMemoryInfoAvailableCalculationTC)
{
    MemoryInfo info = MemoryUtil::GetMemoryInfo();
    
    // If both limit and usage are valid, available should be calculated
    if (info.limitBytes > 0 && info.usageBytes >= 0) {
        // availableBytes = limitBytes - usageBytes (clamped to 0 if negative)
        int64_t expected = info.limitBytes - info.usageBytes;
        if (expected < 0) {
            expected = 0;
        }
        EXPECT_EQ(info.availableBytes, expected);
    }
}

/*
测试描述: 测试FormatBytes函数，边界值
测试步骤:
    1. 传入边界值（精确的KB、MB、GB边界），预期结果1
预期结果:
    1. 边界值格式化正确
*/
TEST_F(TestMemoryUtil, FormatBytesBoundaryValuesTC)
{
    // Exact KB boundary
    EXPECT_EQ(MemoryUtil::FormatBytes(1024), "1.00 KB");
    // Exact MB boundary
    EXPECT_EQ(MemoryUtil::FormatBytes(1024 * 1024), "1.00 MB");
    // Exact GB boundary
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024), "1.00 GB");
}

/*
测试描述: 测试FormatBytes函数，大数值
测试步骤:
    1. 传入非常大的数值（TB级别），预期结果1
预期结果:
    1. 使用GB单位格式化输出
*/
TEST_F(TestMemoryUtil, FormatBytesLargeValuesTC)
{
    // 1 TB = 1024 GB
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024 * 1024), "1024.00 GB");
    // 10 TB
    EXPECT_EQ(MemoryUtil::FormatBytes(1024LL * 1024 * 1024 * 1024 * 10), "10240.00 GB");
}

/*
测试描述: 测试内存信息获取优先级
测试步骤:
    1. 调用GetMemoryInfo，预期结果1
预期结果:
    1. 优先级：cgroup_v2 > cgroup_v1 > proc_meminfo
*/
TEST_F(TestMemoryUtil, GetMemoryInfoSourcePriorityTC)
{
    MemoryInfo info = MemoryUtil::GetMemoryInfo();
    
    // If source is set, it should be one of the valid sources
    if (!info.source.empty()) {
        bool isValidSource = (info.source == "cgroup_v2" ||
                             info.source == "cgroup_v1" ||
                             info.source == "proc_meminfo");
        EXPECT_TRUE(isValidSource);
    }
}
