#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <utility>
#include <cassert>
#include <filesystem>
#include <algorithm>
#include <cctype>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/AppConfigReader.h>
#include <oes/core/utils/Logger.h>

namespace oes::core {

class AppConfigReader::Impl_ {
private:
    nlohmann::json config_json_;
    void transformJsonKeysToLowerCase();
public:
    Impl_() = delete;
    explicit Impl_(const std::string& file_name);
    
    void loadJsonConfig(const std::string& file_path);
    nlohmann::json readJsonFile(const std::string& path);

    const nlohmann::json& getJson();

    void replaceByEnvvarValue(std::string& env_var);
    void replaceAllEnvvariables(nlohmann::json& json_obj);
    
    void merge(const nlohmann::json& json, const bool overwrite_non_empty);
    envvar_map_t getSimpleEnvMap();
};

AppConfigReader::Impl_::Impl_(const std::string& file_path) {
    loadJsonConfig(file_path);
}

nlohmann::json AppConfigReader::Impl_::readJsonFile(const std::string& path) {
    std::ifstream file(path, std::ios::in);
    if(file.rdstate()) {
        assert(!file.is_open());
    }

    nlohmann::json json;
    file >> json;
    file.close();

    return json;
}

void AppConfigReader::Impl_::loadJsonConfig(const std::string& config_file_path) {

    config_json_ = readJsonFile(config_file_path);
    replaceAllEnvvariables(config_json_);

    config_json_ = config_json_.flatten();
    transformJsonKeysToLowerCase();
}

const nlohmann::json& AppConfigReader::Impl_::getJson() {
    return config_json_;
}

void AppConfigReader::Impl_::replaceByEnvvarValue(std::string& env_var) {
    if (env_var.find("$ENV{") == 0 && env_var.rfind("}") == env_var.length() - 1) {
        auto str = env_var.substr(5, env_var.length() - 5 - 1);        
        env_var = AppConfigReader::getEnv(str);
    }
}

void AppConfigReader::Impl_::replaceAllEnvvariables(nlohmann::json& json_obj) {
    for (auto& [key, val] : json_obj.items()) {
        OES_UNUSED(key);
        if (val.is_structured()) {
            replaceAllEnvvariables(val);
        } else if (val.is_string()) {
            std::string tmp (val.get<std::string>());
            replaceByEnvvarValue(tmp);
            val = tmp;
        }
    }
}

void AppConfigReader::Impl_::transformJsonKeysToLowerCase() {
    std::string tmp;
    nlohmann::json new_json;

    for (auto& [key, val] : config_json_.items()) {
        tmp = key;
        std::transform(key.cbegin(), key.cend(), tmp.begin(), [](unsigned char c) { return std::tolower(c); } );

        new_json.emplace(std::make_pair(tmp, val));
    }

    config_json_ = new_json;
}

void AppConfigReader::Impl_::merge(const nlohmann::json& ext_json, const bool overwrite_non_empty) {

    for (const auto& [key, value] : ext_json.items()) {
        if (config_json_.find(key) == config_json_.end()) {
            config_json_.emplace(std::make_pair(key, value));
        } else if (config_json_.at(key).empty() || overwrite_non_empty) {
            config_json_.at(key) = value;
        }
    }
}

envvar_map_t AppConfigReader::Impl_::getSimpleEnvMap() {
    envvar_map_t map;

    for(const auto& [key, value] : config_json_.items()) {
        if (map.find(key) != map.end()) {
            OES_WARNING("Environment variable key \"{}\" already exists (duplicated). Keeping original value.", key);
            continue;
        }
        
        if (value.is_string()) {
            std::string tmp (value.get<std::string>());
            map.emplace(std::make_pair(key, tmp));
        } else {
            map.emplace(std::make_pair(key, ""));
        }
    }

    return map;
}

std::unique_ptr<AppConfigReader::Impl_> AppConfigReader::getOesEnvImpl_(const std::string& default_config_file) {
    std::string oes_env = AppConfigReader::getEnv("OES_ENV");
    if(oes_env.empty() || oes_env == "default") {
        return nullptr;
    }
    
    std::size_t found = default_config_file.find_last_of('.');
    std::string new_file_path(default_config_file.substr(0, found));
    new_file_path += "_" + oes_env + ".json";

    auto abs_path = getAbsFilePath(new_file_path);
    if (abs_path.empty() || !std::filesystem::exists(abs_path)) {
        OES_DEBUG("Could not read additional file \"{}\", using file \"{}\".", new_file_path, default_config_file);
        return nullptr;
    }

    return std::make_unique<Impl_>(abs_path);
}

AppConfigReader::AppConfigReader(const std::string& default_config_file, const std::string& custom_file_path) {

    std::string abs_path;
    if(!custom_file_path.empty()) {
        if (!std::filesystem::exists(custom_file_path)) {
            OES_ERROR("Configuration file {} not found!", custom_file_path);
            assert(false);    
        } 
        // look for default config file in custom file directory
        std::filesystem::path default_config_file_path(custom_file_path);
        default_config_file_path = default_config_file_path.parent_path();
        default_config_file_path /= default_config_file;
        abs_path = default_config_file_path.string();
        if (!std::filesystem::exists(abs_path)) {
            // look for default config file in default locations
            abs_path = getDefaultConfigPath(default_config_file);
        }
    } else {
        abs_path = getAbsFilePath(default_config_file);
    }
    
    if (!std::filesystem::exists(abs_path)) {
        OES_ERROR("Configuration file {} not found! Running in {}.", default_config_file, std::filesystem::current_path().string());
        assert(false);    
    } 

    impl_ptr_.reset(new Impl_(abs_path));

    std::unique_ptr<AppConfigReader::Impl_> tmp_impl;
    if (!custom_file_path.empty()) {
        tmp_impl = std::make_unique<Impl_>(custom_file_path);
    } else {
        tmp_impl = getOesEnvImpl_(default_config_file);
    }
    if (tmp_impl) {
        impl_ptr_->merge(tmp_impl->getJson(), true);
    }
}

AppConfigReader::~AppConfigReader() {}

std::string AppConfigReader::getAbsFilePath(const std::string& file_path) {
    // check if file_name is a path or a simple file name. If it is simple file name, then will look the file in default config dir
    std::string real_path;
    if (std::filesystem::path path(file_path); path.filename() == path) {
        real_path = getDefaultConfigPath(file_path);
    } else  {
        real_path = file_path;
    }

    return real_path;
}


std::string AppConfigReader::getEnv(const std::string& env_var_name) {
    const char* cstr = std::getenv(env_var_name.c_str());

    return cstr ? std::string(cstr) : std::string();
}

std::string AppConfigReader::getDefaultConfigPath(const std::string& file_name) {
    auto path = std::filesystem::current_path();

    if (std::filesystem::exists(path/file_name)) {
        path /= file_name;
        return path.string();
    }
    
    // try parent folder for tests
    path = path.parent_path();
    path /= file_name;
    if (std::filesystem::exists(path)) {
        return path.string();
    }
    
    // try usr/bin for openETPServer
#ifdef __linux__
    path = "/usr/bin";
    path /= file_name;
#endif

    return path.string();
}


envvar_map_t AppConfigReader::getSimpleEnvMap() {
    return impl_ptr_->getSimpleEnvMap();
}

}
