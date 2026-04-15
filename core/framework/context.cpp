#include "core/framework/context.h"

#include "core/framework/module_manager.h"

namespace module_context {
namespace framework {

IContext& IContext::Instance()
{
    static Context ctx;
    return ctx;
}

Context::Context()
    : moduleManager_(new ModuleManager())
{
}

Context::~Context()
{
    Fini();
}

void Context::Init()
{
    if (moduleManager_) {
        moduleManager_->Init(*this);
    }
}

void Context::Start()
{
    if (moduleManager_) {
        moduleManager_->Start();
    }
}

void Context::Stop()
{
    if (moduleManager_) {
        moduleManager_->Stop();
    }
}

void Context::Fini()
{
    if (moduleManager_) {
        moduleManager_->Fini();
    }
}

IModuleManager* Context::ModuleManager()
{
    return moduleManager_.get();
}

} // namespace framework
} // namespace module_context
