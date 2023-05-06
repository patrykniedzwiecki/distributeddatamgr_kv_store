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

#include "cloud_syncer_test.h"
#include "distributeddb_tools_unit_test.h"
#include "mock_iclouddb.h"
#include "mock_icloud_sync_storage_interface.h"
#include "time_helper.h"
#include "types_export.h"

using namespace testing::ext;
using namespace testing;
using namespace DistributedDB;
using namespace DistributedDBUnitTest;

namespace {
static int64_t photoCount = 10;
static double dataHeight = 166.0;
class CloudSyncerDownloadTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

MockICloudSyncStorageInterface *g_iCloud = nullptr;
std::shared_ptr<TestStorageProxy> g_storageProxy = nullptr;
MockICloudDB *g_idb = nullptr;
std::unique_ptr<TestCloudSyncer> g_cloudSyncer = nullptr;

void CloudSyncerDownloadTest::SetUpTestCase(void)
{
    g_iCloud = new MockICloudSyncStorageInterface();
    g_storageProxy = std::make_shared<TestStorageProxy>(g_iCloud);
    g_cloudSyncer = std::make_unique<TestCloudSyncer>(g_storageProxy);
    g_idb = new MockICloudDB();
    g_cloudSyncer->SetMockICloudDB(g_idb);
}

void CloudSyncerDownloadTest::TearDownTestCase(void)
{
    g_cloudSyncer = nullptr;
    g_storageProxy = nullptr;
    delete g_iCloud;
}

void CloudSyncerDownloadTest::SetUp(void)
{
    DistributedDBToolsUnitTest::PrintTestCaseInfo();
}

void CloudSyncerDownloadTest::TearDown(void)
{
}


std::vector<VBucket> GetRetCloudData(uint64_t cnt)
{
    std::vector<uint8_t> photo(photoCount, 'v');
    std::vector<VBucket> cloudData;
    static uint64_t totalCnt = 0;
    for (uint64_t i = totalCnt; i < totalCnt + cnt; ++i) {
        VBucket data;
        data.insert_or_assign("name", "Cloud" + std::to_string(i));
        data.insert_or_assign("height", dataHeight);
        data.insert_or_assign("married", (bool)0);
        data.insert_or_assign("photo", photo);
        data.insert_or_assign("age", 13L);
        data.insert_or_assign(CloudDbConstant::GID_FIELD, std::to_string(i));
        data.insert_or_assign(CloudDbConstant::CREATE_FIELD, (int64_t)i);
        data.insert_or_assign(CloudDbConstant::MODIFY_FIELD, (int64_t)i);
        data.insert_or_assign(CloudDbConstant::DELETE_FIELD, false);
        data.insert_or_assign(CloudDbConstant::CURSOR_FIELD, std::to_string(i));
        cloudData.push_back(data);
    }
    totalCnt += cnt;
    return cloudData;
}

struct InvalidCloudDataOpt {
    bool invalidGID = true;
    bool invalidCreateField = true;
    bool invalidModifyField = true;
    bool invalidDeleteField = true;
    bool invalidCursor = true;
};

std::vector<VBucket> GetInvalidTypeCloudData(uint64_t cnt, InvalidCloudDataOpt fieldOpt)
{
    std::vector<uint8_t> photo(photoCount, 'v');
    std::vector<VBucket> cloudData;
    static uint64_t totalCnt = 0;
    for (uint64_t i = totalCnt; i < totalCnt + cnt; ++i) {
        VBucket data;
        data.insert_or_assign("name", "Cloud" + std::to_string(i));
        data.insert_or_assign("height", dataHeight);
        data.insert_or_assign("married", (bool)0);
        data.insert_or_assign("photo", photo);
        data.insert_or_assign("age", 13L);
        
        if (fieldOpt.invalidGID) {
            data.insert_or_assign(CloudDbConstant::GID_FIELD, (int64_t)i);
        }
        if (fieldOpt.invalidCreateField) {
            data.insert_or_assign(CloudDbConstant::CREATE_FIELD, (Bytes)i);
        }
        if (fieldOpt.invalidModifyField) {
            data.insert_or_assign(CloudDbConstant::MODIFY_FIELD, std::to_string(i));
        }
        if (fieldOpt.invalidDeleteField) {
            data.insert_or_assign(CloudDbConstant::DELETE_FIELD, (int64_t)false);
        }
        if (fieldOpt.invalidCursor) {
            data.insert_or_assign(CloudDbConstant::CURSOR_FIELD, (int64_t)i);
        }
        cloudData.push_back(data);
    }
    totalCnt += cnt;
    return cloudData;
}

std::vector<VBucket> GetInvalidFieldCloudData(uint64_t cnt, InvalidCloudDataOpt fieldOpt)
{
    std::vector<uint8_t> photo(photoCount, 'v');
    std::vector<VBucket> cloudData;
    static uint64_t totalCnt = 0;
    for (uint64_t i = totalCnt; i < totalCnt + cnt; ++i) {
        VBucket data;
        data.insert_or_assign("name", "Cloud" + std::to_string(i));
        data.insert_or_assign("height", dataHeight);
        data.insert_or_assign("married", (bool)0);
        data.insert_or_assign("photo", photo);
        data.insert_or_assign("age", 13L);
        // Invalid means don't have here
        if (!fieldOpt.invalidGID) {
            data.insert_or_assign(CloudDbConstant::GID_FIELD, std::to_string(i));
        }
        if (!fieldOpt.invalidCreateField) {
            data.insert_or_assign(CloudDbConstant::CREATE_FIELD, (int64_t)i);
        }
        if (!fieldOpt.invalidModifyField) {
            data.insert_or_assign(CloudDbConstant::MODIFY_FIELD, (int64_t)i);
        }
        if (!fieldOpt.invalidDeleteField) {
            data.insert_or_assign(CloudDbConstant::DELETE_FIELD, false);
        }
        if (!fieldOpt.invalidCursor) {
            data.insert_or_assign(CloudDbConstant::CURSOR_FIELD, std::to_string(i));
        }
        cloudData.push_back(data);
    }
    totalCnt += cnt;
    return cloudData;
}

LogInfo GetLogInfo(uint64_t timestamp, bool isDeleted)
{
    LogInfo logInfo;
    logInfo.timestamp = timestamp;
    logInfo.cloudGid = std::to_string(timestamp);
    if (isDeleted) {
        logInfo.flag = 1u;
    }
    return logInfo;
}

/**
 * @tc.name: DownloadMockTest001
 * @tc.desc: Test situation with all possible output for GetCloudWaterMark
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockTest001, TestSize.Level1)
{
    TaskId taskId = 1u;
    std::vector<VBucket> retCloudData = GetRetCloudData(5);
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillRepeatedly(DoAll(SetArgReferee<2>(retCloudData), Return(QUERY_END)));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));

    //  1. Read meta data success
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetLogInfoByPrimaryKeyOrGid(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(0, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(1, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(2, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(3, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(4, false)), Return(E_OK)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, E_OK);

    // // 2. Failed to read water level
    taskId = 3u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_FORCE_PUSH);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(-E_INVALID_DB));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_INVALID_DB);

    taskId = 4u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_FORCE_PUSH);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(-E_SECUREC_ERROR));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_SECUREC_ERROR);
    
    taskId = 5u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_FORCE_PUSH);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(-E_INVALID_ARGS));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_INVALID_ARGS);
}

/**
 * @tc.name: DownloadMockTest001
 * @tc.desc: Test situation with all possible output for GetCloudWaterMark
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockTest002, TestSize.Level1)
{
    TaskId taskId = 6u;
    std::vector<VBucket> retCloudData = GetRetCloudData(5);
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillRepeatedly(DoAll(SetArgReferee<2>(retCloudData), Return(QUERY_END)));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));

    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_FORCE_PUSH);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(-E_BUSY));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_BUSY);

    taskId = 7u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_FORCE_PUSH);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(-E_NOT_FOUND));
    EXPECT_CALL(*g_iCloud, GetLogInfoByPrimaryKeyOrGid(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(0, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(1, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(2, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(3, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(4, false)), Return(E_OK)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    // when we coudln't find key in get meta data, read local water mark will return default value and E_OK
    EXPECT_EQ(errCode, E_OK);

    // Other sqlite error, like SQLITE_ERROR
    taskId = 8u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_FORCE_PUSH);
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillOnce(Return(SQLITE_ERROR));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, SQLITE_ERROR);
}

/**
 * @tc.name: DownloadMockQueryTest002
 * @tc.desc: Test situation with all possible output for Query
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockQueryTest002, TestSize.Level1)
{
    TaskId taskId = 1u;
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));

    //  1. Query data success for the first time, but will not reach end
    //  2. While quring second time, no more data comes back and return QUERY END
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(SetArgReferee<2>(GetRetCloudData(5)), Return(QUERY_END)));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetLogInfoByPrimaryKeyOrGid(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(0, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(1, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(2, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(3, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(4, false)), Return(E_OK)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, E_OK);
}

/**
 * @tc.name: DownloadMockQueryTest003
 * @tc.desc: Query data success but return invalid data (type mismatch)
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockQueryTest003, TestSize.Level1)
{
    TaskId taskId = 1u;
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));
    
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetInvalidTypeCloudData(5, {.invalidCursor = false})), Return(QUERY_END)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 2u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetInvalidTypeCloudData(5, {.invalidCursor = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);


    taskId = 3u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(
        SetArgReferee<2>(GetInvalidTypeCloudData(5, {.invalidDeleteField = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 4u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(
        SetArgReferee<2>(GetInvalidTypeCloudData(5, {.invalidGID = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 5u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(
        SetArgReferee<2>(GetInvalidTypeCloudData(5, {.invalidModifyField = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);
}

/**
 * @tc.name: DownloadMockQueryTest004
 * @tc.desc: Query data success but return invalid data (field mismatch)
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockQueryTest004, TestSize.Level1)
{
    TaskId taskId = 1u;
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));

    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(
        SetArgReferee<2>(GetInvalidFieldCloudData(5, {.invalidCreateField = false})), Return(QUERY_END)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 2u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetInvalidFieldCloudData(5, {.invalidCursor = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 3u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(
        SetArgReferee<2>(GetInvalidFieldCloudData(5, {.invalidDeleteField = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 4u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetInvalidFieldCloudData(5, {.invalidGID = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);

    taskId = 5u;
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);
    EXPECT_CALL(*g_idb, Query(_, _, _)).WillOnce(DoAll(
        SetArgReferee<2>(GetInvalidFieldCloudData(5, {.invalidModifyField = false})), Return(QUERY_END)));
    errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);
}

/**
 * @tc.name: DownloadMockQueryTest005
 * @tc.desc: First time, query return OK but empty data set
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockQueryTest005, TestSize.Level1)
{
    TaskId taskId = 1u;
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);

    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillRepeatedly(DoAll(SetArgReferee<2>(GetRetCloudData(0)), Return(OK)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, -E_CLOUD_ERROR);
}

/**
 * @tc.name: DownloadMockTest006
 * @tc.desc: Data from cloud do not exist in local database.
 * therefore, GetLogInfoByPrimaryKeyOrGid will indicate that the datum is -E_NOT_FOUND
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockTest006, TestSize.Level1)
{
    TaskId taskId = 1u;
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);

    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetRetCloudData(5)), Return(QUERY_END)));
    EXPECT_CALL(*g_iCloud, GetLogInfoByPrimaryKeyOrGid(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(0, false)), Return(-E_NOT_FOUND)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(1, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(2, false)), Return(-E_NOT_FOUND)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(3, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(4, false)), Return(E_OK)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, E_OK);
}

/**
 * @tc.name: DownloadMockTest007
 * @tc.desc: Query return OK multiple times and return E_OK finally
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author: WanYi
 */
HWTEST_F(CloudSyncerDownloadTest, DownloadMockTest007, TestSize.Level1)
{
    TaskId taskId = 1u;
    EXPECT_CALL(*g_iCloud, StartTransaction(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetUploadCount(_, _, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Commit()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, Rollback()).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutCloudSyncData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, PutMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, GetMetaData(_, _)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, ChkSchema(_)).WillRepeatedly(Return(E_OK));
    EXPECT_CALL(*g_iCloud, TriggerObserverAction(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(*g_iCloud, GetCloudTableSchema(_, _)).WillRepeatedly(Return(E_OK));
    g_cloudSyncer->InitCloudSyncer(taskId, SYNC_MODE_CLOUD_MERGE);

    EXPECT_CALL(*g_idb, Query(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetRetCloudData(5)), Return(OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetRetCloudData(5)), Return(OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetRetCloudData(5)), Return(QUERY_END)));
    EXPECT_CALL(*g_iCloud, GetLogInfoByPrimaryKeyOrGid(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(0, false)), Return(-E_NOT_FOUND)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(1, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(2, false)), Return(-E_NOT_FOUND)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(3, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(4, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(5, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(6, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(7, true)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(8, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(9, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(photoCount, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(11, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(12, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(13, false)), Return(E_OK)))
        .WillOnce(DoAll(SetArgReferee<2>(GetLogInfo(14, false)), Return(E_OK)));
    int errCode = g_cloudSyncer->CallDoDownload(taskId);
    EXPECT_EQ(errCode, E_OK);
}

}