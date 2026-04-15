#include "core/framework/module_manager.h"

#include "core/api/framework/module_factory.h"
#include "core/framework/shared_library.h"

namespace mc {

ModuleManager::ModuleManager()
    : state_(ModuleState::Created)
{
}

ModuleManager::~ModuleManager()
{
    if (state_ != ModuleState::Fini) {
        fini();
    }
}

bool ModuleManager::loadModule(const std::string& name,
                               const std::string& libraryPath)
{
    if (name.empty() || libraryPath.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < modules_.size(); ++i) {
        if (modules_[i].name == name) {
            return false;
        }
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
    return true;
}

void ModuleManager::init()
{
    for (std::size_t i = 0; i < modules_.size(); ++i) {
        if (modules_[i].module) {
            modules_[i].module->init();
        }
    }

    state_ = ModuleState::Inited;
}

void ModuleManager::start()
{
    for (std::size_t i = 0; i < modules_.size(); ++i) {
        if (modules_[i].module) {
            modules_[i].module->start();
        }
    }

    state_ = ModuleState::Started;
}

void ModuleManager::stop()
{
    for (std::size_t i = modules_.size(); i > 0; --i) {
        if (modules_[i - 1].module) {
            modules_[i - 1].module->stop();
        }
    }

    state_ = ModuleState::Stopped;
}

void ModuleManager::fini()
{
    if (state_ == ModuleState::Started) {
        stop();
    }

    if (state_ == ModuleState::Created) {
        state_ = ModuleState::Fini;
        return;
    }

    for (std::size_t i = modules_.size(); i > 0; --i) {
        if (modules_[i - 1].module) {
            modules_[i - 1].module->fini();
        }
    }

    state_ = ModuleState::Fini;
}

ModuleState ModuleManager::state() const
{
    return state_;
}

IModule* ModuleManager::module(const std::string& name)
{
    for (std::size_t i = 0; i < modules_.size(); ++i) {
        if (modules_[i].name == name) {
            return modules_[i].module.get();
        }
    }

    return nullptr;
}

std::vector<std::string> ModuleManager::moduleNames() const
{
    std::vector<std::string> names;
    names.reserve(modules_.size());

    for (std::size_t i = 0; i < modules_.size(); ++i) {
        names.push_back(modules_[i].name);
    }

    return names;
}

} // namespace mc