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

#ifndef VIRTUAL_CLOUD_DATA_TRANSLATE_H
#define VIRTUAL_CLOUD_DATA_TRANSLATE_H
#include "icloud_data_translate.h"
#include "cloud_store_types.h"

namespace DistributedDB {
class VirtualCloudDataTranslate : public ICloudDataTranslate {
public:
    VirtualCloudDataTranslate() = default;
    ~VirtualCloudDataTranslate() override = default;
    std::vector<uint8_t> AssetToBlob(const Asset &asset) override;
    std::vector<uint8_t> AssetsToBlob(const Assets &assets) override;
    Asset BlobToAsset(const std::vector<uint8_t> &blob) override;
    Assets BlobToAssets(std::vector<uint8_t> &blob) override;
};
}
#endif // VIRTUAL_CLOUD_DATA_TRANSLATE_H
