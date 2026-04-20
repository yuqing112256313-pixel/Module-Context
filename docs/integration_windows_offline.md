# Windows 离线实机 Integration 运行指引（RabbitMQ 已预装）

> 目标：在**无法联网**的 Windows 实机上，使用本地已安装 RabbitMQ 跑通 `task-flow` 与 `perf` integration。

## 1. 前置依赖（离线前准备）

请确保下列组件已在机器中安装，并记录安装位置：

- RabbitMQ Server（建议启用 management plugin）
  - 常见位置：`C:\Program Files\RabbitMQ Server\rabbitmq_server-<version>\`
- Erlang/OTP（RabbitMQ 依赖）
  - 常见位置：`C:\Program Files\Erlang OTP\`
- CMake
  - 常见位置：`C:\Program Files\CMake\bin\cmake.exe`
- C++ 编译工具链（MSVC 或 clang-cl）
  - 常见位置：Visual Studio Build Tools
- Git Bash（用于执行 `*.sh` 集成脚本）
  - 常见位置：`C:\Program Files\Git\bin\bash.exe`
- curl（用于 RabbitMQ API 初始化）
  - 常见位置：Git for Windows 自带或系统 PATH

> 无外网条件下，请提前把以上安装包放到企业内网制品库或 U 盘，并完成安装。

## 2. 仓库与运行目录建议

- 仓库根目录：例如 `D:\work\Module-Context`
- 构建目录：例如 `D:\work\Module-Context\build`
- integration 脚本目录：`tests/integration/`
- integration 运行时目录（自动生成）：
  - `build/tests/rabbitmq_task_flow_runtime`
  - `build/tests/rabbitmq_perf_runtime`

## 3. RabbitMQ 本地模式（external）

本仓库的 integration runner 已支持：

- `MC_RABBITMQ_ENV=external`：不启动 compose，直接使用本机 RabbitMQ。
- `MC_RABBITMQ_ENV=auto`：优先 compose；compose 不可用时若本机 RabbitMQ API 可达则自动 external。

Windows 离线实机建议直接设置：

```bash
export MC_RABBITMQ_ENV=external
export RABBITMQ_API_URL=http://127.0.0.1:15672/api
export RABBITMQ_ADMIN_USER=guest
export RABBITMQ_ADMIN_PASS=guest
```

## 4. 运行方式（Git Bash）

在仓库根目录执行：

```bash
bash tests/integration/run_integration_tests.sh all
```

或单独执行：

```bash
bash tests/integration/run_integration_tests.sh task-flow
bash tests/integration/run_integration_tests.sh perf
```

## 5. 常见问题

1. `curl ... /api/overview` 认证失败（401）
   - 检查 `RABBITMQ_ADMIN_USER` / `RABBITMQ_ADMIN_PASS`。
   - 检查 management plugin 是否启用。

2. 找不到测试可执行文件
   - 先执行 configure/build，或取消 `MC_SKIP_CONFIGURE` / `MC_SKIP_BUILD`。
   - Windows 下 runner 会自动尝试 `*.exe`。

3. 机器没有 Docker/Compose
   - 离线 Windows 推荐固定 `MC_RABBITMQ_ENV=external`，无需 Docker。

## 6. 在线远程打包命令（用于离线 Windows 构建）

在**可联网**机器执行下面一条命令，生成“仅代码”离线包（包含本仓库 + Foundation + AMQP 源码，不包含测试环境与构建工具）：

```bash
bash scripts/package_windows_offline_bundle.sh
```

默认产物位置：

- `dist/windows-offline/Module-Context-offline-src.zip`（若无 `zip`，则为 `tar.gz`）

将该压缩包拷贝到离线 Windows 后，构建命令示例：

```powershell
cmake -S . -B build `
  -DMC_BUILD_TESTS=ON `
  -DMC_BUILD_REAL_RABBITMQ_TESTS=ON `
  -DMC_FOUNDATION_SOURCE_DIR=.\third_party\Foundation `
  -DMC_AMQP_CPP_SOURCE_DIR=.\third_party\AMQP-CPP-CXX11
cmake --build build -j
```

> 这样离线构建阶段不会再访问外网依赖仓库。
