#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/IModule.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

#include <string>

namespace module_context {
namespace messaging {

class IMessageBusService;

}  // namespace messaging
namespace framework {

class IModuleManager;

template <typename T>
struct ServiceTypeTraits;

class MC_FRAMEWORK_API IContext {
public:
    static IContext& Instance();

    virtual ~IContext() {}

    virtual foundation::base::Result<void> Init() = 0;
    virtual foundation::base::Result<void> Start() = 0;
    virtual foundation::base::Result<void> Stop() = 0;
    virtual foundation::base::Result<void> Fini() = 0;

    virtual IModuleManager* ModuleManager() = 0;

    template <typename T>
    foundation::base::Result<T*> GetService(const std::string& name) {
        foundation::base::Result<IModule*> service =
            LookupServiceRaw(ServiceTypeTraits<T>::Key(), name);
        if (!service.IsOk()) {
            return foundation::base::Result<T*>(
                service.GetError(),
                service.GetMessage());
        }

        T* typed_service = dynamic_cast<T*>(service.Value());
        if (typed_service == NULL) {
            return foundation::base::Result<T*>(
                foundation::base::ErrorCode::kInvalidState,
                "Service type cast failed");
        }

        return foundation::base::Result<T*>(typed_service);
    }

    template <typename T>
    foundation::base::Result<T*> GetService() {
        foundation::base::Result<IModule*> service =
            LookupUniqueServiceRaw(ServiceTypeTraits<T>::Key());
        if (!service.IsOk()) {
            return foundation::base::Result<T*>(
                service.GetError(),
                service.GetMessage());
        }

        T* typed_service = dynamic_cast<T*>(service.Value());
        if (typed_service == NULL) {
            return foundation::base::Result<T*>(
                foundation::base::ErrorCode::kInvalidState,
                "Service type cast failed");
        }

        return foundation::base::Result<T*>(typed_service);
    }

private:
    virtual foundation::base::Result<IModule*> LookupServiceRaw(
        const char* service_key,
        const std::string& name) = 0;
    virtual foundation::base::Result<IModule*> LookupUniqueServiceRaw(
        const char* service_key) = 0;
};

template <>
struct ServiceTypeTraits<module_context::messaging::IMessageBusService> {
    static const char* Key() {
        return "module_context.messaging.IMessageBusService";
    }
};

}  // namespace framework
}  // namespace module_context
