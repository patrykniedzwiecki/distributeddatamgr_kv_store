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

#ifndef SCHEMA_MGR_H
#define SCHEMA_MGR_H

#include <string>
#include <unordered_map>
#include <vector>

#include "cloud_store_types.h"
#include "icloud_sync_storage_interface.h"
#include "relational_schema_object.h"

namespace DistributedDB {
class SchemaMgr {
public:
    explicit SchemaMgr();
    ~SchemaMgr() =default;
    void SetCloudDbSchema(const DataBaseSchema &schema);
    std::shared_ptr<DataBaseSchema> GetCloudDbSchema();
    int GetCloudTableSchema(const TableName &tableName, TableSchema &retSchema);
    int ChkSchema(const TableName &tableName, RelationalSchemaObject &localSchema);

private:
    bool CompareType(const FieldInfo &localField, const Field &cloudField);
    bool CompareNullable(const FieldInfo &localField, const Field &cloudField);
    bool CompareIsPrimary(std::map<int, FieldName> &localPrimaryKeys, const Field &cloudField);
    int CompareFieldSchema(std::map<int, FieldName> &primaryKeys, FieldInfoMap &localFields,
        std::vector<Field> &cloudFields);
    std::shared_ptr<DataBaseSchema> cloudSchema_ = nullptr;
};
} // namespace DistributedDB

#endif // SCHEMA_MGR_H