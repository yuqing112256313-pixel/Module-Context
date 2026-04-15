Module-Context（纯 C++ 重构版）
=================================

适用环境：
- C++11
- CMake 3.16+
- 无 Qt 依赖
- 支持 Linux / macOS / Windows 的显式动态加载

本次重构重点：
1. 全面改为纯 C++ 标准库实现；
2. core/framework 目录改为扁平展开，不再分 context/module/modulemgr 子层；
3. 每个模块单独编译为动态库，通过模块工厂符号显式加载；
4. 所有原先返回指针的访问接口改为返回引用，查找失败直接抛异常；
5. 删除错误码机制，统一改为异常风格；
6. 删除 IContext::isInited / isStarted；
7. 删除中间态 Initing / Starting / Stopping / Destroying；
8. 删除内建模块自动注册，所有模块由上层显式加载；
9. 重复注册直接依赖 map 容器判重，不保留冗余判重逻辑；
10. 避免锁滥用：框架层只保留必要互斥，模块基类不再对生命周期状态上锁。

目录概览：
- core/api/framework/   对外接口、异常、状态、模块工厂声明
- core/api/modules/     模块接口
- core/framework/       框架实现（扁平展开）
- core/modules/         各模块独立实现，并编译为动态库
- app/                  最小示例

动态模块约定：
每个模块动态库需导出以下符号：
- mcCreateModule
- mcDestroyModule
- mcGetModuleName

构建方式：
1. cmake -S . -B build
2. cmake --build build
3. 运行示例程序（可选）

说明：
- app 示例默认加载 CMake 构建出来的 module_threadpool 动态库；
- 若需加载自定义模块，可仿照 thread_pool_module.cpp 的导出方式实现。
