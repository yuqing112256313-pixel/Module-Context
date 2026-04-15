#include "core/api/framework/context_access.h"
#include "core/framework/context.h"

namespace mc {

IContext& context()
{
    static Context g_context;
    return g_context;
}

} // namespace mc