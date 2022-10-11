/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#define LOG_TAG "JS_SingleKVStoreV9"
#include "js_single_kv_store.h"
#include "js_util.h"
#include "js_kv_store_resultset.h"
#include "datashare_predicates.h"
#include "js_query.h"
#include "log_print.h"
#include "napi_queue.h"
#include "uv_queue.h"
#include "kv_utils.h"

using namespace OHOS::DistributedKv;
using namespace OHOS::DataShare;
namespace OHOS::DistributedData {
JsSingleKVStoreV9::JsSingleKVStoreV9(const std::string& storeId)
    : JsKVStoreV9(storeId)
{
}

napi_value JsSingleKVStoreV9::Constructor(napi_env env)
{
    const napi_property_descriptor properties[] = {
        DECLARE_NAPI_FUNCTION("put", JsKVStoreV9::Put),
        DECLARE_NAPI_FUNCTION("delete", JsKVStoreV9::Delete),
        DECLARE_NAPI_FUNCTION("putBatch", JsKVStoreV9::PutBatch),
        DECLARE_NAPI_FUNCTION("deleteBatch", JsKVStoreV9::DeleteBatch),
        DECLARE_NAPI_FUNCTION("startTransaction", JsKVStoreV9::StartTransaction),
        DECLARE_NAPI_FUNCTION("commit", JsKVStoreV9::Commit),
        DECLARE_NAPI_FUNCTION("rollback", JsKVStoreV9::Rollback),
        DECLARE_NAPI_FUNCTION("enableSync", JsKVStoreV9::EnableSync),
        DECLARE_NAPI_FUNCTION("setSyncRange", JsKVStoreV9::SetSyncRange),
        DECLARE_NAPI_FUNCTION("backup", JsKVStoreV9::Backup),
        DECLARE_NAPI_FUNCTION("restore", JsKVStoreV9::Restore),
        DECLARE_NAPI_FUNCTION("deleteBackup", JsKVStoreV9::DeleteBackup),
        /* JsSingleKVStoreV9 externs JsKVStoreV9 */
        DECLARE_NAPI_FUNCTION("get", JsSingleKVStoreV9::Get),
        DECLARE_NAPI_FUNCTION("getEntries", JsSingleKVStoreV9::GetEntries),
        DECLARE_NAPI_FUNCTION("getResultSet", JsSingleKVStoreV9::GetResultSet),
        DECLARE_NAPI_FUNCTION("closeResultSet", JsSingleKVStoreV9::CloseResultSet),
        DECLARE_NAPI_FUNCTION("getResultSize", JsSingleKVStoreV9::GetResultSize),
        DECLARE_NAPI_FUNCTION("removeDeviceData", JsSingleKVStoreV9::RemoveDeviceData),
        DECLARE_NAPI_FUNCTION("sync", JsSingleKVStoreV9::Sync),
        DECLARE_NAPI_FUNCTION("setSyncParam", JsSingleKVStoreV9::SetSyncParam),
        DECLARE_NAPI_FUNCTION("getSecurityLevel", JsSingleKVStoreV9::GetSecurityLevel),
        DECLARE_NAPI_FUNCTION("on", JsKVStoreV9::OnEvent), /* same to JsDeviceKVStore */
        DECLARE_NAPI_FUNCTION("off", JsKVStoreV9::OffEvent) /* same to JsDeviceKVStore */
    };
    size_t count = sizeof(properties) / sizeof(properties[0]);
    return JSUtil::DefineClass(env, "SingleKVStoreV9", properties, count, JsSingleKVStoreV9::New);
}

/*
 * [JS API Prototype]
 * [AsyncCallback]
 *      get(key:string, callback:AsyncCallback<boolean|string|number|Uint8Array>):void;
 * [Promise]
 *      get(key:string):Promise<boolean|string|number|Uint8Array>;
 */
napi_value JsSingleKVStoreV9::Get(napi_env env, napi_callback_info info)
{
    ZLOGD("V9SingleKVStore::Get()");
    struct GetContext : public ContextBase {
        std::string key;
        JSUtil::KvStoreVariant value;
    };
    auto ctxt = std::make_shared<GetContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <key>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 1, PARAM_ERROR, "The number of parameters is incorrect.");
        ctxt->status = JSUtil::GetValue(env, argv[0], ctxt->key);
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->status == napi_ok, PARAM_ERROR, "The type of key must be string.");
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("GetV9 exit", ctxt->isThrowError);

    ZLOGD("key=%{public}.8s", ctxt->key.c_str());
    auto execute = [env, ctxt]() {
        OHOS::DistributedKv::Key key(ctxt->key);
        OHOS::DistributedKv::Value value;
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        bool isSchemaStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->IsSchemaStore();
        Status status = kvStore->Get(key, value);
        ZLOGD("kvStore->Get return %{public}d", status);
        ctxt->value = isSchemaStore ? value.ToString() : JSUtil::Blob2VariantValue(value);
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    auto output = [env, ctxt](napi_value& result) {
        ctxt->status = JSUtil::SetValue(env, ctxt->value, result);
        CHECK_STATUS_RETURN_VOID(ctxt, "output failed");
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute, output);
}

enum class ArgsType : uint8_t {
    /* input arguments' combination type */
    KEYPREFIX = 0,
    QUERY,
    UNKNOWN = 255
};
struct VariantArgs {
    /* input arguments' combinations */
    std::string keyPrefix;
    JsQuery* query;
    ArgsType type = ArgsType::UNKNOWN;
    DataQuery dataQuery;
    std::string errMsg;
};

static napi_status GetVariantArgs(napi_env env, size_t argc, napi_value* argv, VariantArgs& va)
{
    // required 1 arguments :: <keyPrefix/query>
    if (argc != 1) {
        va.errMsg = "The number of parameters is incorrect.";
        return napi_invalid_arg;
    }
    napi_valuetype type = napi_undefined;
    napi_status status = napi_typeof(env, argv[0], &type);
    if (!(type == napi_string || type == napi_object)) {
        va.errMsg = "The type of parameters keyPrefix/query is incorrect.";
        return napi_invalid_arg;
    }
    if (type == napi_string) {
        status = JSUtil::GetValue(env, argv[0], va.keyPrefix);
        if (va.keyPrefix.empty()) {
            va.errMsg = "The type of parameters keyPrefix is incorrect.";
            return napi_invalid_arg;
        }
        va.type = ArgsType::KEYPREFIX;
    } else if (type == napi_object) {
        bool result = false;
        status = napi_instanceof(env, argv[0], JsQuery::Constructor(env), &result);
        if ((status == napi_ok) && (result != false)) {
            status = JSUtil::Unwrap(env, argv[0], reinterpret_cast<void**>(&va.query), JsQuery::Constructor(env));
            if (va.query == nullptr) {
                va.errMsg = "The parameters query is incorrect.";
                return napi_invalid_arg;
            }
            va.type = ArgsType::QUERY;
        } else {
            status = JSUtil::GetValue(env, argv[0], va.dataQuery);
            ZLOGD("kvStoreDataShare->GetResultSet return %{public}d", status);
        }
    }
    return status;
};

/*
 * [JS API Prototype]
 *  getEntries(keyPrefix:string, callback:AsyncCallback<Entry[]>):void
 *  getEntries(keyPrefix:string):Promise<Entry[]>
 *
 *  getEntries(query:Query, callback:AsyncCallback<Entry[]>):void
 *  getEntries(query:Query) : Promise<Entry[]>
 */
napi_value JsSingleKVStoreV9::GetEntries(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::GetEntries()");
    struct GetEntriesContext : public ContextBase {
        VariantArgs va;
        std::vector<Entry> entries;
    };
    auto ctxt = std::make_shared<GetEntriesContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <keyPrefix/query>
        ctxt->status = GetVariantArgs(env, argc, argv, ctxt->va);
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->status != napi_invalid_arg, PARAM_ERROR, ctxt->va.errMsg);
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("GetEntriesV9 exit", ctxt->isThrowError);

    auto execute = [ctxt]() {
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        Status status = Status::INVALID_ARGUMENT;
        if (ctxt->va.type == ArgsType::KEYPREFIX) {
            OHOS::DistributedKv::Key keyPrefix(ctxt->va.keyPrefix);
            status = kvStore->GetEntries(keyPrefix, ctxt->entries);
            ZLOGD("kvStore->GetEntries() return %{public}d", status);
        } else if (ctxt->va.type == ArgsType::QUERY) {
            auto query = ctxt->va.query->GetNative();
            status = kvStore->GetEntries(query, ctxt->entries);
            ZLOGD("kvStore->GetEntries() return %{public}d", status);
        }
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    auto output = [env, ctxt](napi_value& result) {
        auto isSchemaStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->IsSchemaStore();
        ctxt->status = JSUtil::SetValue(env, ctxt->entries, result, isSchemaStore);
        CHECK_STATUS_RETURN_VOID(ctxt, "output failed!");
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute, output);
}

/*
 * [JS API Prototype]
 *  getResultSet(keyPrefix:string, callback:AsyncCallback<KvStoreResultSet>):void
 *  getResultSet(keyPrefix:string):Promise<KvStoreResultSet>
 *
 *  getResultSet(query:Query, callback:AsyncCallback<KvStoreResultSet>):void
 *  getResultSet(query:Query):Promise<KvStoreResultSet>
 */
napi_value JsSingleKVStoreV9::GetResultSet(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::GetResultSet()");
    struct GetResultSetContext : public ContextBase {
        VariantArgs va;
        JsKVStoreResultSet* resultSet = nullptr;
        napi_ref ref = nullptr;
    };
    auto ctxt = std::make_shared<GetResultSetContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <keyPrefix/query>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 1, PARAM_ERROR, "The number of parameters is incorrect.");
        ctxt->status = GetVariantArgs(env, argc, argv, ctxt->va);
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->status != napi_invalid_arg, PARAM_ERROR, ctxt->va.errMsg);
        ctxt->ref = JSUtil::NewWithRef(env, 0, nullptr, reinterpret_cast<void**>(&ctxt->resultSet),
            JsKVStoreResultSet::Constructor(env));
        CHECK_THROW_BUSINESS_ERR(ctxt, (ctxt->resultSet != nullptr) && (ctxt->ref != nullptr), PARAM_ERROR, "KVStoreResultSet::New failed!");
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("GetResultSetV9 exit", ctxt->isThrowError);
    
    auto execute = [ctxt]() {
        std::shared_ptr<KvStoreResultSet> kvResultSet;
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        Status status = Status::INVALID_ARGUMENT;
        if (ctxt->va.type == ArgsType::KEYPREFIX) {
            OHOS::DistributedKv::Key keyPrefix(ctxt->va.keyPrefix);
            status = kvStore->GetResultSet(keyPrefix, kvResultSet);
            ZLOGD("kvStore->GetEntries() return %{public}d", status);
        } else if (ctxt->va.type == ArgsType::QUERY) {
            auto query = ctxt->va.query->GetNative();
            status = kvStore->GetResultSet(query, kvResultSet);
            ZLOGD("kvStore->GetEntries() return %{public}d", status);
        } else {
            status = kvStore->GetResultSet(ctxt->va.dataQuery, kvResultSet);
            ZLOGD("ArgsType::PREDICATES GetResultSetWithQuery return %{public}d", status);
        };
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
        ctxt->resultSet->SetNative(kvResultSet);
    };
    auto output = [env, ctxt](napi_value& result) {
        ctxt->status = napi_get_reference_value(env, ctxt->ref, &result);
        napi_delete_reference(env, ctxt->ref);
        CHECK_STATUS_RETURN_VOID(ctxt, "output kvResultSet failed");
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute, output);
}

/*
 * [JS API Prototype]
 *  closeResultSet(resultSet:KVStoreResultSet, callback: AsyncCallback<void>):void
 *  closeResultSet(resultSet:KVStoreResultSet):Promise<void>
 */
napi_value JsSingleKVStoreV9::CloseResultSet(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::CloseResultSet()");
    struct CloseResultSetContext : public ContextBase {
        JsKVStoreResultSet* resultSet = nullptr;
    };
    auto ctxt = std::make_shared<CloseResultSetContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <resultSet>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 1, PARAM_ERROR, "The number of parameters is incorrect."); 
        napi_valuetype type = napi_undefined;
        ctxt->status = napi_typeof(env, argv[0], &type);
        CHECK_THROW_BUSINESS_ERR(ctxt, type == napi_object, PARAM_ERROR, "The type of parameters resultSet is incorrect."); 
        ctxt->status = JSUtil::Unwrap(env, argv[0], reinterpret_cast<void**>(&ctxt->resultSet),
            JsKVStoreResultSet::Constructor(env));
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->resultSet != nullptr, PARAM_ERROR, "The parameters resultSet is incorrect."); 
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("CloseResultSetV9 exit", ctxt->isThrowError);

    auto execute = [ctxt]() {
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        Status status = kvStore->CloseResultSet(ctxt->resultSet->GetNative());
        ZLOGD("kvStore->CloseResultSet return %{public}d", status);
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute);
}
/*
 * [JS API Prototype]
 *  getResultSize(query:Query, callback: AsyncCallback<number>):void
 *  getResultSize(query:Query):Promise<number>
 */
napi_value JsSingleKVStoreV9::GetResultSize(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::GetResultSize()");
    struct ResultSizeContext : public ContextBase {
        JsQuery* query = nullptr;
        int resultSize = 0;
    };
    auto ctxt = std::make_shared<ResultSizeContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <query>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 1, PARAM_ERROR, "The number of parameters is incorrect.");
        napi_valuetype type = napi_undefined;
        ctxt->status = napi_typeof(env, argv[0], &type);
        CHECK_THROW_BUSINESS_ERR(ctxt, type == napi_object, PARAM_ERROR, "The type of parameters query is incorrect."); 
        ctxt->status = JSUtil::Unwrap(env, argv[0], reinterpret_cast<void**>(&ctxt->query), JsQuery::Constructor(env));
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->query != nullptr, PARAM_ERROR, "The parameters query is incorrect."); 
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("CloseResultSetV9 exit", ctxt->isThrowError);

    auto execute = [ctxt]() {
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        auto query = ctxt->query->GetNative();
        Status status = kvStore->GetCount(query, ctxt->resultSize);
        ZLOGD("kvStore->GetCount() return %{public}d", status);
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    auto output = [env, ctxt](napi_value& result) {
        ctxt->status = JSUtil::SetValue(env, static_cast<int32_t>(ctxt->resultSize), result);
        CHECK_STATUS_RETURN_VOID(ctxt, "output resultSize failed!");
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute, output);
}

/*
 * [JS API Prototype]
 *  removeDeviceData(deviceId:string, callback: AsyncCallback<void>):void
 *  removeDeviceData(deviceId:string):Promise<void>
 */
napi_value JsSingleKVStoreV9::RemoveDeviceData(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::RemoveDeviceData()");
    struct RemoveDeviceContext : public ContextBase {
        std::string deviceId;
    };
    auto ctxt = std::make_shared<RemoveDeviceContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <deviceId>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 1, PARAM_ERROR, "The number of parameters is incorrect.");
        ctxt->status = JSUtil::GetValue(env, argv[0], ctxt->deviceId);
        CHECK_THROW_BUSINESS_ERR(ctxt, (!ctxt->deviceId.empty()) && (ctxt->status == napi_ok), PARAM_ERROR, "The parameters deviceId is incorrect.");
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("CloseResultSetV9 exit", ctxt->isThrowError);

    auto execute = [ctxt]() {
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        Status status = kvStore->RemoveDeviceData(ctxt->deviceId);
        ZLOGD("kvStore->RemoveDeviceData return %{public}d", status);
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute);
}

/*
 * [JS API Prototype]
 *  sync(deviceIdList:string[], mode:SyncMode, allowedDelayMs?:number):void
 */
napi_value JsSingleKVStoreV9::Sync(napi_env env, napi_callback_info info)
{
    struct SyncContext : public ContextBase {
        std::vector<std::string> deviceIdList;
        uint32_t mode = 0;
        uint32_t allowedDelayMs = 0;
        JsQuery* query = nullptr;
        napi_valuetype type = napi_undefined;
    };
    auto ctxt = std::make_shared<SyncContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 3 arguments :: <deviceIdList> <mode> [allowedDelayMs]
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 2, PARAM_ERROR, "The number of parameters is incorrect.");
        ctxt->status = JSUtil::GetValue(env, argv[0], ctxt->deviceIdList);
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->status == napi_ok, PARAM_ERROR, "The deviceIdList parameters is incorrect.");
        napi_typeof(env, argv[1], &ctxt->type);
        if (ctxt->type == napi_object) {
            ctxt->status = JSUtil::Unwrap(env,
                argv[1], reinterpret_cast<void**>(&ctxt->query), JsQuery::Constructor(env));
            CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->status == napi_ok, PARAM_ERROR, "The parameters mode is incorrect.");
            ctxt->status = JSUtil::GetValue(env, argv[2], ctxt->mode);
        }
        if (ctxt->type == napi_number) {
            ctxt->status = JSUtil::GetValue(env, argv[1], ctxt->mode);
            if (argc == 3) {
                ctxt->status = JSUtil::GetValue(env, argv[2], ctxt->allowedDelayMs);
            }
        }
        CHECK_THROW_BUSINESS_ERR(ctxt, (ctxt->mode <= uint32_t(SyncMode::PUSH_PULL)) && (ctxt->status == napi_ok), PARAM_ERROR,
            "The number of parameters is incorrect.");
    };
    ctxt->GetCbInfoSync(env, info, input);
    CHECK_IF_RETURN("SyncV9 exit", ctxt->isThrowError);

    ZLOGD("sync deviceIdList.size=%{public}d, mode:%{public}u, allowedDelayMs:%{public}u",
        (int)ctxt->deviceIdList.size(), ctxt->mode, ctxt->allowedDelayMs);

    auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
    Status status = Status::INVALID_ARGUMENT;
    if (ctxt->type == napi_object) {
        auto query = ctxt->query->GetNative();
        status = kvStore->Sync(ctxt->deviceIdList, static_cast<SyncMode>(ctxt->mode), query, nullptr);
    }
    if (ctxt->type == napi_number) {
        status = kvStore->Sync(ctxt->deviceIdList, static_cast<SyncMode>(ctxt->mode), ctxt->allowedDelayMs);
    }
    ZLOGD("kvStore->Sync return %{public}d!", status);
    ThrowNapiError(env, status, "", false);
    return nullptr;
}

/*
 * [JS API Prototype]
 *  setSyncParam(defaultAllowedDelayMs:number, callback: AsyncCallback<number>):void
 *  setSyncParam(defaultAllowedDelayMs:number):Promise<void>
 */
napi_value JsSingleKVStoreV9::SetSyncParam(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::SetSyncParam()");
    struct SyncParamContext : public ContextBase {
        uint32_t allowedDelayMs;
    };
    auto ctxt = std::make_shared<SyncParamContext>();
    auto input = [env, ctxt](size_t argc, napi_value* argv) {
        // required 1 arguments :: <allowedDelayMs>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 1, PARAM_ERROR, "The number of parameters is incorrect.");
        ctxt->status = JSUtil::GetValue(env, argv[0], ctxt->allowedDelayMs);
        CHECK_THROW_BUSINESS_ERR(ctxt, ctxt->status == napi_ok, PARAM_ERROR, "The parameters allowedDelayMs is incorrect.");
    };
    ctxt->GetCbInfo(env, info, input);
    CHECK_IF_RETURN("SetSyncParamV9 exit", ctxt->isThrowError);

    auto execute = [ctxt]() {
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        KvSyncParam syncParam { ctxt->allowedDelayMs };
        Status status = kvStore->SetSyncParam(syncParam);
        ZLOGD("kvStore->SetSyncParam return %{public}d", status);
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute);
}

/*
 * [JS API Prototype]
 *  getSecurityLevel(callback: AsyncCallback<SecurityLevel>):void
 *  getSecurityLevel():Promise<SecurityLevel>
 */
napi_value JsSingleKVStoreV9::GetSecurityLevel(napi_env env, napi_callback_info info)
{
    ZLOGD("SingleKVStore::GetSecurityLevel()");
    struct SecurityLevelContext : public ContextBase {
        SecurityLevel securityLevel;
    };
    auto ctxt = std::make_shared<SecurityLevelContext>();
    ctxt->GetCbInfo(env, info);

    auto execute = [ctxt]() {
        auto& kvStore = reinterpret_cast<JsSingleKVStoreV9*>(ctxt->native)->GetNative();
        Status status = kvStore->GetSecurityLevel(ctxt->securityLevel);
        ZLOGD("kvStore->GetSecurityLevel return %{public}d", status);
        ctxt->status = (GenerateNapiError(status, ctxt->jsCode, ctxt->error) == Status::SUCCESS) ?
            napi_ok : napi_generic_failure;
    };
    auto output = [env, ctxt](napi_value& result) {
        ctxt->status = JSUtil::SetValue(env, static_cast<uint8_t>(ctxt->securityLevel), result);
        CHECK_STATUS_RETURN_VOID(ctxt, "output failed!");
    };
    return NapiQueue::AsyncWork(env, ctxt, std::string(__FUNCTION__), execute, output);
}

napi_value JsSingleKVStoreV9::New(napi_env env, napi_callback_info info)
{
    ZLOGE("Constructor single kv store!");
    std::string storeId;
    auto ctxt = std::make_shared<ContextBase>();
    auto input = [env, ctxt, &storeId](size_t argc, napi_value* argv) {
        // required 2 arguments :: <storeId> <options>
        CHECK_THROW_BUSINESS_ERR(ctxt, argc >= 2, PARAM_ERROR, "The number of parameters is incorrect.");
        ctxt->status = JSUtil::GetValue(env, argv[0], storeId);
        CHECK_THROW_BUSINESS_ERR(ctxt, (ctxt->status == napi_ok) && !storeId.empty(), PARAM_ERROR, "The type of storeId must be string.");
    };
    ctxt->GetCbInfoSync(env, info, input);
    CHECK_IF_RETURN("SingleKVStoreV9 exit", ctxt->isThrowError);
    if (ctxt->status != napi_ok) {
        ZLOGE("no memory for kvStore");
        ThrowNapiError(env, PARAM_ERROR, "");
        return nullptr;
    }

    JsSingleKVStoreV9* kvStore = new (std::nothrow) JsSingleKVStoreV9(storeId);
    if (kvStore == nullptr) {
        ZLOGE("no memory for kvStore");
        ThrowNapiError(env, PARAM_ERROR, "");
        return nullptr;
    }

    auto finalize = [](napi_env env, void* data, void* hint) {
        ZLOGD("singleKVStore finalize.");
        auto* kvStore = reinterpret_cast<JsSingleKVStoreV9*>(data);
        CHECK_RETURN_VOID(kvStore != nullptr, "finalize null!");
        delete kvStore;
    };
    NAPI_CALL(env, napi_wrap(env, ctxt->self, kvStore, finalize, nullptr, nullptr));
    return ctxt->self;
}
} // namespace OHOS::DistributedData
