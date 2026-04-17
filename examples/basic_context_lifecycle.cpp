#include "module_context/framework/IContext.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

#include <iostream>

int main() {
    module_context::framework::IContext& context =
        module_context::framework::IContext::Instance();

    foundation::base::Result<void> result = context.Init();
    if (!result.IsOk()) {
        std::cerr << "上下文初始化失败: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    result = context.Start();
    if (!result.IsOk()) {
        std::cerr << "上下文启动失败: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    result = context.Stop();
    if (!result.IsOk()) {
        std::cerr << "上下文停止失败: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    result = context.Fini();
    if (!result.IsOk()) {
        std::cerr << "上下文释放失败: " << result.GetMessage() << std::endl;
        return static_cast<int>(result.GetError());
    }

    std::cout << "上下文生命周期执行成功。" << std::endl;
    return 0;
}
