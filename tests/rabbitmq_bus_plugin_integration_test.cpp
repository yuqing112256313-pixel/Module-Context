#include "core/api/framework/IModuleManager.h"
#include "core/api/messaging/IMessageBusService.h"
#include "framework/ModuleManager.h"

#include "foundation/base/ErrorCode.h"

#include <iostream>
#include <string>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << std::endl;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    module_context::framework::ModuleManager manager;

    foundation::base::Result<void> load_result =
        manager.LoadModule("rabbitmq_bus", MC_TEST_RABBITMQ_BUS_PLUGIN_PATH);
    if (!Expect(load_result.IsOk(), "LoadModule should load rabbitmq_bus plugin")) {
        return 1;
    }

    foundation::base::Result<module_context::messaging::IMessageBusService*> bus =
        manager.ModuleAs<module_context::messaging::IMessageBusService>("rabbitmq_bus");
    if (!Expect(bus.IsOk(), "ModuleAs<IMessageBusService> should succeed")) {
        return 1;
    }

    if (!Expect(bus.Value() != NULL, "ModuleAs should return a non-null service")) {
        return 1;
    }

    if (!Expect(
            bus.Value()->GetConnectionState() ==
                module_context::messaging::ConnectionState::Created,
            "Freshly loaded rabbitmq_bus service should be in Created state")) {
        return 1;
    }

    foundation::base::Result<void> fini_result = manager.Fini();
    if (!Expect(fini_result.IsOk(), "Fini should unload rabbitmq_bus plugin")) {
        return 1;
    }

    std::cout << "[PASSED] rabbitmq_bus_plugin_integration_test" << std::endl;
    return 0;
}
