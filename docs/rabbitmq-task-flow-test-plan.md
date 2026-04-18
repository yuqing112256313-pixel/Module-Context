# RabbitMQ 主从拟真测试计划与实施

## 1. 目标

为 `rabbitmq_bus` 通信模块搭建一套可重复、可隔离、可清理的拟真测试环境，验证以下完整链路：

1. 主机程序写入图片文件
2. 主机向任务队列发布任务（`task_id + image_path`）
3. 从机消费任务并读取图片
4. 从机模拟处理并生成输出文件与报告
5. 从机向结果队列发布处理结果
6. 主机消费结果并完成闭环验证

## 2. 设计原则

### 2.1 环境隔离

- RabbitMQ 通过专用 `docker-compose.yml` 启动
- 使用独立 vhost: `mc_integration`
- 使用独立账号：
  - `mc_master`
  - `mc_worker`
- 使用独立持久卷，默认保留环境，按需通过单独清理脚本回收
- 测试运行时目录放在构建目录下，便于复现、查看日志和按需删除

### 2.2 拟真但可控

- 启动真实 RabbitMQ 服务，而不是 mock
- 主从程序分别使用各自账号登录
- 预先创建交换机、队列、绑定与权限
- 主从程序只拥有自己的最小读写权限
- 主从程序不负责创建完整 RabbitMQ 拓扑，交换机/队列/绑定由 bootstrap 统一创建
- 由于当前 `rabbitmq_bus` 模块会校验并被动声明自己依赖的交换机/队列，主从账号仍保留这些资源的最小 `configure` 权限

### 2.3 便于后续调整

- RabbitMQ 环境、账号初始化、主从程序、执行脚本分离
- 路由键、交换机、vhost、账号都集中在脚本和文档里，后续改起来集中
- 运行脚本默认保留环境，方便复现、查看日志、增加打印后再次运行
- 清理由单独入口负责，避免误删现场
- 为避免保留环境后出现旧消息污染，每次 bootstrap 会清空任务队列和结果队列

## 3. 测试拓扑

```text
主机程序(mc_rabbitmq_task_master_sim)
  -> exchange: mc.task.exchange
  -> routing_key: task.dispatch
  -> queue: mc.task.queue

从机程序(mc_rabbitmq_task_worker_sim)
  <- queue: mc.task.queue
  -> exchange: mc.result.exchange
  -> routing_key: result.ready
  -> queue: mc.result.queue

主机程序(mc_rabbitmq_task_master_sim)
  <- queue: mc.result.queue
```

## 4. 权限模型

### 主机账号 `mc_master`

- write: `mc.task.exchange`
- read: `mc.result.queue`
- configure: `mc.task.exchange`、`mc.result.queue`

### 从机账号 `mc_worker`

- read: `mc.task.queue`
- write: `mc.result.exchange`
- configure: `mc.task.queue`、`mc.result.exchange`

### 管理账号 `mc_admin`

- 用于 bootstrap 阶段创建：
  - vhost
  - 账号
  - 交换机
  - 队列
  - 绑定
  - 权限

## 5. 目录与文件

新增实现如下：

```text
tests/integration/rabbitmq_task_flow/
├── common.h
├── common.cpp
├── master_sim.cpp
├── worker_sim.cpp
├── docker-compose.yml
├── bootstrap_rabbitmq.sh
├── run_task_flow_test.sh
├── show_task_flow_status.sh
└── cleanup_task_flow_env.sh
```

## 6. 实施内容

### 6.1 主机模拟程序

主机程序职责：

- 连接 RabbitMQ（账号：`mc_master`）
- 注册结果消费者 `result_consumer`
- 在共享目录生成一张样例 PPM 图片
- 发布任务消息，消息体仅包含：
  - `task_id`
  - `image_path`
- 等待结果消息返回并校验结果状态

### 6.2 从机模拟程序

从机程序职责：

- 连接 RabbitMQ（账号：`mc_worker`）
- 注册任务消费者 `task_consumer`
- 读取任务中的图片路径并加载图片内容
- 模拟处理（延时 + 复制输出文件）
- 输出：
  - 处理后的图片副本
  - 结果报告文本
- 发布结果消息返回主机

### 6.3 RabbitMQ bootstrap 脚本

负责：

- 等待管理 API 就绪
- 创建 `mc_integration` vhost
- 创建 `mc_master` / `mc_worker`
- 声明任务交换机、结果交换机、队列、绑定
- 设置最小权限

### 6.4 一键运行脚本

负责：

1. 启动 docker compose
2. bootstrap RabbitMQ
3. 创建或重建运行时目录
4. 启动从机进程
5. 启动主机进程
6. 校验结果文件与日志
7. 默认保留现场，输出查看与清理入口

### 6.5 状态查看脚本

`show_task_flow_status.sh` 用于查看：

- compose 服务状态
- runtime 目录内容
- `master.log` / `worker.log` 尾部日志
- 最新结果报告路径

### 6.6 清理脚本

`cleanup_task_flow_env.sh` 用于：

- 停止并删除 RabbitMQ compose 环境
- 删除卷和网络
- 默认保留 runtime 目录
- 需要时通过 `MC_REMOVE_RUNTIME=1` 一并删除本地产物

## 7. 构建与运行建议

### 构建目标

建议新增两个测试执行程序：

- `mc_rabbitmq_task_master_sim`
- `mc_rabbitmq_task_worker_sim`

### 手工执行流程

前置条件：

- 本机可用 `docker compose` 或 `docker-compose`
- 或通过环境变量 `MC_COMPOSE_BIN` 指定兼容的 compose 命令

```bash
cmake -S . -B build -DMC_BUILD_TESTS=ON
cmake --build build -j

bash tests/integration/rabbitmq_task_flow/run_task_flow_test.sh \
  build/tests/mc_rabbitmq_task_master_sim \
  build/tests/mc_rabbitmq_task_worker_sim \
  build/tests/rabbitmq_task_flow_runtime
```

默认行为：

- 保留 RabbitMQ 容器、网络、卷
- 保留 runtime 目录和主从日志
- 默认生成唯一 `task_id`，避免不同运行批次互相混淆
- bootstrap 时清空任务队列与结果队列，避免旧消息污染当前复现
- 输出后续查看和清理命令

查看现场：

```bash
bash tests/integration/rabbitmq_task_flow/show_task_flow_status.sh \
  build/tests/rabbitmq_task_flow_runtime
```

清理环境：

```bash
bash tests/integration/rabbitmq_task_flow/cleanup_task_flow_env.sh \
  build/tests/rabbitmq_task_flow_runtime
```

如果连 runtime 目录也一起删：

```bash
MC_REMOVE_RUNTIME=1 \
  bash tests/integration/rabbitmq_task_flow/cleanup_task_flow_env.sh \
  build/tests/rabbitmq_task_flow_runtime
```

如果想临时恢复“跑完自动清理”行为：

```bash
MC_KEEP_ENV=0 bash tests/integration/rabbitmq_task_flow/run_task_flow_test.sh \
  build/tests/mc_rabbitmq_task_master_sim \
  build/tests/mc_rabbitmq_task_worker_sim \
  build/tests/rabbitmq_task_flow_runtime
```

## 8. 当前实现价值

这套方案已经覆盖了：

- 真实 RabbitMQ 服务
- 独立账号登录
- 任务队列与结果队列闭环
- 主从两个独立程序
- 文件读写与结果回传
- 隔离、清理、重复执行能力

也就是说，它已经不只是“单模块单接口测试”，而是更接近真实部署形态的端到端联调环境。

## 9. 后续可继续扩展的方向

1. 把共享图片目录替换成网络共享存储/NAS/对象存储挂载
2. 增加批量任务、多从机并发消费测试
3. 增加失败注入，例如：
   - 图片不存在
   - 结果发布失败
   - RabbitMQ 重启/断链重连
4. 增加性能指标采集：
   - 投递耗时
   - 消费耗时
   - 端到端总耗时
5. 接入 CTest 或 CI 中的条件化集成测试流水线

## 10. 结论

这套测试环境的核心价值在于：

- **足够真实**，因为用了真实 RabbitMQ 和真实账号权限
- **足够隔离**，因为资源、账号、vhost、运行目录都独立
- **足够容易清理**，因为 compose 和 runtime 都是一次性资产
- **足够容易演进**，因为主从程序、环境脚本、权限拓扑都已经拆开

对于后续通信模块的功能验证、回归测试、故障注入和权限策略收敛，这会是一个很合适的基础底座。

## 11. 当前实施状态

截至本次实施：

- 主从模拟程序已编译通过
- 隔离环境脚本、bootstrap 脚本、运行脚本、状态脚本、清理脚本已落地
- 真实 RabbitMQ 主从闭环已经跑通
- 当前默认策略已调整为“保留现场，不自动删环境”

这意味着这套环境已经不仅可运行，而且适合你后续直接复现、看日志、加打印、再跑一次。
