#pragma once

namespace module_context {
namespace framework {

enum class ModuleState {
    Created = 0,
    Inited,
    Started,
    Stopped,
    Fini
};

}  // namespace framework
}  // namespace module_context
