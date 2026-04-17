#include "framework/ModuleManager.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/config/ConfigValue.h"
#include "foundation/filesystem/FileUtils.h"
#include "foundation/filesystem/PathUtils.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << std::endl;
        return false;
    }
    return true;
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

bool WriteConfigFile(const std::string& file_name, const std::string& content) {
    foundation::base::Result<void> write_result =
        foundation::filesystem::WriteAllText(
            foundation::filesystem::Join(MC_TEST_BINARY_DIR, file_name),
            content);
    return Expect(
        write_result.IsOk(),
        "Failed to write config file '" + file_name + "'");
}

std::string ConfigPath(const std::string& file_name) {
    return foundation::filesystem::Join(MC_TEST_BINARY_DIR, file_name);
}

bool RunValidInlineConfigCase() {
    std::ostringstream config;
    config
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"modules\": [\n"
        << "    {\n"
        << "      \"name\": \"config_test_module\",\n"
        << "      \"library_path\": \"" << JsonEscape(MC_TEST_CONFIG_PLUGIN_PATH) << "\",\n"
        << "      \"config\": {\n"
        << "        \"role\": \"master\",\n"
        << "        \"worker_pool\": {\n"
        << "          \"thread_count\": 4\n"
        << "        }\n"
        << "      }\n"
        << "    }\n"
        << "  ]\n"
        << "}\n";

    if (!WriteConfigFile("module_manager_config_valid.json", config.str())) {
        return false;
    }

    module_context::framework::ModuleManager manager;
    foundation::base::Result<void> load_result =
        manager.LoadModules(ConfigPath("module_manager_config_valid.json"));
    if (!Expect(load_result.IsOk(), "LoadModules should accept inline module config")) {
        return false;
    }

    foundation::base::Result<module_context::framework::IModule*> module =
        manager.Module("config_test_module");
    if (!Expect(module.IsOk(), "Loaded module should be queryable by name")) {
        return false;
    }

    foundation::base::Result<foundation::config::ConfigValue> config_value =
        manager.ModuleConfig("config_test_module");
    if (!Expect(config_value.IsOk(), "ModuleConfig should return inline config")) {
        return false;
    }

    foundation::base::Result<foundation::config::ConfigValue> role =
        config_value.Value().ObjectGet("role");
    if (!Expect(role.IsOk(), "Module config should contain 'role'")) {
        return false;
    }
    if (!Expect(
            role.Value().AsString().IsOk() &&
                role.Value().AsString().Value() == "master",
            "Module config role should equal 'master'")) {
        return false;
    }

    foundation::base::Result<foundation::config::ConfigValue> worker_pool =
        config_value.Value().ObjectGet("worker_pool");
    if (!Expect(worker_pool.IsOk(), "Module config should contain worker_pool")) {
        return false;
    }

    foundation::base::Result<foundation::config::ConfigValue> thread_count =
        worker_pool.Value().ObjectGet("thread_count");
    if (!Expect(thread_count.IsOk(), "worker_pool should contain thread_count")) {
        return false;
    }
    if (!Expect(
            thread_count.Value().AsInt64().IsOk() &&
                thread_count.Value().AsInt64().Value() == 4,
            "worker_pool.thread_count should equal 4")) {
        return false;
    }

    foundation::base::Result<foundation::config::ConfigValue> missing_config =
        manager.ModuleConfig("missing_module");
    if (!Expect(
            !missing_config.IsOk() &&
                missing_config.GetError() == foundation::base::ErrorCode::kNotFound,
            "ModuleConfig should return kNotFound for unknown modules")) {
        return false;
    }

    foundation::base::Result<void> fini_result = manager.Fini();
    if (!Expect(fini_result.IsOk(), "Fini should succeed after LoadModules")) {
        return false;
    }

    return true;
}

bool RunConfigMustBeObjectCase() {
    std::ostringstream config;
    config
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"modules\": [\n"
        << "    {\n"
        << "      \"name\": \"config_test_module\",\n"
        << "      \"library_path\": \"" << JsonEscape(MC_TEST_CONFIG_PLUGIN_PATH) << "\",\n"
        << "      \"config\": 123\n"
        << "    }\n"
        << "  ]\n"
        << "}\n";

    if (!WriteConfigFile("module_manager_config_invalid_object.json", config.str())) {
        return false;
    }

    module_context::framework::ModuleManager manager;
    foundation::base::Result<void> load_result =
        manager.LoadModules(ConfigPath("module_manager_config_invalid_object.json"));
    return Expect(
        !load_result.IsOk() &&
            load_result.GetError() == foundation::base::ErrorCode::kParseError,
        "LoadModules should reject non-object module config");
}

bool RunDuplicateModuleNameCase() {
    std::ostringstream config;
    config
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"modules\": [\n"
        << "    {\n"
        << "      \"name\": \"config_test_module\",\n"
        << "      \"library_path\": \"" << JsonEscape(MC_TEST_CONFIG_PLUGIN_PATH) << "\"\n"
        << "    },\n"
        << "    {\n"
        << "      \"name\": \"config_test_module\",\n"
        << "      \"library_path\": \"" << JsonEscape(MC_TEST_CONFIG_PLUGIN_PATH) << "\"\n"
        << "    }\n"
        << "  ]\n"
        << "}\n";

    if (!WriteConfigFile("module_manager_config_duplicate.json", config.str())) {
        return false;
    }

    module_context::framework::ModuleManager manager;
    foundation::base::Result<void> load_result =
        manager.LoadModules(ConfigPath("module_manager_config_duplicate.json"));
    return Expect(
        !load_result.IsOk() &&
            load_result.GetError() == foundation::base::ErrorCode::kAlreadyExists,
        "LoadModules should reject duplicate module names");
}

bool RunMissingLibraryPathCase() {
    std::ostringstream config;
    config
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"modules\": [\n"
        << "    {\n"
        << "      \"name\": \"config_test_module\"\n"
        << "    }\n"
        << "  ]\n"
        << "}\n";

    if (!WriteConfigFile("module_manager_config_missing_library.json", config.str())) {
        return false;
    }

    module_context::framework::ModuleManager manager;
    foundation::base::Result<void> load_result =
        manager.LoadModules(ConfigPath("module_manager_config_missing_library.json"));
    return Expect(
        !load_result.IsOk() &&
            load_result.GetError() == foundation::base::ErrorCode::kParseError,
        "LoadModules should reject entries without library_path");
}

}  // namespace

int main() {
    if (!RunValidInlineConfigCase()) {
        return 1;
    }
    if (!RunConfigMustBeObjectCase()) {
        return 1;
    }
    if (!RunDuplicateModuleNameCase()) {
        return 1;
    }
    if (!RunMissingLibraryPathCase()) {
        return 1;
    }

    std::cout << "[PASSED] module_manager_config_test" << std::endl;
    return 0;
}
