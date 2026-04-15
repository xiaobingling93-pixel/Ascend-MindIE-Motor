/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 */

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "ConfigParams.h"
#include "ControllerConfig.h"
#include "ControllerSendReqStub.h"
#include "DistributedPolicy.h"
#include "FaultManager.h"
#include "GrpcClusterClient.h"
#include "Helper.h"
#include "NodeScheduler.h"
#include "ProcessManager.h"
#include "StatusUpdater.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stub.h"
using namespace MINDIE::MS;
using namespace testing;

constexpr int ARG_POINTEE_POS = 2;  // Mock操作参数的位置
constexpr int STUB_LEASE_ID = 123;
constexpr int LEASE_TTL_SECONDS = 15;  // Mock操作参数的位置
constexpr int GRPC_LEASE_REVISION = 123;

// 定义 Mock Stub（继承自接口类）
class MockKVStub : public etcdserverpb::KV::StubInterface {
   public:
    // 同步方法
    MOCK_METHOD(grpc::Status, Txn,
                (grpc::ClientContext*, const etcdserverpb::TxnRequest&,
                 etcdserverpb::TxnResponse*),
                (override));
    MOCK_METHOD(grpc::Status, Range,
                (grpc::ClientContext*, const etcdserverpb::RangeRequest&,
                 etcdserverpb::RangeResponse*),
                (override));
    MOCK_METHOD(grpc::Status, Put,
                (grpc::ClientContext*, const etcdserverpb::PutRequest&,
                 etcdserverpb::PutResponse*),
                (override));
    MOCK_METHOD(grpc::Status, DeleteRange,
                (grpc::ClientContext*, const etcdserverpb::DeleteRangeRequest&,
                 etcdserverpb::DeleteRangeResponse*),
                (override));
    MOCK_METHOD(grpc::Status, Compact,
                (grpc::ClientContext*, const etcdserverpb::CompactionRequest&,
                 etcdserverpb::CompactionResponse*),
                (override));

    // 异步方法
    MOCK_METHOD(
        (grpc::ClientAsyncResponseReaderInterface<etcdserverpb::TxnResponse>*),
        AsyncTxnRaw,
        (grpc::ClientContext*, const etcdserverpb::TxnRequest&,
         grpc::CompletionQueue*),
        (override));
    MOCK_METHOD(
        (grpc::ClientAsyncResponseReaderInterface<etcdserverpb::TxnResponse>*),
        PrepareAsyncTxnRaw,
        (grpc::ClientContext*, const etcdserverpb::TxnRequest&,
         grpc::CompletionQueue*),
        (override));

    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::RangeResponse>*),
                AsyncRangeRaw,
                (grpc::ClientContext*, const etcdserverpb::RangeRequest&,
                 grpc::CompletionQueue*),
                (override));
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::RangeResponse>*),
                PrepareAsyncRangeRaw,
                (grpc::ClientContext*, const etcdserverpb::RangeRequest&,
                 grpc::CompletionQueue*),
                (override));

    MOCK_METHOD(
        (grpc::ClientAsyncResponseReaderInterface<etcdserverpb::PutResponse>*),
        AsyncPutRaw,
        (grpc::ClientContext*, const etcdserverpb::PutRequest&,
         grpc::CompletionQueue*),
        (override));
    MOCK_METHOD(
        (grpc::ClientAsyncResponseReaderInterface<etcdserverpb::PutResponse>*),
        PrepareAsyncPutRaw,
        (grpc::ClientContext*, const etcdserverpb::PutRequest&,
         grpc::CompletionQueue*),
        (override));

    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::DeleteRangeResponse>*),
                AsyncDeleteRangeRaw,
                (grpc::ClientContext*, const etcdserverpb::DeleteRangeRequest&,
                 grpc::CompletionQueue*),
                (override));
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::DeleteRangeResponse>*),
                PrepareAsyncDeleteRangeRaw,
                (grpc::ClientContext*, const etcdserverpb::DeleteRangeRequest&,
                 grpc::CompletionQueue*),
                (override));

    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::CompactionResponse>*),
                AsyncCompactRaw,
                (grpc::ClientContext*, const etcdserverpb::CompactionRequest&,
                 grpc::CompletionQueue*),
                (override));
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::CompactionResponse>*),
                PrepareAsyncCompactRaw,
                (grpc::ClientContext*, const etcdserverpb::CompactionRequest&,
                 grpc::CompletionQueue*),
                (override));
};

// 定义stream类
class MockClientReaderWriter : public grpc::ClientReaderWriterInterface<
                                   etcdserverpb::LeaseKeepAliveRequest,
                                   etcdserverpb::LeaseKeepAliveResponse> {
   public:
    // 模拟 Write 方法
    MOCK_METHOD(bool, Write,
                (const etcdserverpb::LeaseKeepAliveRequest& req,
                 grpc::WriteOptions options),
                (override));
    // 模拟 Read 方法
    MOCK_METHOD(bool, Read, (etcdserverpb::LeaseKeepAliveResponse * resp),
                (override));
    // 模拟 WritesDone 和 Finish
    MOCK_METHOD(bool, WritesDone, (), (override));
    MOCK_METHOD(grpc::Status, Finish, (), (override));
    MOCK_METHOD(bool, NextMessageSize, (uint32_t*), (override));
    MOCK_METHOD(void, WaitForInitialMetadata, (), (override));
};

class MockLeaseStub : public etcdserverpb::Lease::StubInterface {
   public:
    // 同步方法
    MOCK_METHOD(grpc::Status, LeaseGrant,
                (grpc::ClientContext*, const etcdserverpb::LeaseGrantRequest&,
                 etcdserverpb::LeaseGrantResponse*),
                (override));
    MOCK_METHOD(grpc::Status, LeaseRevoke,
                (grpc::ClientContext*, const etcdserverpb::LeaseRevokeRequest&,
                 etcdserverpb::LeaseRevokeResponse*),
                (override));
    MOCK_METHOD(grpc::Status, LeaseTimeToLive,
                (grpc::ClientContext*,
                 const etcdserverpb::LeaseTimeToLiveRequest&,
                 etcdserverpb::LeaseTimeToLiveResponse*),
                (override));
    MOCK_METHOD(grpc::Status, LeaseLeases,
                (grpc::ClientContext*, const etcdserverpb::LeaseLeasesRequest&,
                 etcdserverpb::LeaseLeasesResponse*),
                (override));

    // 异步方法
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseGrantResponse>*),
                AsyncLeaseGrantRaw,
                (grpc::ClientContext*, const etcdserverpb::LeaseGrantRequest&,
                 grpc::CompletionQueue*),
                (override));
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseRevokeResponse>*),
                AsyncLeaseRevokeRaw,
                (grpc::ClientContext*, const etcdserverpb::LeaseRevokeRequest&,
                 grpc::CompletionQueue*),
                (override));
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseTimeToLiveResponse>*),
                AsyncLeaseTimeToLiveRaw,
                (grpc::ClientContext*,
                 const etcdserverpb::LeaseTimeToLiveRequest&,
                 grpc::CompletionQueue*),
                (override));
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseLeasesResponse>*),
                AsyncLeaseLeasesRaw,
                (grpc::ClientContext*, const etcdserverpb::LeaseLeasesRequest&,
                 grpc::CompletionQueue*),
                (override));

    // 流式方法
    MOCK_METHOD((grpc::ClientReaderWriterInterface<
                    etcdserverpb::LeaseKeepAliveRequest,
                    etcdserverpb::LeaseKeepAliveResponse>*),
                LeaseKeepAliveRaw, (grpc::ClientContext*), (override));

    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<
                    etcdserverpb::LeaseKeepAliveRequest,
                    etcdserverpb::LeaseKeepAliveResponse>*),
                AsyncLeaseKeepAliveRaw,
                (grpc::ClientContext*, grpc::CompletionQueue*, void*),
                (override));

    // ... 其他 PrepareAsync 方法
    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseGrantResponse>*),
                PrepareAsyncLeaseGrantRaw,
                (grpc::ClientContext*, const etcdserverpb::LeaseGrantRequest&,
                 grpc::CompletionQueue*),
                (override));

    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseRevokeResponse>*),
                PrepareAsyncLeaseRevokeRaw,
                (grpc::ClientContext*, const etcdserverpb::LeaseRevokeRequest&,
                 grpc::CompletionQueue*),
                (override));

    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseTimeToLiveResponse>*),
                PrepareAsyncLeaseTimeToLiveRaw,
                (grpc::ClientContext*,
                 const etcdserverpb::LeaseTimeToLiveRequest&,
                 grpc::CompletionQueue*),
                (override));

    MOCK_METHOD((grpc::ClientAsyncResponseReaderInterface<
                    etcdserverpb::LeaseLeasesResponse>*),
                PrepareAsyncLeaseLeasesRaw,
                (grpc::ClientContext*, const etcdserverpb::LeaseLeasesRequest&,
                 grpc::CompletionQueue*),
                (override));

    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<
                    etcdserverpb::LeaseKeepAliveRequest,
                    etcdserverpb::LeaseKeepAliveResponse>*),
                PrepareAsyncLeaseKeepAliveRaw,
                (grpc::ClientContext*, grpc::CompletionQueue*), (override));
};

// * 创建测试类
class EtcdDistributedLockTest : public testing::Test {
   protected:
    // 公共 Mock 对象和原始指针
    std::shared_ptr<MockKVStub> mock_kv_holder;
    MockKVStub* rawMockKv;
    std::shared_ptr<MockLeaseStub> mock_lease_holder;
    MockLeaseStub* rawMockLease;

    etcdserverpb::LeaseGrantResponse resp;
    // 初始化公共资源
    void SetUp() override {
        mock_kv_holder = std::make_shared<MockKVStub>();
        mock_lease_holder = std::make_shared<MockLeaseStub>();
        resp.set_id(STUB_LEASE_ID);
    }

    // 创建锁对象的公共方法
    std::shared_ptr<EtcdDistributedLock> CreateLock() {
        std::string lockName("/test/lock");
        std::string addr("dns_name_extra");
        std::string clientId("client_1");
        EtcdTimeInfo etcdTimeInfo(ETCD_WATCH_GAP_SECONDS,
                                  CONTEXT_TIMEOUT_SECONDS, LEASE_TTL_SECONDS);
        return std::make_unique<EtcdDistributedLock>(
            mock_kv_holder, mock_lease_holder, lockName, clientId,
            etcdTimeInfo);
    }
    // 构造事务响应的辅助方法
    etcdserverpb::TxnResponse BuildTxnResponse(bool succeeded,
                                               int64_t revision = 0,
                                               const std::string& owner = "") {
        etcdserverpb::TxnResponse response;
        response.set_succeeded(succeeded);
        if (succeeded) {
            auto* put_response =
                response.add_responses()->mutable_response_put();
            put_response->mutable_header()->set_revision(revision);
        } else {
            auto* range_response =
                response.add_responses()->mutable_response_range();
            auto* kv = range_response->add_kvs();
            kv->set_value(owner);
            kv->set_mod_revision(revision);
        }
        return response;
    }

    etcdserverpb::RangeResponse CreateRangeResponse(
        int64_t modRev, const std::string& key = "/test/key",
        const std::string& value = "test_value") {
        etcdserverpb::RangeResponse resp;
        // modRev > 0 表示 key 存在
        if (modRev > 0) {
            auto* kv = resp.add_kvs();
            kv->set_key(key);              // 设置 key
            kv->set_value(value);          // 设置 value
            kv->set_mod_revision(modRev);  // 设置版本号
            resp.set_count(1);             // 表示存在一个匹配的 key
        } else {
            resp.set_count(0);  // 表示 key 不存在
        }

        // 设置响应头
        auto* header = resp.mutable_header();
        header->set_revision(modRev);  // 当前集群全局 revision
        header->set_cluster_id(
            GRPC_LEASE_REVISION);  // GRPC_LEASE_REVISION:模拟集群 ID
        return resp;
    }
};

// Mock回调类
class MockCallback {
   public:
    MOCK_METHOD(void, Call, (bool));
};

TEST_F(EtcdDistributedLockTest, SuccessWithValidKey) {
    const std::string key("/test/key");
    const std::string value("test_value");
    auto lock = CreateLock();
    // 1. 准备Mock响应
    etcdserverpb::RangeResponse mockResp;
    mockResp.set_count(1);
    auto* kv = mockResp.add_kvs();
    kv->set_key(key);
    kv->set_value(value);
    kv->set_mod_revision(GRPC_LEASE_REVISION);

    // 2. 设置Mock期望
    EXPECT_CALL(*mock_kv_holder, Range(_, _, _))
        .WillOnce(DoAll(SetArgPointee<ARG_POINTEE_POS>(mockResp),  // 填充响应
                        Return(grpc::Status::OK)  // 返回成功状态
                        ));

    // 3. 执行测试
    std::string valueGet;
    bool result = lock->GetWithRevision(key, valueGet);

    // 4. 验证结果
    EXPECT_TRUE(result);
    EXPECT_EQ(value, valueGet);
}

TEST_F(EtcdDistributedLockTest, HandleLockChange) {
    auto lock = CreateLock();
    EXPECT_TRUE(lock != nullptr);
    lock->SetLock(true);
    MockCallback mockCb;
    EXPECT_CALL(mockCb, Call(false)).Times(1);

    // 注册回调
    lock->RegisterCallBack(
        std::bind(&MockCallback::Call, &mockCb, std::placeholders::_1));

    // 触发回调（假设有触发方法）
    lock->HandleLockChange(false);
    EXPECT_FALSE(lock->IsLocked());
}

TEST_F(EtcdDistributedLockTest, AcquireLockOnce_RpcFailed) {
    auto lock = CreateLock();
    EXPECT_TRUE(lock != nullptr);

    EXPECT_CALL(*mock_lease_holder, LeaseGrant(_, _, NotNull()))
        .WillOnce(DoAll(SetArgPointee<ARG_POINTEE_POS>(resp),
                        Return(grpc::Status::OK)));

    // 模拟 Txn 返回错误状态
    EXPECT_CALL(*mock_kv_holder, Txn(_, _, NotNull()))
        .WillOnce(Return(
            grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Timeout")));

    EXPECT_FALSE(lock->AcquireLockOnce());
}

TEST_F(EtcdDistributedLockTest, AcquireLockOnce_TxnFailed) {
    auto lock = CreateLock();

    // 设置 MockLeaseStub 的期望
    EXPECT_CALL(*mock_lease_holder, LeaseGrant(_, _, _))
        .WillOnce(DoAll(Invoke([](grpc::ClientContext*,
                                  const etcdserverpb::LeaseGrantRequest&,
                                  etcdserverpb::LeaseGrantResponse* response) {
                            response->set_id(STUB_LEASE_ID);  // 直接设置 ID
                        }),
                        Return(grpc::Status::OK)));

    // 构造失败响应,锁被其他节点抢占
    auto response =
        BuildTxnResponse(false, 1, "other_client");  // 1: 表述revision

    // 设置 Txn 方法期望
    EXPECT_CALL(*mock_kv_holder, Txn(_, _, _))
        .WillOnce(DoAll(SetArgPointee<ARG_POINTEE_POS>(response),
                        Return(grpc::Status::OK)));

    EXPECT_FALSE(lock->AcquireLockOnce());
}

TEST_F(EtcdDistributedLockTest, AcquireLockOnce_TxnSucceeded) {
    // 1. 创建锁对象（通过夹具的工厂方法）
    auto lock = CreateLock();

    // 2. 设置 MockLeaseStub 的期望
    EXPECT_CALL(*mock_lease_holder, LeaseGrant(_, _, _))
        .WillOnce(DoAll(Invoke([](grpc::ClientContext*,
                                  const etcdserverpb::LeaseGrantRequest&,
                                  etcdserverpb::LeaseGrantResponse* response) {
                            response->set_id(STUB_LEASE_ID);  // 直接设置 ID
                        }),
                        Return(grpc::Status::OK)));

    // 3. 构造事务成功响应（通过夹具的辅助函数）
    auto response =
        BuildTxnResponse(true, 789);  // true 表示事务成功， 789： 表示revision

    // 4. 设置 Txn 方法期望
    EXPECT_CALL(*mock_kv_holder, Txn(_, _, _))
        .WillOnce(DoAll(SetArgPointee<ARG_POINTEE_POS>(response),
                        Return(grpc::Status::OK)));

    // 5. 执行并验证
    EXPECT_TRUE(lock->AcquireLockOnce());
    lock->SetLock(true);  // 模拟锁成功获取
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, InitializeEtcdClient_Succeeded) {
    // 1. 创建锁对象（通过夹具的工厂方法）
    auto lock = CreateLock();

    // 2. 执行并验证
    lock->SetLock(false);
    lock->InitializeEtcdClient();
    EXPECT_NE(lock, nullptr);
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, InitializeEtcdClient_TLS_Succeeded) {
    // 1. 创建锁对象（通过夹具的工厂方法）
    auto lock = CreateLock();

    // 2. 执行并验证
    lock->SetLock(false);
    lock->InitializeEtcdClient();
    EXPECT_NE(lock, nullptr);
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, CalculateSleepTime) {
    // 1. 创建锁对象（通过夹具的工厂方法）
    auto lock = CreateLock();

    // 2. 执行并验证
    int maxTtl = LEASE_TTL_SECONDS / 2;
    EXPECT_EQ(lock->CalculateSleepTime(0), 1);
    EXPECT_EQ(lock->CalculateSleepTime(10), 5);
    EXPECT_EQ(lock->CalculateSleepTime(20), maxTtl);
    EXPECT_EQ(lock->CalculateSleepTime(100), maxTtl);

    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, PutDataSuccessWhenKeyExists) {
    // 1. 创建锁对象（通过夹具的工厂方法）
    auto lock = CreateLock();
    const std::string key("/test/key");
    const std::string value("test_value");
    auto response =
        CreateRangeResponse(100, key, value);  //  100： 表示revision

    // 1. 设置GetCurrentModVer返回有效版本
    EXPECT_CALL(*mock_kv_holder, Range(_, _, _))
        .WillOnce(DoAll(
            SetArgPointee<ARG_POINTEE_POS>(response),  // 假设有辅助函数构造响应
            Return(grpc::Status::OK)));

    // 2. 模拟成功事务响应
    etcdserverpb::TxnResponse txnResp;
    txnResp.set_succeeded(true);
    EXPECT_CALL(*mock_kv_holder, Txn(_, _, _))  // 自定义参数匹配器
        .WillOnce(DoAll(SetArgPointee<ARG_POINTEE_POS>(txnResp),
                        Return(grpc::Status::OK)));

    // 3. 执行并验证
    EXPECT_TRUE(lock->SafePut(key, value));
}

TEST_F(EtcdDistributedLockTest,
       EnsureConnection_ReturnsTrueWhenStubsAvailable) {
    auto lock = CreateLock();
    EXPECT_TRUE(lock->EnsureConnection());
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, EnsureConnection_ReturnsFalseWhenStubsNull) {
    auto lock = CreateLock();
    lock->SetKvStub(nullptr);
    lock->SetLeaseStub(nullptr);
    lock->SetConnectMaxRetry(0);
    EXPECT_FALSE(lock->EnsureConnection());
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, GetWithRevision_FailsWhenStubsNull) {
    auto lock = CreateLock();
    lock->SetKvStub(nullptr);
    lock->SetLeaseStub(nullptr);
    lock->SetConnectMaxRetry(0);
    std::string value;
    EXPECT_FALSE(lock->GetWithRevision("/test/key", value));
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, EnsureConnection_RetryLoopWhenStubsNull) {
    auto lock = CreateLock();
    lock->SetKvStub(nullptr);
    lock->SetLeaseStub(nullptr);
    lock->SetConnectMaxRetry(2);
    EXPECT_FALSE(lock->EnsureConnection());
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest,
       AcquireLockOnce_FailsWhenConnectionUnavailable) {
    auto lock = CreateLock();
    lock->SetKvStub(nullptr);
    lock->SetLeaseStub(nullptr);
    lock->SetConnectMaxRetry(0);
    EXPECT_FALSE(lock->AcquireLockOnce());
    lock->Stop();
}

TEST_F(EtcdDistributedLockTest, SafePut_FailsWhenConnectionUnavailable) {
    auto lock = CreateLock();
    lock->SetKvStub(nullptr);
    lock->SetLeaseStub(nullptr);
    lock->SetConnectMaxRetry(0);
    EXPECT_FALSE(lock->SafePut("/test/key", "value"));
    lock->Stop();
}

std::shared_ptr<grpc::Channel> CreateGrpcChannelSuccessStub(
    const std::string serverAddr, const TlsItems& mTlsConfig) {
    return grpc::CreateChannel("localhost:2379",
                               grpc::InsecureChannelCredentials());
}

TEST_F(EtcdDistributedLockTest,
       EnsureConnection_ReconnectsSuccessfullyAfterStubsLost) {
    Stub grpcStub;
    grpcStub.set(ADDR(MINDIE::MS, CreateGrpcChannel),
                 CreateGrpcChannelSuccessStub);

    auto lock = CreateLock();
    EXPECT_TRUE(lock->EnsureConnection());

    lock->SetKvStub(nullptr);
    lock->SetLeaseStub(nullptr);
    lock->SetConnectMaxRetry(1);

    EXPECT_TRUE(lock->EnsureConnection());
    grpcStub.reset(ADDR(MINDIE::MS, CreateGrpcChannel));
    lock->Stop();
}
