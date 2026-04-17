# RabbitMQ Bus Access

Use `IContext::GetService<IMessageBusService>(name)` as the public lookup path
for RabbitMQ bus instances. Module identity and lifecycle access stay on
`ModuleManager()->Module<IModule>(name)`, while bus operations are queried as a
service.

```cpp
#include "core/api/framework/IContext.h"
#include "core/api/messaging/IMessageBusService.h"

module_context::framework::IContext& context =
    module_context::framework::IContext::Instance();

foundation::base::Result<module_context::messaging::IMessageBusService*> bus =
    context.GetService<module_context::messaging::IMessageBusService>("bus_primary");
if (!bus.IsOk()) {
    return;
}

module_context::messaging::ConnectionState state =
    bus.Value()->GetConnectionState();
```

`GetService<IMessageBusService>()` is also available as a convenience overload,
but it succeeds only when exactly one `IMessageBusService` instance is
registered in the current context.
