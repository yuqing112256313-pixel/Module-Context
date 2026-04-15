#include "core/framework/module_manager.h"

#include "core/api/framework/module_factory.h"
#include "core/framework/shared_library.h"

#include <fstream>

namespace module_context {
namespace framework {

namespace detail {

std::string TrimValue(const std::string& value)
{
    std::size_t begin = 0;
    while (begin < value.size()
           && (value[begin] == ' '
               || value[begin] == '\t'
               || value[begin] == '\r'
               || value[begin] == '\n')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin
           && (value[end - 1] == ' '
               || value[end - 1] == '\t'
               || value[end - 1] == '\r'
               || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(begin, end - begin);
}

bool ParseLine(const std::string& line,
               std::string& name,
               std::string& libraryPath)
{
    const std::size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;
    }

    name = TrimValue(line.substr(0, pos));
    libraryPath = TrimValue(line.substr(pos + 1));

    return !name.empty() && !libraryPath.empty();
}

} // namespace detail

ModuleManager::ModuleManager()
{
}

ModuleManager::~ModuleManager()
{
    Fini();
}

bool ModuleManager::LoadModules(const std::string& configFilePath)
{
    std::ifstream ifs(configFilePath.c_str());
    if (!ifs.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        line = detail::TrimValue(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::string name;
        std::string libraryPath;
        if (!detail::ParseLine(line, name, libraryPath)) {
            return false;
        }

        if (!LoadModule(name, libraryPath)) {
            return false;
        }
    }

    return true;
}

bool ModuleManager::LoadModule(const std::string& name,
                              const std::string& libraryPath)
{
    if (name.empty() || libraryPath.empty()) {
        return false;
    }

    if (moduleIndexByName_.find(name) != moduleIndexByName_.end()) {
        return false;
    }

    std::unique_ptr<SharedLibrary> library(new SharedLibrary(libraryPath));
    if (!library->open()) {
        return false;
    }

    CreateModuleFn createFn = library->resolve<CreateModuleFn>(kCreateModuleSymbol);
    DestroyModuleFn destroyFn = library->resolve<DestroyModuleFn>(kDestroyModuleSymbol);
    if (!createFn || !destroyFn) {
        return false;
    }

    IModule* rawModule = createFn();
    if (!rawModule) {
        return false;
    }

    ModuleRecord record;
    record.name = name;
    record.libraryPath = libraryPath;
    record.library = std::move(library);
    record.module = ModulePtr(rawModule, [destroyFn](IModule* module) {
        if (module) {
            destroyFn(module);
        }
    });

    modules_.push_back(std::move(record));
    moduleIndexByName_.emplace(name, modules_.size() - 1);
    return true;
}

void ModuleManager::Init(IContext& ctx)
{
    for (auto& moduleRecord : modules_) {
        if (moduleRecord.module) {
            moduleRecord.module->Init(ctx);
        }
    }
}

void ModuleManager::Start()
{
    for (auto& moduleRecord : modules_) {
        if (moduleRecord.module) {
            moduleRecord.module->Start();
        }
    }
}

void ModuleManager::Stop()
{
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        if (it->module) {
            it->module->Stop();
        }
    }
}

void ModuleManager::Fini()
{
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        if (it->module) {
            it->module->Fini();
        }
    }
}

IModule* ModuleManager::Module(const std::string& name)
{
    const auto it = moduleIndexByName_.find(name);
    if (it == moduleIndexByName_.end()) {
        return nullptr;
    }

    return modules_[it->second].module.get();
}

} // namespace framework
} // namespace module_context
