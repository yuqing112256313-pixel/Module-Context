# Module Context Core

`module_context_project_cmake` 是一个基于 `foundation` 的轻量级模块运行时框架，
用于统一管理插件模块的加载、生命周期与查询。

## 主要能力

- 通过 `foundation::plugin::PluginLoader<IModule>` 动态加载共享库插件
- 通过 `foundation::config::ConfigReader` 读取 JSON 模块配置
- 对外统一返回 `foundation::base::Result<T>`，便于携带错误码与错误信息
- 插件导出符号遵循约定：`GetPluginApiVersion`、`CreatePlugin`、`DestroyPlugin`
- 生命周期管理顺序清晰：`Init -> Start -> Stop -> Fini`

## 目录结构

```text
.
|-- examples/
|-- include/
|   `-- core/api/framework/
|-- src/
|   `-- framework/
`-- tests/
```

## 构建说明

`foundation` 依赖解析顺序如下：

1. 若父工程已提供 `foundation::foundation`，则直接复用
2. 若传入 `MC_FOUNDATION_SOURCE_DIR`，则使用指定路径
3. 否则尝试常见同级目录
4. 若都未命中，则自动从远端仓库拉取

构建命令：

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

指定本地 Foundation 路径：

```bash
cmake -S . -B build -DMC_FOUNDATION_SOURCE_DIR=/path/to/Foundation
```

## 快速使用

```cpp
#include "core/api/framework/IContext.h"

module_context::framework::IContext& context =
    module_context::framework::IContext::Instance();

foundation::base::Result<void> result = context.Init();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}

result = context.Start();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}

result = context.Stop();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}

result = context.Fini();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}
```

## 模块配置格式

推荐使用以下 JSON 格式：

- 根节点必须是对象
- `schema_version` 必填，当前仅支持 `2`
- `modules` 必填，且必须是数组
- 生命周期执行顺序与 `modules` 数组顺序一致
- 每个模块项必须包含非空字符串字段：`name`、`type`、`library_path`
- `name` 表示实例名，必须在单个配置文件内唯一
- `type` 表示模块类型名，用于与插件运行时类型校验
- 相对 `library_path` 将按配置文件所在目录进行解析
- `config` 可选，若提供则必须是对象

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

## 运行时身份语义

- `ModuleName()` 返回实例名
- `ModuleType()` 返回类型名
- 框架在插件实例创建后、进入生命周期前调用 `SetModuleName(name)` 注入实例名
- `ModuleManager::Module<T>(name)` 与 `ModuleManager::ModuleConfig(name)` 都按实例名查询

## 插件导出方式

插件模块建议在实现文件中使用：

```cpp
MC_DECLARE_MODULE_FACTORY(YourModuleType)
```

该宏会导出与框架兼容的工厂函数与插件 API 版本符号。

## 运行时行为

- 生命周期顺序：`Init -> Start -> Stop -> Fini`
- `Stop()` 与 `Fini()` 按模块加载逆序执行
- `LoadModules()` 会先完整校验配置，再一次性提交成功加载的模块
- 任意阶段遇到错误会立即返回，错误中包含 `foundation` 错误码与说明信息
- `Module<T>(name)` 返回 `foundation::base::Result<T*>`，可区分“未找到”与“类型/状态异常”

## 示例与测试

- `examples/basic_context_lifecycle.cpp`：最小上下文生命周期示例
- `examples/module_config.sample.json`：模块配置示例
- 构建后可通过 `ctest --test-dir build --output-on-failure` 执行测试
