#pragma once

#include "core/api/modules/ithread_pool.h"
#include "core/framework/module_base.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mc {

/**
 * @brief 基于固定线程数的任务线程池模块，支持立即任务与延时任务。
 */
class ThreadPoolModule : public ModuleBase, public IThreadPool
{
public:
    ThreadPoolModule();
    ~ThreadPoolModule() override;

    std::string moduleName() const override;
    std::string moduleVersion() const override;
    StringList dependencies() const override;

    void post(const std::function<void()>& fn) override;
    void postDelayed(int ms, const std::function<void()>& fn) override;
    int activeThreadCount() const override;
    int maxThreadCount() const override;
    void setMaxThreadCount(int n) override;

protected:
    void onInit() override;
    void onStart() override;
    void onStop() override;
    void onFini() override;

private:
    struct DelayedTask
    {
        std::uint64_t dueMs;
        std::uint64_t seq;
        std::function<void()> fn;
    };

    struct DelayedTaskLess
    {
        bool operator()(const DelayedTask& left, const DelayedTask& right) const
        {
            if (left.dueMs == right.dueMs) {
                return left.seq > right.seq;
            }
            return left.dueMs > right.dueMs;
        }
    };

    void startWorkers();
    void stopWorkers();
    void workerLoop();

    void startDelayThread();
    void stopDelayThread();
    void delayLoop();

    void enqueueImmediate(const std::function<void()>& fn);
    static std::uint64_t nowMs();

private:
    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::condition_variable idleCv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool acceptingTasks_;
    bool stoppingWorkers_;
    std::size_t activeCount_;

    mutable std::mutex delayMutex_;
    std::condition_variable delayCv_;
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, DelayedTaskLess> delayedTasks_;
    std::thread delayThread_;
    bool delayStopRequested_;
    bool delayAccepting_;
    std::uint64_t delaySeq_;

    int maxThreadCount_;
};

} // namespace mc
