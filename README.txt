Module-Context
==============

一个轻量级 C++ 模块化运行时示例，包含：
- 核心框架（`Context`、`ModuleManager`、`ModuleBase`）
- 动态库加载（`SharedLibrary`）
- 示例模块：线程池（`module_threadpool`）
- 示例程序：`module_context_demo`

主要改动（本次修复）
--------------------
1. 统一并修复了模块接口：
   - `IModule` 现在包含模块元信息（名称、版本、依赖）及 `init(IContext&)` 生命周期。
2. 修复管理器与上下文生命周期参数传递：
   - `IModuleManager::init` 改为 `init(IContext&)`，确保模块初始化时能拿到上下文。
3. 修复线程池模块与基类接口不一致问题：
   - 线程池模块改为通过 `ModuleBase` 的 `onInit/onStart/onStop/onFini` 扩展生命周期。
4. 去除冗余/错误调用：
   - 移除了不存在的 API 调用（如 `destroy()` / `loadModule()` 误用）。
5. 修复工厂宏使用错误和格式问题。
6. 补充关键注释，提升可维护性。

构建
----
```bash
cmake -S . -B build
cmake --build build -j
```

运行示例
--------
```bash
./build/module_context_demo
```

若希望显式指定模块动态库路径：
```bash
./build/module_context_demo /absolute/path/to/libmodule_threadpool.so
```

项目结构
--------
- `core/api/framework/`：框架接口层
- `core/framework/`：框架实现层
- `core/api/modules/`：模块接口定义
- `core/modules/threadpool/`：线程池模块实现
- `app/`：演示程序

后续建议
--------
- 在 `Context::loadModules` 的基础上增加配置示例文件。
- 为生命周期状态流转与线程池并发场景添加单元测试。
- 将日志输出替换为统一日志组件（当前以 `std::cerr` 为主）。
