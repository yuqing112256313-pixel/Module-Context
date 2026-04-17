#pragma once

#include "core/api/messaging/IMessageBusService.h"

#include "foundation/base/Result.h"
#include "foundation/concurrent/ThreadPool.h"
#include "foundation/config/ConfigValue.h"

#include <map>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace module_context {
namespace messaging {

struct ReconnectConfig {
    bool enabled;
    int initial_delay_ms;
    int max_delay_ms;

    ReconnectConfig()
        : enabled(true),
          initial_delay_ms(1000),
          max_delay_ms(30000) {
    }
};

struct ConnectionConfig {
    std::string uri;
    std::uint16_t heartbeat_seconds;
    int socket_timeout_ms;
    ReconnectConfig reconnect;

    ConnectionConfig()
        : uri(),
          heartbeat_seconds(30),
          socket_timeout_ms(100) {
    }
};

struct PublisherSpec {
    std::string name;
    std::string exchange;
    std::string routing_key;
    std::string content_type;
    foundation::config::ConfigValue::Object headers;
    bool persistent;

    PublisherSpec()
        : name(),
          exchange(),
          routing_key(),
          content_type(),
          headers(),
          persistent(false) {
    }
};

struct RabbitMqBusConfig {
    ConnectionConfig connection;
    std::size_t worker_thread_count;
    std::vector<ExchangeSpec> exchanges;
    std::vector<QueueSpec> queues;
    std::vector<BindingSpec> bindings;
    std::vector<PublisherSpec> publishers;
    std::vector<ConsumerSpec> consumers;

    RabbitMqBusConfig()
        : connection(),
          worker_thread_count(4),
          exchanges(),
          queues(),
          bindings(),
          publishers(),
          consumers() {
    }
};

class RabbitMqConnectionDriver;

struct RabbitMqBusSharedState {
    RabbitMqBusSharedState()
        : mutex(),
          config(new RabbitMqBusConfig()),
          handlers(),
          worker_pool(),
          driver() {
    }

    std::mutex mutex;
    std::shared_ptr<RabbitMqBusConfig> config;
    std::map<std::string, MessageHandler> handlers;
    std::shared_ptr<foundation::concurrent::ThreadPool> worker_pool;
    std::shared_ptr<RabbitMqConnectionDriver> driver;
};

foundation::base::Result<void> PublishWithDriver(
    const std::shared_ptr<RabbitMqConnectionDriver>& driver,
    const PublishRequest& request);
foundation::base::Result<void> DeclareExchangeWithDriver(
    const std::shared_ptr<RabbitMqConnectionDriver>& driver,
    const ExchangeSpec& spec);
foundation::base::Result<void> DeclareQueueWithDriver(
    const std::shared_ptr<RabbitMqConnectionDriver>& driver,
    const QueueSpec& spec);
foundation::base::Result<void> BindQueueWithDriver(
    const std::shared_ptr<RabbitMqConnectionDriver>& driver,
    const BindingSpec& spec);
ConnectionState GetConnectionStateFromDriver(
    const std::shared_ptr<RabbitMqConnectionDriver>& driver);

}  // namespace messaging
}  // namespace module_context
