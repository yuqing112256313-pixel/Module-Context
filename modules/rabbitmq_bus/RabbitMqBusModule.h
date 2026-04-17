#pragma once

#include "core/api/messaging/IMessageBusService.h"
#include "framework/ModuleBase.h"

#include "foundation/base/Result.h"

#include <memory>
#include <string>

namespace module_context {
namespace messaging {

class RabbitMqBusModule final
    : public module_context::framework::ModuleBase,
      public IMessageBusService {
public:
    RabbitMqBusModule();
    ~RabbitMqBusModule() override;

    std::string ModuleName() const override;
    std::string ModuleVersion() const override;

    foundation::base::Result<void> Publish(
        const PublishRequest& request) override;
    foundation::base::Result<void> RegisterConsumerHandler(
        const std::string& consumer_name,
        MessageHandler handler) override;
    foundation::base::Result<void> UnregisterConsumerHandler(
        const std::string& consumer_name) override;
    foundation::base::Result<void> DeclareExchange(
        const ExchangeSpec& spec) override;
    foundation::base::Result<void> DeclareQueue(
        const QueueSpec& spec) override;
    foundation::base::Result<void> BindQueue(
        const BindingSpec& spec) override;
    ConnectionState GetConnectionState() const override;

protected:
    foundation::base::Result<void> OnInit() override;
    foundation::base::Result<void> OnStart() override;
    foundation::base::Result<void> OnStop() override;
    foundation::base::Result<void> OnFini() override;

private:
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace messaging
}  // namespace module_context
