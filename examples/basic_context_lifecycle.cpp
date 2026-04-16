#include "core/api/framework/IContext.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

#include <iostream>

int main() {
    module_context::framework::IContext& context =
        module_context::framework::IContext::Instance();

    foundation::base::Result<void> result = context.Init();
    if (!result.IsOk()) {
        std::cerr << "Init failed: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    result = context.Start();
    if (!result.IsOk()) {
        std::cerr << "Start failed: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    result = context.Stop();
    if (!result.IsOk()) {
        std::cerr << "Stop failed: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    result = context.Fini();
    if (!result.IsOk()) {
        std::cerr << "Fini failed: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    std::cout << "Context lifecycle completed successfully." << std::endl;
    return 0;
}
