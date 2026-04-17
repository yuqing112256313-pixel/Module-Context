#pragma once

#include "module_context/framework/Export.h"

#include "foundation/config/ConfigValue.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace module_context {
namespace messaging {

/**
 * @brief 交换机类型。
 */
enum class ExchangeType {
    Direct = 0,  ///< 直连交换机，按路由键精确路由。
    Fanout = 1,  ///< 广播交换机，消息投递给所有绑定队列。
    Topic = 2,   ///< 主题交换机，按通配路由规则投递。
    Headers = 3  ///< 头交换机，按消息头匹配规则投递。
};

/**
 * @brief 消费回调处理完成后的确认动作。
 */
enum class ConsumeAction {
    Ack = 0,     ///< 确认消费成功。
    Requeue = 1, ///< 重新入队，等待后续再次消费。
    Reject = 2   ///< 拒绝消息且不重新入队。
};

/**
 * @brief 消息总线连接状态。
 */
enum class ConnectionState {
    Created = 0,      ///< 服务对象已创建，尚未启动连接。
    Connecting = 1,   ///< 正在建立连接。
    Connected = 2,    ///< 已连接并可收发消息。
    Reconnecting = 3, ///< 连接中断后正在重连。
    Stopped = 4,      ///< 已主动停止。
    Error = 5         ///< 出现错误且当前不可用。
};

/**
 * @brief 交换机声明参数。
 */
struct MC_FRAMEWORK_API ExchangeSpec {
    std::string name;                               ///< 交换机名称。
    ExchangeType type;                             ///< 交换机类型。
    bool durable;                                  ///< 是否持久化。
    bool auto_delete;                              ///< 是否在无绑定后自动删除。
    bool passive;                                  ///< 是否仅校验存在性而不主动创建。
    bool internal;                                 ///< 是否禁止客户端直接发布消息。
    foundation::config::ConfigValue::Object arguments;  ///< 扩展参数。

    ExchangeSpec()
        : name(),
          type(ExchangeType::Direct),
          durable(false),
          auto_delete(false),
          passive(false),
          internal(false),
          arguments() {
    }
};

/**
 * @brief 队列声明参数。
 */
struct MC_FRAMEWORK_API QueueSpec {
    std::string name;                               ///< 队列名称。
    bool durable;                                  ///< 是否持久化。
    bool auto_delete;                              ///< 是否在消费者断开后自动删除。
    bool passive;                                  ///< 是否仅校验存在性而不主动创建。
    bool exclusive;                                ///< 是否为独占队列。
    foundation::config::ConfigValue::Object arguments;  ///< 扩展参数。

    QueueSpec()
        : name(),
          durable(false),
          auto_delete(false),
          passive(false),
          exclusive(false),
          arguments() {
    }
};

/**
 * @brief 队列绑定参数。
 */
struct MC_FRAMEWORK_API BindingSpec {
    std::string exchange;                           ///< 源交换机名称。
    std::string queue;                              ///< 目标队列名称。
    std::string routing_key;                        ///< 绑定使用的路由键。
    foundation::config::ConfigValue::Object arguments;  ///< 扩展参数。
};

/**
 * @brief 消费者声明参数。
 */
struct MC_FRAMEWORK_API ConsumerSpec {
    std::string name;                               ///< 消费者逻辑名称。
    std::string queue;                              ///< 要消费的队列名称。
    std::string consumer_tag;                       ///< 自定义消费者标识；为空则由消息代理生成。
    bool auto_ack;                                  ///< 是否自动确认消息。
    bool exclusive;                                 ///< 是否独占消费该队列。
    bool no_local;                                  ///< 是否忽略本连接自身发布的消息。
    std::uint16_t prefetch_count;                   ///< 预取数量。
    foundation::config::ConfigValue::Object arguments;  ///< 扩展参数。

    ConsumerSpec()
        : name(),
          queue(),
          consumer_tag(),
          auto_ack(false),
          exclusive(false),
          no_local(false),
          prefetch_count(1),
          arguments() {
    }
};

/**
 * @brief 发布消息请求。
 */
struct MC_FRAMEWORK_API PublishRequest {
    std::string exchange;                           ///< 目标交换机名称。
    std::string routing_key;                        ///< 路由键。
    std::vector<char> payload;                      ///< 消息体字节序列。
    std::string content_type;                       ///< 内容类型，例如 JSON 文本。
    std::string correlation_id;                     ///< 关联标识。
    std::string reply_to;                           ///< 响应目标。
    foundation::config::ConfigValue::Object headers;  ///< 自定义消息头。
    bool persistent;                                ///< 是否以持久消息方式发送。

    PublishRequest()
        : exchange(),
          routing_key(),
          payload(),
          content_type(),
          correlation_id(),
          reply_to(),
          headers(),
          persistent(false) {
    }
};

/**
 * @brief 收到的消息内容。
 */
struct MC_FRAMEWORK_API IncomingMessage {
    std::string consumer_name;                      ///< 触发该消息的消费者逻辑名称。
    std::string exchange;                           ///< 来源交换机名称。
    std::string routing_key;                        ///< 投递使用的路由键。
    std::vector<char> payload;                      ///< 消息体字节序列。
    std::string content_type;                       ///< 内容类型。
    std::string correlation_id;                     ///< 关联标识。
    std::string reply_to;                           ///< 响应目标。
    foundation::config::ConfigValue::Object headers;  ///< 自定义消息头。
    bool redelivered;                               ///< 是否为重复投递。

    IncomingMessage()
        : consumer_name(),
          exchange(),
          routing_key(),
          payload(),
          content_type(),
          correlation_id(),
          reply_to(),
          headers(),
          redelivered(false) {
    }
};

/**
 * @brief 消息处理回调签名。
 *
 * 回调返回值决定消息确认策略。
 */
typedef std::function<ConsumeAction(const IncomingMessage&)> MessageHandler;

}  // namespace messaging
}  // namespace module_context
