# Module Context Core

`module_context_project_cmake` 是一个基于 `foundation` 的轻量级模块运行时框架，
用于统一管理插件模块的加载、生命周期与查询。

## 主要能力

## 目录结构

```text
.
├── examples/
├── include/
│   └── core/api/framework/
├── src/
│   └── framework/  # 内部实现（含不导出头文件）
└── tests/
```

- 通过 `foundation::plugin::PluginLoader<IModule>` 动态加载共享库插件。
- 通过 `foundation::config::ConfigReader` 读取 JSON 模块配置。
- 对外接口统一返回 `foundation::base::Result<T>`，便于携带错误码与错误信息。
- 插件导出符号遵循约定：`GetPluginApiVersion`、`CreatePlugin`、`DestroyPlugin`。
- 生命周期管理顺序清晰：`Init -> Start -> Stop -> Fini`。

## 构建说明

`foundation` 依赖解析顺序如下：

1. 若父工程已提供 `foundation::foundation`，则直接复用；
2. 若传入 `MC_FOUNDATION_SOURCE_DIR`，则使用指定路径；
3. 回退查找常见同级目录（例如 `../foundation_0415_v1/foundation`、`../Foundation`）；
4. 若都未命中，则自动从远端仓库拉取。

构建命令：

```bash
cmake -S . -B build
cmake --build build -j
```

启用示例与测试（默认开启）：

```bash
cmake -S . -B build -DMC_BUILD_EXAMPLES=ON -DMC_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

指定本地 Foundation 路径（可选）：

```bash
cmake -S . -B build -DMC_FOUNDATION_SOURCE_DIR=/path/to/Foundation
```

## 快速使用

```cpp
#include "core/api/framework/IContext.h"
#include "foundation/base/ErrorCode.h"

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

## Examples

- `examples/basic_context_lifecycle.cpp`：最小上下文生命周期驱动示例；
- `examples/module_config.sample.json`：可直接参考的模块配置文件模板。

## Tests

- `tests/module_base_lifecycle_test.cpp`：覆盖 `ModuleBase` 的核心状态机与 Hook 调用次数；
- 构建后可通过 `ctest --test-dir build --output-on-failure` 执行。

## 模块配置格式（JSON）

推荐使用以下标准格式：

- 根节点必须是对象；
- `schema_version` 必填，当前仅支持 `1`；
- `modules` 必填，且必须是数组；
- 生命周期执行顺序与 `modules` 数组顺序一致；
- 每个模块项需包含非空字符串字段：`name`、`library_path`；
- 单个配置文件内模块名必须唯一；
- 相对 `library_path` 将按配置文件所在目录进行解析。

示例：

```json
{
  "schema_version": 1,
  "modules": [
    {
      "name": "MyModule",
      "library_path": "./plugins/MyModule.dll"
    },
    {
      "name": "OtherModule",
      "library_path": "./plugins/OtherModule.dll"
    }
  ]
}
```

## 插件导出方式

插件模块建议在实现文件中使用：

```cpp
MC_DECLARE_MODULE_FACTORY(YourModuleType)
```

该宏会导出与框架兼容的工厂函数与 API 版本符号。

## 运行时行为

- 生命周期顺序：`Init -> Start -> Stop -> Fini`；
- `Stop()` 与 `Fini()` 按模块加载逆序执行；
- `LoadModules()` 会先完整校验配置，再一次性提交加载成功的模块；
- 任意阶段遇到错误会立即返回，错误包含 `foundation` 错误码与说明信息；
- `Module(name)` 返回 `foundation::base::Result<IModule*>`，可区分“未找到”与“状态异常”。
