#include "core/api/framework/ModuleFactory.h"
#include "framework/ModuleBase.h"

#include <string>

namespace {

class ConfigTestModule : public module_context::framework::ModuleBase {
public:
    std::string ModuleName() const override {
        return "config_test_module";
    }
};

}  // namespace

MC_DECLARE_MODULE_FACTORY(ConfigTestModule)
