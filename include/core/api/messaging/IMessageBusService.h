#pragma once

#include "core/api/framework/Export.h"
#include "core/api/messaging/Types.h"

#include "foundation/base/Result.h"

#include <string>

namespace module_context {
namespace messaging {

class MC_FRAMEWORK_API IMessageBusService {
public:
    virtual ~IMessageBusService() {}

    virtual foundation::base::Result<void> Publish(
        const PublishRequest& request) = 0;
    virtual foundation::base::Result<void> RegisterConsumerHandler(
        const std::string& consumer_name,
        MessageHandler handler) = 0;
    virtual foundation::base::Result<void> UnregisterConsumerHandler(
        const std::string& consumer_name) = 0;
    virtual foundation::base::Result<void> DeclareExchange(
        const ExchangeSpec& spec) = 0;
    virtual foundation::base::Result<void> DeclareQueue(
        const QueueSpec& spec) = 0;
    virtual foundation::base::Result<void> BindQueue(
        const BindingSpec& spec) = 0;
    virtual ConnectionState GetConnectionState() const = 0;
};

}  // namespace messaging
}  // namespace module_context
