#pragma once

namespace module_context {
namespace framework {

enum class ModuleState {
    Created = 0,  ///< 模块对象已构造，但尚未 Init。
    Inited,       ///< Init 成功，资源已准备完成。
    Started,      ///< Start 成功，模块业务处于运行状态。
    Stopped,      ///< Stop 成功，运行已停止，可再次 Start 或进入 Fini。
    Fini          ///< Fini 完成，资源已释放。
};

}  // namespace framework
}  // namespace module_context
