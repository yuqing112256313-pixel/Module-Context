#pragma once

#include "core/api/framework/icontext.h"

#include <memory>
#include <string>

namespace mc {

class Context final : public IContext
{
public:
    Context();
    ~Context() override;

    bool loadModules(const std::string& configFilePath) override;

    void init() override;
    void start() override;
    void stop() override;
    void fini() override;

    ModuleState state() const override;

    IModuleManager* moduleMgr() override;

private:
    std::unique_ptr<IModuleManager> moduleManager_;
    ModuleState state_;
};

} // namespace mc