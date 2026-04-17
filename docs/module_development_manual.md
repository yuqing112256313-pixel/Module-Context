# 模块开发手册（标准版）

本文档面向组内开发人员，目标是统一“新增一个模块”的实现方式，确保模块能够被 `module_context` 运行时稳定加载、驱动生命周期，并按约定对外暴露服务。

> 适用范围：本仓库当前实现（`module_context_core` + 插件动态库）。

---

## 1. 开发前先明确的边界

在开始写模块前，先确认三件事：

1. **模块实例边界**：模块是 `IModule` 的实现，受框架统一生命周期驱动（`Init/Start/Stop/Fini`）。
2. **服务能力边界**：模块可以额外实现一个或多个服务接口，供业务通过 `IContext::GetService<T>()` 查询。
3. **配置边界**：模块私有配置来自模块配置文件中的 `config` 字段，运行时通过 `IModuleManager::ModuleConfig(name)` 获取。

参考接口：

- `IModule` 生命周期契约：`include/module_context/framework/IModule.h`
- `IContext` / `GetService` 查询契约：`include/module_context/framework/IContext.h`
- `IModuleManager` 模块装载与配置读取契约：`include/module_context/framework/IModuleManager.h`

---

## 2. 标准目录与文件建议

建议按内置 `rabbitmq_bus` 的结构组织新模块：

```text
modules/
  <your_module>/
    CMakeLists.txt
    src/
      <YourModule>.h
      <YourModule>.cpp
      internal/   # 可选，放模块私有实现
```

并在 `modules/CMakeLists.txt` 中 `add_subdirectory(<your_module>)`，纳入统一构建流程。

---

## 3. 标准实现步骤

### 步骤 1：定义模块类

推荐继承 `framework::ModuleBase`，而不是直接手写 `IModule` 全量状态机。

- 你必须实现：`ModuleType()`、`ModuleVersion()`。
- 你通常重写：`OnInit()`、`OnStart()`、`OnStop()`、`OnFini()`。

`ModuleBase` 已内置：

- 生命周期状态流转校验（非法状态切换会返回 `kInvalidState`）。
- `SetModuleName()` 只能在 `Created` 状态下注入。
- `Context()` 访问上下文能力（仅在初始化后可用）。

这也是当前 `rabbitmq_bus` 与测试模块采用的统一方式。

---

### 步骤 2：在 `OnInit()` 读取模块配置

不要在构造函数里读配置。标准做法是在 `OnInit()`：

1. 通过 `Context().ModuleManager()` 获取 `IModuleManager`。
2. 用 `ModuleName()` 作为 key 调用 `ModuleConfig(ModuleName())`。
3. 对关键字段做严格校验（类型、必填、非空）。

测试中的 `ConfigTestModule` 就是该模式的最小参考实现。

---

### 步骤 3：若模块对外提供服务，定义服务接口与 ServiceTraits

若新模块要被业务调用，请新增（或复用）服务接口，并为它提供：

- 服务接口头文件（例如 `IXXXService`）
- `ServiceTypeTraits<IXXXService>` 特化（用于 `GetService<T>` 路由 service key）

当前 `IMessageBusService` 的做法可直接参考：接口与 `ServiceTypeTraits` 放在同一公开头中。

---

### 步骤 4：让运行时识别你的服务（关键）

当前框架的服务注册是“**已知服务白名单**”机制：

- 模块装载时，`ModuleManager` 会调用 `ServiceRegistry::RegisterKnownServices`。
- `ServiceRegistry` 通过 `dynamic_cast` 判断模块是否实现某个服务接口；匹配后才登记到服务表。

这意味着：

- 如果你只实现模块，不提供服务，则无需改注册逻辑。
- 如果你新增服务接口类型，必须同步扩展 `ServiceRegistry::RegisterKnownServices` 的识别分支，否则 `GetService<T>` 查不到。

---

### 步骤 5：导出插件工厂符号

每个模块动态库都必须导出固定 C ABI 符号，建议直接在 `.cpp` 末尾使用宏：

```cpp
MC_DECLARE_MODULE_FACTORY(YourModule)
```

该宏会导出：

- `GetPluginApiVersion()`
- `CreatePlugin()`
- `DestroyPlugin()`

运行时会校验插件 API 版本（当前是 `kModulePluginApiVersion = 2`），不匹配会加载失败。

---

### 步骤 6：补齐 CMake 构建

最小清单：

1. `add_library(<module_name> SHARED ...)`
2. 包含目录至少要能访问：
   - 仓库 `include/`
   - 框架内部头（若你继承 `ModuleBase`，需要 `src/`）
3. 链接 `mc_core_framework` 与必要第三方依赖
4. 设置插件产物输出目录（便于示例/测试加载）
5. `install(TARGETS ... DESTINATION .../plugins)`

可直接仿照 `modules/rabbitmq_bus/CMakeLists.txt` 的模板。

---

### 步骤 7：配置文件接入与约束

模块配置由统一 JSON 文件驱动，关键约束：

- 根对象必须包含 `schema_version: 2`
- `modules` 必须是数组
- 每项必须有非空字符串：`name` / `type` / `library_path`
- `config`（可选）必须是对象
- `name` 在同一文件内必须唯一
- `type` 必须与模块运行时 `ModuleType()` 一致

推荐你在模块文档中明确自己的 `config` 子结构，并在 `OnInit()` 把错误信息写清楚（方便定位配置问题）。

---

## 4. 生命周期实现约定（强烈建议遵守）

1. **OnInit**：只做“配置解析、资源创建、依赖检查”，避免阻塞操作。
2. **OnStart**：启动线程/连接/消费者等运行态逻辑。
3. **OnStop**：停止运行态逻辑，保证可重复调用时尽量幂等。
4. **OnFini**：释放最终资源，回到可销毁状态。

另外要注意：`ModuleManager` 在框架层面是“前向 Init/Start、逆向 Stop/Fini”，因此模块间有依赖关系时请按加载顺序组织配置。

---

## 5. 新模块落地检查清单（可直接复用）

### 代码检查

- [ ] 新模块继承 `ModuleBase`，实现 `ModuleType/ModuleVersion`。
- [ ] 生命周期钩子错误码使用 `foundation::base::Result`，错误消息可定位。
- [ ] 如有服务接口，已添加 `ServiceTypeTraits`。
- [ ] 如有新增服务类型，已扩展 `ServiceRegistry::RegisterKnownServices`。
- [ ] 插件导出宏 `MC_DECLARE_MODULE_FACTORY(...)` 已添加。

### 构建检查

- [ ] `modules/<your_module>/CMakeLists.txt` 可独立编译。
- [ ] 顶层 `modules/CMakeLists.txt` 已纳入子目录。
- [ ] 插件产物会输出到可被配置文件引用的位置。

### 运行检查

- [ ] 配置文件中 `type` 与 `ModuleType()` 一致。
- [ ] `LoadModules -> Init -> Start -> Stop -> Fini` 全流程通过。
- [ ] `GetService<T>(module_name)`（如适用）能成功查询。

---

## 6. 参考实现索引（建议开发时并行打开）

- 生命周期基类：`src/framework/ModuleBase.h` / `src/framework/ModuleBase.cpp`
- 模块装载与生命周期编排：`src/framework/ModuleManager.cpp`
- 服务注册入口：`src/framework/ServiceRegistry.cpp`
- 插件工厂宏：`src/plugin/ModuleFactory.h`
- 内置模块样例：`modules/rabbitmq_bus/src/RabbitMqBusModule.h` + `.cpp`
- 配置读取样例（测试插件）：`tests/module_manager_config_test_plugin.cpp`

---

## 7. 推荐的最小开发流程（1 天版本）

1. 复制 `rabbitmq_bus` 的骨架（只保留模块类与 CMake 框架）。
2. 改 `ModuleType/Version` 与类名，先实现空 `OnInit/OnStart/OnStop/OnFini`。
3. 接入插件导出宏，先确保能被 `LoadModule` 成功创建。
4. 在 `OnInit` 加入配置解析，并写一个最小测试覆盖错误路径。
5. 如需对外服务，再补服务接口 + `ServiceTypeTraits` + `ServiceRegistry` 注册。
6. 跑完整生命周期测试后，再补业务逻辑。

这样能把“装载问题、配置问题、业务问题”分层隔离，排错成本最低。
