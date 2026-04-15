# Module Context Core

## 项目简介

这是一个轻量级 C++ 模块化框架示例，支持按需加载动态模块并统一管理生命周期。

- 支持从配置文件批量加载模块
- 支持手动加载单个模块
- 提供稳定的上下文与模块管理入口
- 适配 Windows 与 Linux 动态库加载（`LoadLibrary` / `dlopen`）

当前实现包含以下核心抽象：

- `IContext`：运行时上下文入口
- `IModuleManager`：模块管理器接口
- `IModule`：模块统一接口
- `ModuleBase`：模块基类，减少重复实现
- `SharedLibrary`：动态库封装，兼容平台差异

## 目录结构

- `core/api/framework/`
  - `icontext.h`
  - `imodule_manager.h`
  - `imodule.h`
  - `module_factory.h`
  - `module_state.h`
- `core/framework/`
  - `context.h`
  - `module_manager.h`
  - `module_base.h`
  - `shared_library.h`
  - `shared_library.cpp`
  - `module_manager.cpp`
  - `module_base.cpp`
  - `context.cpp`
- `core/modules/`
  - 示例模块实现（可选）

## 命名空间约定

框架核心命名空间为：

- `module_context::framework`

推荐写法如下：

```cpp
module_context::framework::Context context;
```

不再保留 `mc` 兼容写法，所有代码请直接使用 `module_context::framework`。

## 构建方式

```bash
cmake -S . -B build
cmake --build build -j
```

## 使用示例

```cpp
module_context::framework::Context context;
context.Init();
context.Start();
context.Stop();
context.Fini();
```

`modules.conf` 配置文件格式：

```text
# 模块名称 = 动态库路径
MyModule = /path/to/libmymodule.so
```

查询模块实例：

```cpp
auto* module = context.ModuleManager()->Module("MyModule");
```

## API 说明

- 关键调用顺序：`Init -> Start -> Stop -> Fini`
- 模块由 `ModuleManager` 创建和销毁，可通过 `Module(name)` 按名查找
- 当前实现返回布尔值 `bool` 表示加载成功或失败，后续可扩展错误码与日志上报

## 已知限制

1. `LoadModules` 遇到任意一行解析失败会直接返回 `false`，后续行不会继续加载。
2. 上下文未初始化时调用 `Init/Start/Stop/Fini` 会进入空调用保护路径，不会抛异常。
3. 反向停止与销毁采用 `reverse` 顺序，避免依赖问题，但当前实现仍可继续细化更多错误诊断。
4. 当前版本使用 `std::string` + 映射表，若模块量较大可考虑增加更细粒度错误信息与缓存策略。
