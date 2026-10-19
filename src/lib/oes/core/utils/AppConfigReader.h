// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================
#pragma once

#include <oes/core/utils/export.h>

#include <string>
#include <map>
#include <memory>

namespace oes::core {

using envvar_map_t = std::map<std::string, std::string>;

class OES_CORE_UTILS_EXPORT AppConfigReader {
public:
    AppConfigReader() = delete;
    explicit AppConfigReader(const std::string& default_config_file, const std::string& custom_file_path = "");
    ~AppConfigReader();
    
    std::string getAbsFilePath(const std::string& file_path);
    
    static std::string getEnv(const std::string& env_var_name);
    static std::string getDefaultConfigPath(const std::string& file_name);

    envvar_map_t getSimpleEnvMap();

private:
    class Impl_;
    std::unique_ptr<Impl_> impl_ptr_;

    std::unique_ptr<Impl_> getOesEnvImpl_(const std::string& default_config_file);
};

}