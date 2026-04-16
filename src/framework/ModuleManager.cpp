#include "framework/ModuleManager.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/config/ConfigReader.h"
#include "foundation/config/ConfigValue.h"
#include "foundation/base/Result.h"
#include "foundation/filesystem/PathUtils.h"

#include "core/api/framework/ModuleFactory.h"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace module_context {
namespace framework {

namespace {

struct ModuleConfigSpec {
    std::string name;
    std::string library_path;
};

static const char kConfigSchemaVersionKey[] = "schema_version";
static const std::int64_t kConfigSchemaVersion = 1;
static const char kConfigModulesKey[] = "modules";
static const char kConfigNameKey[] = "name";
static const char kConfigLibraryPathKey[] = "library_path";

std::string BuildMessage(const std::string& prefix, const std::string& detail) {
    if (detail.empty()) {
        return prefix;
    }

    return prefix + ": " + detail;
}

foundation::base::Result<ModuleConfigSpec> MakeConfigSpecError(
    const std::string& message) {
    return foundation::base::Result<ModuleConfigSpec>(
        foundation::base::ErrorCode::kParseError,
        message);
}

foundation::base::Result<ModuleConfigSpec> ParseModuleConfigEntry(
    const foundation::config::ConfigValue& value,
    std::size_t index,
    const std::string& config_directory) {
    if (!value.IsObject()) {
        return MakeConfigSpecError(
            "Invalid module config entry at modules[" +
            std::to_string(static_cast<unsigned long long>(index)) +
            "]: expected object");
    }

    foundation::base::Result<foundation::config::ConfigValue> name_value =
        value.ObjectGet(kConfigNameKey);
    if (!name_value.IsOk()) {
        return MakeConfigSpecError(
            "Invalid module config entry at modules[" +
            std::to_string(static_cast<unsigned long long>(index)) +
            "]: missing 'name'");
    }

    foundation::base::Result<foundation::config::ConfigValue> library_path_value =
        value.ObjectGet(kConfigLibraryPathKey);
    if (!library_path_value.IsOk()) {
        return MakeConfigSpecError(
            "Invalid module config entry at modules[" +
            std::to_string(static_cast<unsigned long long>(index)) +
            "]: missing 'library_path'");
    }

    foundation::base::Result<std::string> name = name_value.Value().AsString();
    foundation::base::Result<std::string> library_path =
        library_path_value.Value().AsString();
    if (!name.IsOk() || !library_path.IsOk()) {
        return MakeConfigSpecError(
            "Invalid module config entry at modules[" +
            std::to_string(static_cast<unsigned long long>(index)) +
            "]: 'name' and 'library_path' must both be strings");
    }

    if (name.Value().empty() || library_path.Value().empty()) {
        return MakeConfigSpecError(
            "Invalid module config entry at modules[" +
            std::to_string(static_cast<unsigned long long>(index)) +
            "]: 'name' and 'library_path' must not be empty");
    }

    ModuleConfigSpec spec;
    spec.name = name.Value();
    // 将路径归一化为绝对路径，避免不同工作目录导致加载行为不一致。
    if (foundation::filesystem::IsAbsolute(library_path.Value())) {
        spec.library_path =
            foundation::filesystem::GetAbsolutePath(library_path.Value());
    } else {
        spec.library_path = foundation::filesystem::GetAbsolutePath(
            foundation::filesystem::Join(
                config_directory,
                library_path.Value()));
    }

    return foundation::base::Result<ModuleConfigSpec>(spec);
}

foundation::base::Result<std::vector<ModuleConfigSpec> > ReadModuleConfig(
    const std::string& config_file_path) {
    foundation::config::ConfigReader reader;

    foundation::base::ErrorCode load_error =
        reader.LoadFromJsonFile(config_file_path);
    if (load_error != foundation::base::ErrorCode::kOk) {
        return foundation::base::Result<std::vector<ModuleConfigSpec> >(
            load_error,
            "Failed to load module config file '" + config_file_path +
                "' as JSON"
                " (error=" + foundation::base::ErrorCodeToString(load_error) +
                ")");
    }

    if (!reader.Root().IsObject()) {
        return foundation::base::Result<std::vector<ModuleConfigSpec> >(
            foundation::base::ErrorCode::kParseError,
            "Module config root must be a JSON object");
    }

    foundation::base::Result<std::int64_t> schema_version =
        reader.GetInt64(kConfigSchemaVersionKey);
    if (!schema_version.IsOk()) {
        return foundation::base::Result<std::vector<ModuleConfigSpec> >(
            foundation::base::ErrorCode::kParseError,
            "Module config must contain integer field '" +
                std::string(kConfigSchemaVersionKey) + "'");
    }

    if (schema_version.Value() != kConfigSchemaVersion) {
        return foundation::base::Result<std::vector<ModuleConfigSpec> >(
            foundation::base::ErrorCode::kVersionMismatch,
            "Unsupported module config schema_version: " +
                std::to_string(
                    static_cast<long long>(schema_version.Value())));
    }

    foundation::base::Result<foundation::config::ConfigValue> modules_value =
        reader.Get(kConfigModulesKey);
    if (!modules_value.IsOk()) {
        return foundation::base::Result<std::vector<ModuleConfigSpec> >(
            foundation::base::ErrorCode::kParseError,
            "Module config must contain array field '" +
                std::string(kConfigModulesKey) + "'");
    }

    const foundation::config::ConfigValue::Array* modules =
        modules_value.Value().GetArray();
    if (modules == NULL) {
        return foundation::base::Result<std::vector<ModuleConfigSpec> >(
            foundation::base::ErrorCode::kParseError,
            "Module config field '" + std::string(kConfigModulesKey) +
                "' must be an array");
    }

    const std::string config_directory =
        foundation::filesystem::GetParentPath(
            foundation::filesystem::GetAbsolutePath(config_file_path));

    std::unordered_set<std::string> seen_names;
    std::vector<ModuleConfigSpec> specs;
    specs.reserve(modules->size());

    for (std::size_t index = 0; index < modules->size(); ++index) {
        foundation::base::Result<ModuleConfigSpec> spec =
            ParseModuleConfigEntry(
                (*modules)[index],
                index,
                config_directory);
        if (!spec.IsOk()) {
            return foundation::base::Result<std::vector<ModuleConfigSpec> >(
                spec.GetError(),
                BuildMessage(
                    "Failed to parse module config file '" +
                        config_file_path + "'",
                    spec.GetMessage()));
        }

        // 在配置解析阶段就拦截重复模块名，避免后续半加载状态。
        if (!seen_names.insert(spec.Value().name).second) {
            return foundation::base::Result<std::vector<ModuleConfigSpec> >(
                foundation::base::ErrorCode::kAlreadyExists,
                "Module config contains duplicate module name '" +
                    spec.Value().name + "'");
        }

        specs.push_back(spec.Value());
    }

    return foundation::base::Result<std::vector<ModuleConfigSpec> >(specs);
}

foundation::base::Result<void> PrefixModuleError(
    const char* operation,
    const std::string& module_name,
    const foundation::base::Result<void>& result) {
    if (result.IsOk()) {
        return result;
    }

    return foundation::base::Result<void>(
        result.GetError(),
        BuildMessage(
            std::string("ModuleManager::") + operation +
                " failed for module '" + module_name + "'",
            result.GetMessage()));
}

}  // namespace

ModuleManager::ModuleManager()
    : modules_by_name_(),
      module_order_() {
}

ModuleManager::~ModuleManager() {
    (void)Fini();
}

foundation::base::Result<void> ModuleManager::LoadModules(
    const std::string& config_file_path) {
    if (config_file_path.empty()) {
        return foundation::base::Result<void>(
            foundation::base::ErrorCode::kInvalidArgument,
            "ModuleManager::LoadModules failed: config file path is empty");
    }

    foundation::base::Result<std::vector<ModuleConfigSpec> > entries =
        ReadModuleConfig(config_file_path);
    if (!entries.IsOk()) {
        return foundation::base::Result<void>(
            entries.GetError(),
            entries.GetMessage());
    }

    // 预检查：如果管理器中已存在同名模块，则拒绝本次批量加载。
    for (std::size_t index = 0; index < entries.Value().size(); ++index) {
        if (modules_by_name_.find(entries.Value()[index].name) !=
            modules_by_name_.end()) {
            return foundation::base::Result<void>(
                foundation::base::ErrorCode::kAlreadyExists,
                "ModuleManager::LoadModules failed: module '" +
                    entries.Value()[index].name + "' is already loaded");
        }
    }

    // 两阶段提交：
    // 1) 先全部创建到 staged_modules；
    // 2) 全成功后再统一写入 modules_by_name_ / module_order_。
    std::vector<std::pair<std::string, ModuleHandle> > staged_modules;
    staged_modules.reserve(entries.Value().size());

    for (std::size_t index = 0; index < entries.Value().size(); ++index) {
        foundation::base::Result<ModuleHandle> created_module =
            CreateModuleHandle(entries.Value()[index].library_path);
        if (!created_module.IsOk()) {
            return foundation::base::Result<void>(
                created_module.GetError(),
                created_module.GetMessage());
        }

        staged_modules.push_back(
            std::make_pair(
                entries.Value()[index].name,
                std::move(created_module.Value())));
    }

    for (std::size_t index = 0; index < staged_modules.size(); ++index) {
        StoreLoadedModule(
            staged_modules[index].first,
            std::move(staged_modules[index].second));
    }

    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleManager::LoadModule(
    const std::string& name,
    const std::string& library_path) {
    if (name.empty() || library_path.empty()) {
        return foundation::base::Result<void>(
            foundation::base::ErrorCode::kInvalidArgument,
            "ModuleManager::LoadModule failed: name or library path is empty");
    }

    if (modules_by_name_.find(name) != modules_by_name_.end()) {
        return foundation::base::Result<void>(
            foundation::base::ErrorCode::kAlreadyExists,
            "ModuleManager::LoadModule failed: module '" + name +
                "' is already loaded");
    }

    const std::string normalized_library_path =
        foundation::filesystem::GetAbsolutePath(library_path);

    foundation::base::Result<ModuleHandle> created_module =
        CreateModuleHandle(normalized_library_path);
    if (!created_module.IsOk()) {
        return foundation::base::Result<void>(
            created_module.GetError(),
            created_module.GetMessage());
    }

    StoreLoadedModule(name, std::move(created_module.Value()));
    return foundation::base::MakeSuccess();
}

foundation::base::Result<ModuleManager::ModuleHandle>
ModuleManager::CreateModuleHandle(
    const std::string& normalized_library_path) {
    // 使用临时 loader 完成“打开库 + 创建实例”，最终返回可移动的句柄。
    ModuleLoader loader;
    foundation::base::Result<void> open_result = loader.Open(
        normalized_library_path,
        kModulePluginApiVersion);
    if (!open_result.IsOk()) {
        return foundation::base::Result<ModuleHandle>(
            open_result.GetError(),
            BuildMessage(
                "ModuleManager failed to open plugin '" +
                normalized_library_path + "'",
                open_result.GetMessage()));
    }

    foundation::base::Result<ModuleHandle> created_module = loader.Create();
    if (!created_module.IsOk()) {
        return foundation::base::Result<ModuleHandle>(
            created_module.GetError(),
            BuildMessage(
                "ModuleManager failed to create plugin instance from '" +
                normalized_library_path + "'",
                created_module.GetMessage()));
    }

    return foundation::base::Result<ModuleHandle>(
        std::move(created_module.Value()));
}

void ModuleManager::StoreLoadedModule(
    const std::string& name,
    ModuleHandle module) {
    modules_by_name_.emplace(name, std::move(module));
    module_order_.push_back(name);
}

foundation::base::Result<void> ModuleManager::Init(IContext& ctx) {
    // Init/Start 按加载顺序执行，便于满足模块间前置依赖关系。
    for (std::size_t index = 0; index < module_order_.size(); ++index) {
        ModuleMap::iterator it = modules_by_name_.find(module_order_[index]);
        if (it == modules_by_name_.end() || !it->second.IsValid()) {
            continue;
        }

        foundation::base::Result<void> init_result =
            it->second->Init(ctx);
        if (!init_result.IsOk()) {
            return PrefixModuleError(
                "Init",
                module_order_[index],
                init_result);
        }
    }

    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleManager::Start() {
    for (std::size_t index = 0; index < module_order_.size(); ++index) {
        ModuleMap::iterator it = modules_by_name_.find(module_order_[index]);
        if (it == modules_by_name_.end() || !it->second.IsValid()) {
            continue;
        }

        foundation::base::Result<void> start_result =
            it->second->Start();
        if (!start_result.IsOk()) {
            return PrefixModuleError(
                "Start",
                module_order_[index],
                start_result);
        }
    }

    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleManager::Stop() {
    foundation::base::Result<void> first_error =
        foundation::base::MakeSuccess();

    // Stop/Fini 按逆序执行，对应“后启动先停止”的栈式回收语义。
    for (ModuleOrder::reverse_iterator it = module_order_.rbegin();
         it != module_order_.rend();
         ++it) {
        ModuleMap::iterator module = modules_by_name_.find(*it);
        if (module == modules_by_name_.end() || !module->second.IsValid()) {
            continue;
        }

        foundation::base::Result<void> stop_result = module->second->Stop();
        if (first_error.IsOk() && !stop_result.IsOk()) {
            first_error = PrefixModuleError("Stop", *it, stop_result);
        }
    }

    return first_error;
}

foundation::base::Result<void> ModuleManager::Fini() {
    foundation::base::Result<void> first_error =
        foundation::base::MakeSuccess();

    for (ModuleOrder::reverse_iterator it = module_order_.rbegin();
         it != module_order_.rend();
         ++it) {
        ModuleMap::iterator module = modules_by_name_.find(*it);
        if (module == modules_by_name_.end() || !module->second.IsValid()) {
            continue;
        }

        foundation::base::Result<void> fini_result = module->second->Fini();
        if (first_error.IsOk() && !fini_result.IsOk()) {
            first_error = PrefixModuleError("Fini", *it, fini_result);
        }
    }

    // 完成反初始化后清空容器，释放句柄并重置管理器状态。
    modules_by_name_.clear();
    module_order_.clear();

    return first_error;
}

foundation::base::Result<IModule*> ModuleManager::Module(
    const std::string& name) {
    ModuleMap::iterator it = modules_by_name_.find(name);
    if (it == modules_by_name_.end()) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kNotFound,
            "ModuleManager::Module failed: module '" + name + "' was not found");
    }

    IModule* module = it->second.Get();
    if (module == NULL) {
        return foundation::base::Result<IModule*>(
            foundation::base::ErrorCode::kInvalidState,
            "ModuleManager::Module failed: module '" + name +
                "' is not available");
    }

    return foundation::base::Result<IModule*>(module);
}

}  // namespace framework
}  // namespace module_context
