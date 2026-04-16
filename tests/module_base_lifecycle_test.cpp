#include "framework/ModuleBase.h"

#include "core/api/framework/IModuleManager.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

#include <iostream>
#include <string>

namespace {

class DummyContext : public module_context::framework::IContext {
public:
    foundation::base::Result<void> Init() override {
        return foundation::base::MakeSuccess();
    }

    foundation::base::Result<void> Start() override {
        return foundation::base::MakeSuccess();
    }

    foundation::base::Result<void> Stop() override {
        return foundation::base::MakeSuccess();
    }

    foundation::base::Result<void> Fini() override {
        return foundation::base::MakeSuccess();
    }

    module_context::framework::IModuleManager* ModuleManager() override {
        return NULL;
    }
};

class TestModule : public module_context::framework::ModuleBase {
public:
    TestModule()
        : on_init_calls_(0),
          on_start_calls_(0),
          on_stop_calls_(0),
          on_fini_calls_(0) {
    }

    std::string ModuleName() const override {
        return "test-module";
    }

    int InitCalls() const { return on_init_calls_; }
    int StartCalls() const { return on_start_calls_; }
    int StopCalls() const { return on_stop_calls_; }
    int FiniCalls() const { return on_fini_calls_; }

protected:
    foundation::base::Result<void> OnInit() override {
        ++on_init_calls_;
        return foundation::base::MakeSuccess();
    }

    foundation::base::Result<void> OnStart() override {
        ++on_start_calls_;
        return foundation::base::MakeSuccess();
    }

    foundation::base::Result<void> OnStop() override {
        ++on_stop_calls_;
        return foundation::base::MakeSuccess();
    }

    foundation::base::Result<void> OnFini() override {
        ++on_fini_calls_;
        return foundation::base::MakeSuccess();
    }

private:
    int on_init_calls_;
    int on_start_calls_;
    int on_stop_calls_;
    int on_fini_calls_;
};

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << std::endl;
        return false;
    }

    return true;
}

}  // namespace

int main() {
    DummyContext ctx;

    {
        TestModule module;

        if (!Expect(module.State() == module_context::framework::ModuleState::Created,
                    "initial state should be Created")) {
            return 1;
        }

        foundation::base::Result<void> result = module.Start();
        if (!Expect(!result.IsOk(), "Start before Init should fail")) {
            return 1;
        }
        if (!Expect(result.GetError() == foundation::base::ErrorCode::kInvalidState,
                    "Start before Init should return kInvalidState")) {
            return 1;
        }

        result = module.Init(ctx);
        if (!Expect(result.IsOk(), "Init should succeed from Created")) {
            return 1;
        }

        result = module.Start();
        if (!Expect(result.IsOk(), "Start should succeed from Inited")) {
            return 1;
        }

        result = module.Stop();
        if (!Expect(result.IsOk(), "Stop should succeed from Started")) {
            return 1;
        }

        result = module.Fini();
        if (!Expect(result.IsOk(), "Fini should succeed from Stopped")) {
            return 1;
        }

        if (!Expect(module.InitCalls() == 1, "OnInit should be called exactly once")) {
            return 1;
        }
        if (!Expect(module.StartCalls() == 1, "OnStart should be called exactly once")) {
            return 1;
        }
        if (!Expect(module.StopCalls() == 1, "OnStop should be called exactly once")) {
            return 1;
        }
        if (!Expect(module.FiniCalls() == 1, "OnFini should be called exactly once")) {
            return 1;
        }

        if (!Expect(module.State() == module_context::framework::ModuleState::Fini,
                    "state should be Fini after Fini()")) {
            return 1;
        }
    }

    {
        TestModule module;
        foundation::base::Result<void> result = module.Fini();
        if (!Expect(result.IsOk(), "Fini from Created should succeed")) {
            return 1;
        }
        if (!Expect(module.FiniCalls() == 0,
                    "Fini from Created should not call OnFini")) {
            return 1;
        }
        if (!Expect(module.State() == module_context::framework::ModuleState::Fini,
                    "Fini from Created should move state to Fini")) {
            return 1;
        }
    }

    std::cout << "[PASSED] module_base_lifecycle_test" << std::endl;
    return 0;
}
