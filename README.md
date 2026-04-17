# Module Context Core

`module_context_project_cmake` 是一个基于 `foundation` 的轻量级模块运行时框架，用于统一管理模块装载、生命周期驱动与服务接口查询。

## 项目定位

- 面向外部使用者公开的契约只有 `include/module_context/**` 下的运行时 API。
- 外部应用通过 `IContext` 和 `IModuleManager` 管理模块，通过 `GetService()` 获取服务接口。
- 插件工厂辅助宏、具体模块实现类、框架内部基类属于仓库内部实现，不承诺对外兼容。

## 非目标

- 不将插件开发辅助头视为公开 SDK。
- 不保证具体模块实现类可被外部工程直接依赖。
- 不把内部目录结构作为公开契约的一部分。

## 公开目录与边界

公开头文件：

- `include/module_context/framework/Export.h`
- `include/module_context/framework/IContext.h`
- `include/module_context/framework/IModule.h`
- `include/module_context/framework/IModuleManager.h`
- `include/module_context/framework/ModuleState.h`
- `include/module_context/messaging/IMessageBusService.h`
- `include/module_context/messaging/Types.h`

非公开内容：

- 框架内部基类与内部服务注册实现
- 插件工厂辅助宏与插件装载细节
- 具体模块实现类与模块内部共享状态

## 目录结构

```text
.
|-- include/
|   `-- module_context/
|       |-- framework/
|       `-- messaging/
|-- modules/
|   `-- rabbitmq_bus/
|-- examples/
|-- tests/
`-- docs/
```

## 构建说明

`foundation` 依赖解析顺序如下：

1. 若父工程已提供 `foundation::foundation`，则直接复用。
2. 若传入 `MC_FOUNDATION_SOURCE_DIR`，则使用指定路径。
3. 否则尝试常见同级目录。
4. 若都未命中，则自动从远端仓库拉取。

RabbitMQ 模块依赖 `AMQP-CPP-CXX11`，解析顺序与 `foundation` 类似，可通过 `MC_AMQP_CPP_SOURCE_DIR` 指定本地路径。

基础构建命令：

```bash
cmake -S . -B build
cmake --build build -j
```

启用示例与测试：

```bash
cmake -S . -B build -DMC_BUILD_EXAMPLES=ON -DMC_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

指定本地依赖源码：

```bash
cmake -S . -B build ^
  -DMC_FOUNDATION_SOURCE_DIR=/path/to/Foundation ^
  -DMC_AMQP_CPP_SOURCE_DIR=/path/to/AMQP-CPP-CXX11
```

默认远端依赖仓库：

- Foundation: `https://github.com/yuqing112256313-pixel/Foundation.git`
- AMQP-CPP-CXX11: `https://github.com/yuqing112256313-pixel/AMQP-CPP-CXX11.git`

## 快速开始

以下示例展示“加载模块配置 -> 启动上下文 -> 获取消息总线服务”的最小流程：

```cpp
#include "module_context/framework/IContext.h"
#include "module_context/framework/IModuleManager.h"
#include "module_context/messaging/IMessageBusService.h"

int main() {
    module_context::framework::IContext& context =
        module_context::framework::IContext::Instance();
    module_context::framework::IModuleManager* manager = context.ModuleManager();
    if (manager == NULL) {
        return 1;
    }

    foundation::base::Result<void> result =
        manager->LoadModules("examples/module_config.sample.json");
    if (!result.IsOk()) {
        return static_cast<int>(result.GetError());
    }

    result = context.Init();
    if (!result.IsOk()) {
        return static_cast<int>(result.GetError());
    }

    result = context.Start();
    if (!result.IsOk()) {
        return static_cast<int>(result.GetError());
    }

    foundation::base::Result<module_context::messaging::IMessageBusService*> bus =
        context.GetService<module_context::messaging::IMessageBusService>("bus_primary");
    if (!bus.IsOk()) {
        return static_cast<int>(bus.GetError());
    }

    result = context.Stop();
    if (!result.IsOk()) {
        return static_cast<int>(result.GetError());
    }

    result = context.Fini();
    return result.IsOk() ? 0 : static_cast<int>(result.GetError());
}
```

如果当前上下文中某种服务接口只注册了一个实例，也可以使用无参重载：

```cpp
foundation::base::Result<module_context::messaging::IMessageBusService*> bus =
    context.GetService<module_context::messaging::IMessageBusService>();
```

该重载仅在“恰好一个实例”时成功。

## 模块配置格式

推荐使用如下 JSON 结构：

- 根节点必须是对象。
- `schema_version` 必填，当前仅支持 `2`。
- `modules` 必填，且必须是数组。
- 生命周期执行顺序与 `modules` 数组顺序一致。
- 每个模块项必须包含非空字符串字段：`name`、`type`、`library_path`。
- `config` 为可选对象，用于传递模块私有配置。

示例：

```json
{
  "schema_version": 2,
  "modules": [
    {
      "name": "bus_primary",
      "type": "rabbitmq_bus",
      "library_path": "./plugins/rabbitmq_bus.dll",
      "config": {}
    },
    {
      "name": "bus_backup",
      "type": "rabbitmq_bus",
      "library_path": "./plugins/rabbitmq_bus.dll",
      "config": {}
    }
  ]
}
```

关键语义：

- `name` 表示模块实例名，在单个配置文件内必须唯一。
- `type` 表示模块类型名，用于和模块运行时上报的类型名校验。
- 相对 `library_path` 会按配置文件所在目录解析。
- `config` 会原样保存，供模块在初始化阶段读取。

## 生命周期与查询规则

- 模块生命周期顺序为：`Init -> Start -> Stop -> Fini`。
- `Init()` 与 `Start()` 按加载顺序执行，便于满足前置依赖。
- `Stop()` 与 `Fini()` 按逆序执行，符合“后启动先停止”的资源回收顺序。
- `IModuleManager::Module<T>(name)` 用于查询模块实例。
- `IContext::GetService<T>(name)` 用于查询服务接口。
- `IContext::GetService<T>()` 仅适用于当前上下文中该服务接口只有一个实例的情况。

## 内置服务与模块

当前仓库内置 `rabbitmq_bus` 模块：

- 模块类型名：`rabbitmq_bus`
- 对外服务接口：`module_context::messaging::IMessageBusService`
- 服务查询方式：`IContext::GetService<IMessageBusService>(instance_name)`

该服务支持：

- 发布消息
- 注册/注销消费者处理回调
- 声明交换机、队列和绑定
- 查询连接状态

更多架构说明见 [docs/architecture.md](docs/architecture.md)。

## 示例与测试

- `examples/basic_context_lifecycle.cpp`：最小上下文生命周期示例。
- `examples/module_config.sample.json`：模块配置示例。
- `tests/`：覆盖生命周期、配置解析、插件集成和 RabbitMQ 模块行为。
