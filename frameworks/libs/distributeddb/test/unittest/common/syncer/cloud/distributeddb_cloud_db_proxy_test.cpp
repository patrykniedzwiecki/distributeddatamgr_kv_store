/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>

#include <utility>
#include "cloud_db_constant.h"
#include "cloud_db_data_utils.h"
#include "cloud_db_types.h"
#include "cloud_db_proxy.h"
#include "distributeddb_tools_unit_test.h"
#include "mock_icloud_sync_storage_interface.h"
#include "virtual_cloud_db.h"
#include "virtual_cloud_syncer.h"

using namespace std;
using namespace testing::ext;
using namespace DistributedDB;

namespace {
constexpr const char *TABLE_NAME = "Table";
std::vector<Field> GetFields()
{
    return {
        {
            .colName = "col1",
            .type = TYPE_INDEX<int64_t>,
            .primary = true,
            .nullable = false
        },
        {
            .colName = "col2",
            .type = TYPE_INDEX<std::string>,
            .primary = false
        },
        {
            .colName = "col3",
            .type = TYPE_INDEX<Bytes>,
            .primary = false
        }
    };
}

void ModifyRecords(std::vector<VBucket> &expectRecord)
{
    std::vector<VBucket> tempRecord;
    for (const auto &record: expectRecord) {
        VBucket bucket;
        for (auto &[field, val] : record) {
            LOGD("modify field %s", field.c_str());
            if (val.index() == TYPE_INDEX<int64_t>) {
                int64_t v = std::get<int64_t>(val);
                bucket.insert({ field, static_cast<int64_t>(v + 1) });
            } else {
                bucket.insert({ field, val });
            }
        }
        tempRecord.push_back(bucket);
    }
    expectRecord = tempRecord;
}

DBStatus Sync(CloudSyncer *cloudSyncer)
{
    std::mutex processMutex;
    std::condition_variable cv;
    SyncProcess syncProcess;
    const auto callback = [&syncProcess, &processMutex, &cv](const std::map<std::string, SyncProcess> &process) {
        {
            std::lock_guard<std::mutex> autoLock(processMutex);
            syncProcess = std::move(process.begin()->second);
            if (process.size() >= 1u) {
                syncProcess = process.begin()->second;
            } else {
                SyncProcess tmpProcess;
                syncProcess = tmpProcess;
            }
        }
        cv.notify_all();
    };
    EXPECT_EQ(cloudSyncer->Sync({ "cloud" }, SyncMode::SYNC_MODE_CLOUD_MERGE, { TABLE_NAME }, callback, 0), E_OK);
    {
        LOGI("begin to wait sync");
        std::unique_lock<std::mutex> uniqueLock(processMutex);
        cv.wait(uniqueLock, [&syncProcess]() {
            return syncProcess.process == ProcessStatus::FINISHED;
        });
        LOGI("end to wait sync");
    }
    return syncProcess.errCode;
}

class DistributedDBCloudDBProxyTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;

protected:
    std::shared_ptr<VirtualCloudDb> virtualCloudDb_ = nullptr;
};

void DistributedDBCloudDBProxyTest::SetUpTestCase()
{
}

void DistributedDBCloudDBProxyTest::TearDownTestCase()
{
}

void DistributedDBCloudDBProxyTest::SetUp()
{
    DistributedDBUnitTest::DistributedDBToolsUnitTest::PrintTestCaseInfo();
    virtualCloudDb_ = std::make_shared<VirtualCloudDb>();
}

void DistributedDBCloudDBProxyTest::TearDown()
{
    virtualCloudDb_ = nullptr;
}

/**
 * @tc.name: CloudDBProxyTest001
 * @tc.desc: Verify cloud db init and close function.
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: zhangqiquan
 */
HWTEST_F(DistributedDBCloudDBProxyTest, CloudDBProxyTest001, TestSize.Level0)
{
    /**
     * @tc.steps: step1. set cloud db to proxy
     * @tc.expected: step1. E_OK
     */
    CloudDBProxy proxy;
    EXPECT_EQ(proxy.SetCloudDB(virtualCloudDb_), E_OK);
    /**
     * @tc.steps: step2. proxy close cloud db with cloud error
     * @tc.expected: step2. -E_CLOUD_ERROR
     */
    virtualCloudDb_->SetCloudError(true);
    EXPECT_EQ(proxy.Close(), -E_CLOUD_ERROR);
    /**
     * @tc.steps: step3. proxy close cloud db again
     * @tc.expected: step3. E_OK because cloud db has been set nullptr
     */
    EXPECT_EQ(proxy.Close(), E_OK);
    virtualCloudDb_->SetCloudError(false);
    EXPECT_EQ(proxy.Close(), E_OK);
}

/**
 * @tc.name: CloudDBProxyTest002
 * @tc.desc: Verify cloud db insert function.
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: zhangqiquan
 */
HWTEST_F(DistributedDBCloudDBProxyTest, CloudDBProxyTest002, TestSize.Level0)
{
    /**
     * @tc.steps: step1. set cloud db to proxy
     * @tc.expected: step1. E_OK
     */
    CloudDBProxy proxy;
    ASSERT_EQ(proxy.SetCloudDB(virtualCloudDb_), E_OK);
    /**
     * @tc.steps: step2. insert data to cloud db
     * @tc.expected: step2. OK
     */
    TableSchema schema = {
        .name = TABLE_NAME,
        .fields = GetFields()
    };
    std::vector<VBucket> expectRecords = CloudDBDataUtils::GenerateRecords(10, schema); // generate 10 records
    std::vector<VBucket> expectExtends = CloudDBDataUtils::GenerateExtends(10); // generate 10 extends
    Info uploadInfo;
    std::vector<VBucket> insert = expectRecords;
    EXPECT_EQ(proxy.BatchInsert(TABLE_NAME, insert, expectExtends, uploadInfo), OK);

    VBucket extend;
    extend[CloudDbConstant::CURSOR_FIELD] = std::string("");
    std::vector<VBucket> actualRecords;
    EXPECT_EQ(proxy.Query(TABLE_NAME, extend, actualRecords), OK);
    /**
     * @tc.steps: step3. proxy query data
     * @tc.expected: step3. data is equal to expect
     */
    ASSERT_EQ(actualRecords.size(), expectRecords.size());
    for (size_t i = 0; i < actualRecords.size(); ++i) {
        for (const auto &field: schema.fields) {
            Type expect = expectRecords[i][field.colName];
            Type actual = actualRecords[i][field.colName];
            EXPECT_EQ(expect.index(), actual.index());
        }
    }
    /**
     * @tc.steps: step4. proxy close cloud db
     * @tc.expected: step4. E_OK
     */
    EXPECT_EQ(proxy.Close(), E_OK);
}

/**
 * @tc.name: CloudDBProxyTest003
 * @tc.desc: Verify cloud db update function.
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: zhangqiquan
 */
HWTEST_F(DistributedDBCloudDBProxyTest, CloudDBProxyTest003, TestSize.Level0)
{
    TableSchema schema = {
        .name = TABLE_NAME,
        .fields = GetFields()
    };
    /**
     * @tc.steps: step1. set cloud db to proxy
     * @tc.expected: step1. E_OK
     */
    CloudDBProxy proxy;
    ASSERT_EQ(proxy.SetCloudDB(virtualCloudDb_), E_OK);
    /**
     * @tc.steps: step2. insert data to cloud db
     * @tc.expected: step2. OK
     */
    std::vector<VBucket> expectRecords = CloudDBDataUtils::GenerateRecords(10, schema); // generate 10 records
    std::vector<VBucket> expectExtends = CloudDBDataUtils::GenerateExtends(10); // generate 10 extends
    Info uploadInfo;
    std::vector<VBucket> insert = expectRecords;
    EXPECT_EQ(proxy.BatchInsert(TABLE_NAME, insert, expectExtends, uploadInfo), OK);
    /**
     * @tc.steps: step3. update data to cloud db
     * @tc.expected: step3. E_OK
     */
    ModifyRecords(expectRecords);
    std::vector<VBucket> update = expectRecords;
    EXPECT_EQ(proxy.BatchUpdate(TABLE_NAME, update, expectExtends, uploadInfo), OK);
    /**
     * @tc.steps: step3. proxy close cloud db
     * @tc.expected: step3. E_OK
     */
    VBucket extend;
    extend[CloudDbConstant::CURSOR_FIELD] = std::string("");
    std::vector<VBucket> actualRecords;
    EXPECT_EQ(proxy.Query(TABLE_NAME, extend, actualRecords), OK);
    ASSERT_EQ(actualRecords.size(), expectRecords.size());
    for (size_t i = 0; i < actualRecords.size(); ++i) {
        for (const auto &field: schema.fields) {
            Type expect = expectRecords[i][field.colName];
            Type actual = actualRecords[i][field.colName];
            EXPECT_EQ(expect.index(), actual.index());
        }
    }
    /**
     * @tc.steps: step4. proxy close cloud db
     * @tc.expected: step4. E_OK
     */
    EXPECT_EQ(proxy.Close(), E_OK);
}

/**
 * @tc.name: CloudDBProxyTest004
 * @tc.desc: Verify cloud db heartbeat and lock function.
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: zhangqiquan
 */
HWTEST_F(DistributedDBCloudDBProxyTest, CloudDBProxyTest004, TestSize.Level3)
{
    /**
     * @tc.steps: step1. set cloud db to proxy and sleep 5s when download
     * @tc.expected: step1. E_OK
     */
    auto iCloud = std::make_shared<MockICloudSyncStorageInterface>();
    auto cloudSyncer = new(std::nothrow) VirtualCloudSyncer(StorageProxy::GetCloudDb(iCloud.get()));
    EXPECT_CALL(*iCloud, StartTransaction).WillRepeatedly(testing::Return(E_OK));
    EXPECT_CALL(*iCloud, Commit).WillRepeatedly(testing::Return(E_OK));
    ASSERT_NE(cloudSyncer, nullptr);
    ASSERT_EQ(cloudSyncer->SetCloudDB(virtualCloudDb_), E_OK);
    cloudSyncer->SetSyncAction(true, false);
    cloudSyncer->SetDownloadFunc([]() {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // sleep 5s
        return E_OK;
    });
    /**
     * @tc.steps: step2. call sync and wait sync finish
     * @tc.expected: step2. E_OK
     */
    std::mutex processMutex;
    std::condition_variable cv;
    SyncProcess syncProcess;
    LOGI("[CloudDBProxyTest004] Call cloud sync");
    const auto callback = [&syncProcess, &processMutex, &cv](const std::map<std::string, SyncProcess> &process) {
        {
            std::lock_guard<std::mutex> autoLock(processMutex);
            if (process.size() >= 1u) {
                syncProcess = std::move(process.begin()->second);
            } else {
                SyncProcess tmpProcess;
                syncProcess = tmpProcess;
            }
        }
        cv.notify_all();
    };
    EXPECT_EQ(cloudSyncer->Sync({ "cloud" }, SyncMode::SYNC_MODE_CLOUD_MERGE, { TABLE_NAME }, callback, 0), E_OK);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(virtualCloudDb_->GetLockStatus());
    {
        LOGI("[CloudDBProxyTest004] begin to wait sync");
        std::unique_lock<std::mutex> uniqueLock(processMutex);
        cv.wait(uniqueLock, [&syncProcess]() {
            return syncProcess.process == ProcessStatus::FINISHED;
        });
        LOGI("[CloudDBProxyTest004] end to wait sync");
        EXPECT_EQ(syncProcess.errCode, OK);
    }
    /**
     * @tc.steps: step3. get cloud lock status and heartbeat count
     * @tc.expected: step3. cloud is unlock and more than twice heartbeat
     */
    EXPECT_FALSE(virtualCloudDb_->GetLockStatus());
    EXPECT_GE(virtualCloudDb_->GetHeartbeatCount(), 2);
    virtualCloudDb_->ClearHeartbeatCount();
    cloudSyncer->Close();
    RefObject::KillAndDecObjRef(cloudSyncer);
}

/**
 * @tc.name: CloudDBProxyTest005
 * @tc.desc: Verify sync failed after cloud error.
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: zhangqiquan
 */
HWTEST_F(DistributedDBCloudDBProxyTest, CloudDBProxyTest005, TestSize.Level0)
{
    /**
     * @tc.steps: step1. set cloud db to proxy and sleep 5s when download
     * @tc.expected: step1. E_OK
     */
    auto iCloud = std::make_shared<MockICloudSyncStorageInterface>();
    auto cloudSyncer = new(std::nothrow) VirtualCloudSyncer(StorageProxy::GetCloudDb(iCloud.get()));
    EXPECT_CALL(*iCloud, StartTransaction).WillRepeatedly(testing::Return(E_OK));
    EXPECT_CALL(*iCloud, Commit).WillRepeatedly(testing::Return(E_OK));
    ASSERT_NE(cloudSyncer, nullptr);
    ASSERT_EQ(cloudSyncer->SetCloudDB(virtualCloudDb_), E_OK);
    cloudSyncer->SetSyncAction(false, false);
    virtualCloudDb_->SetCloudError(true);
    /**
     * @tc.steps: step2. call sync and wait sync finish
     * @tc.expected: step2. CLOUD_ERROR by lock error
     */
    EXPECT_EQ(Sync(cloudSyncer), CLOUD_ERROR);
    /**
     * @tc.steps: step3. get cloud lock status and heartbeat count
     * @tc.expected: step3. cloud is unlock and no heartbeat
     */
    EXPECT_FALSE(virtualCloudDb_->GetLockStatus());
    EXPECT_GE(virtualCloudDb_->GetHeartbeatCount(), 0);
    virtualCloudDb_->ClearHeartbeatCount();
    cloudSyncer->Close();
    RefObject::KillAndDecObjRef(cloudSyncer);
}

/**
 * @tc.name: CloudDBProxyTest006
 * @tc.desc: Verify sync failed by heartbeat failed.
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: zhangqiquan
 */
HWTEST_F(DistributedDBCloudDBProxyTest, CloudDBProxyTest006, TestSize.Level3)
{
    /**
     * @tc.steps: step1. set cloud db to proxy and sleep 5s when download
     * @tc.expected: step1. E_OK
     */
    auto iCloud = std::make_shared<MockICloudSyncStorageInterface>();
    auto cloudSyncer = new(std::nothrow) VirtualCloudSyncer(StorageProxy::GetCloudDb(iCloud.get()));
    EXPECT_CALL(*iCloud, StartTransaction).WillRepeatedly(testing::Return(E_OK));
    EXPECT_CALL(*iCloud, Commit).WillRepeatedly(testing::Return(E_OK));
    EXPECT_CALL(*iCloud, Rollback).WillRepeatedly(testing::Return(E_OK));
    ASSERT_NE(cloudSyncer, nullptr);
    ASSERT_EQ(cloudSyncer->SetCloudDB(virtualCloudDb_), E_OK);
    cloudSyncer->SetSyncAction(true, false);
    cloudSyncer->SetDownloadFunc([]() {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // sleep 5s
        return E_OK;
    });
    virtualCloudDb_->SetHeartbeatError(true);
    /**
     * @tc.steps: step2. call sync and wait sync finish
     * @tc.expected: step2. sync failed by heartbeat error
     */
    EXPECT_EQ(Sync(cloudSyncer), CLOUD_ERROR);
    /**
     * @tc.steps: step3. get cloud lock status and heartbeat count
     * @tc.expected: step3. cloud is unlock and twice heartbeat
     */
    EXPECT_FALSE(virtualCloudDb_->GetLockStatus());
    EXPECT_EQ(virtualCloudDb_->GetHeartbeatCount(), 2);
    virtualCloudDb_->ClearHeartbeatCount();
    cloudSyncer->Close();
    RefObject::KillAndDecObjRef(cloudSyncer);
}
}