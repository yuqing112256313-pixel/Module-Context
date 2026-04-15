#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"
#include "core/api/framework/imodule_manager.h"
#include "core/framework/shared_library.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mc {

class MC_FRAMEWORK_API ModuleManager final : public IModuleManager
{
public:
    ModuleManager();
    ~ModuleManager() override;

    bool loadModule(const std::string& name,
                    const std::string& libraryPath) override;

    void init(IContext& ctx) override;
    void start() override;
    void stop() override;
    void fini() override;

    ModuleState state() const override;

    IModule* module(const std::string& name) override;
    std::vector<std::string> moduleNames() const override;

private:
    using ModuleDeleter = std::function<void(IModule*)>;
    using ModulePtr = std::unique_ptr<IModule, ModuleDeleter>;

    struct ModuleRecord
    {
        std::string name;
        std::string libraryPath;
        std::unique_ptr<SharedLibrary> library;
        ModulePtr module;

        ModuleRecord()
            : module(nullptr, ModuleDeleter())
        {
        }
    };

private:
    std::vector<ModuleRecord> modules_;
    ModuleState state_;
};

} // namespace mc
