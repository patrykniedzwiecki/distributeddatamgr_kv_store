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
#ifndef VIRTUAL_CLOUD_SYNCER_H
#define VIRTUAL_CLOUD_SYNCER_H
#include "cloud_syncer.h"

namespace DistributedDB {
class VirtualCloudSyncer final : public CloudSyncer {
public:
    explicit VirtualCloudSyncer(std::shared_ptr<StorageProxy> storageProxy);
    ~VirtualCloudSyncer() override = default;

    int DoDownload(TaskId taskId) override;

    int DoUpload(TaskId taskId, bool lastTable) override;

    void SetSyncAction(bool doDownload, bool doUpload);

    void SetDownloadFunc(const std::function<int (void)> &);

    void Notify(bool notifyIfError = false);

    size_t GetQueueCount();
private:
    std::function<int (void)> downloadFunc_;
    std::atomic<bool> doDownload_;
    std::atomic<bool> doUpload_;
};
}
#endif // VIRTUAL_CLOUD_SYNCER_H
