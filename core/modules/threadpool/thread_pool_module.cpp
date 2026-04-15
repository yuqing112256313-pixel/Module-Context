#include "core/modules/threadpool/thread_pool_module.h"

#include "core/api/framework/exception.h"
#include "core/api/framework/module_factory.h"

#include <chrono>
#include <exception>
#include <iostream>

namespace mc {

namespace {

int defaultThreadCount()
{
    const unsigned int hc = std::thread::hardware_concurrency();
    return hc > 0 ? static_cast<int>(hc) : 4;
}

} // namespace

ThreadPoolModule::ThreadPoolModule()
    : acceptingTasks_(false)
    , stoppingWorkers_(false)
    , activeCount_(0)
    , delayStopRequested_(false)
    , delayAccepting_(false)
    , delaySeq_(0)
    , maxThreadCount_(defaultThreadCount())
{
}

ThreadPoolModule::~ThreadPoolModule() = default;

std::string ThreadPoolModule::moduleName() const
{
    return "ThreadPool";
}

std::string ThreadPoolModule::moduleVersion() const
{
    return "2.0.0";
}

StringList ThreadPoolModule::dependencies() const
{
    return StringList();
}

void ThreadPoolModule::onInit()
{
}

void ThreadPoolModule::startWorkers()
{
    workers_.clear();
    workers_.reserve(static_cast<std::size_t>(maxThreadCount_));
    for (int i = 0; i < maxThreadCount_; ++i) {
        workers_.push_back(std::thread(&ThreadPoolModule::workerLoop, this));
    }
}

void ThreadPoolModule::startDelayThread()
{
    delayThread_ = std::thread(&ThreadPoolModule::delayLoop, this);
}

void ThreadPoolModule::onStart()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        acceptingTasks_ = true;
        stoppingWorkers_ = false;
        activeCount_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(delayMutex_);
        delayStopRequested_ = false;
        delayAccepting_ = true;
    }

    startWorkers();
    startDelayThread();
}

void ThreadPoolModule::stopDelayThread()
{
    {
        std::lock_guard<std::mutex> lock(delayMutex_);
        delayStopRequested_ = true;
        delayAccepting_ = false;
    }
    delayCv_.notify_all();

    if (delayThread_.joinable()) {
        delayThread_.join();
    }

    std::priority_queue<DelayedTask, std::vector<DelayedTask>, DelayedTaskLess> empty;
    {
        std::lock_guard<std::mutex> lock(delayMutex_);
        delayedTasks_.swap(empty);
    }
}

void ThreadPoolModule::stopWorkers()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        acceptingTasks_ = false;
        stoppingWorkers_ = true;
    }
    queueCv_.notify_all();

    for (std::size_t i = 0; i < workers_.size(); ++i) {
        if (workers_[i].joinable()) {
            workers_[i].join();
        }
    }
    workers_.clear();

    std::queue<std::function<void()>> empty;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        tasks_.swap(empty);
        activeCount_ = 0;
    }
    idleCv_.notify_all();
}

void ThreadPoolModule::onStop()
{
    stopDelayThread();
    stopWorkers();
}

void ThreadPoolModule::onFini()
{
    stopDelayThread();
    stopWorkers();
}

void ThreadPoolModule::enqueueImmediate(const std::function<void()>& fn)
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!acceptingTasks_) {
        throw InvalidStateError("thread pool is not accepting tasks");
    }
    tasks_.push(fn);
    queueCv_.notify_one();
}

void ThreadPoolModule::post(const std::function<void()>& fn)
{
    if (!fn) {
        throw InvalidArgumentError("post failed: empty function");
    }
    if (state() != ModuleState::Started) {
        throw InvalidStateError("thread pool is not started");
    }
    enqueueImmediate(fn);
}

void ThreadPoolModule::postDelayed(int ms, const std::function<void()>& fn)
{
    if (!fn) {
        throw InvalidArgumentError("postDelayed failed: empty function");
    }
    if (ms < 0) {
        throw InvalidArgumentError("postDelayed failed: negative delay");
    }
    if (state() != ModuleState::Started) {
        throw InvalidStateError("thread pool is not started");
    }

    DelayedTask task;
    task.dueMs = nowMs() + static_cast<std::uint64_t>(ms);
    task.fn = fn;

    {
        std::lock_guard<std::mutex> lock(delayMutex_);
        if (!delayAccepting_) {
            throw InvalidStateError("delay queue is not accepting tasks");
        }
        task.seq = ++delaySeq_;
        delayedTasks_.push(task);
    }
    delayCv_.notify_all();
}

int ThreadPoolModule::activeThreadCount() const
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    return static_cast<int>(activeCount_);
}

int ThreadPoolModule::maxThreadCount() const
{
    return maxThreadCount_;
}

void ThreadPoolModule::setMaxThreadCount(int n)
{
    if (n <= 0) {
        throw InvalidArgumentError("max thread count must be positive");
    }
    if (state() == ModuleState::Started) {
        throw InvalidStateError("cannot change max thread count while started");
    }
    maxThreadCount_ = n;
}

void ThreadPoolModule::workerLoop()
{
    while (true) {
        std::function<void()> fn;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this]() {
                return stoppingWorkers_ || !tasks_.empty();
            });

            if (tasks_.empty()) {
                if (stoppingWorkers_) {
                    return;
                }
                continue;
            }

            fn = tasks_.front();
            tasks_.pop();
            ++activeCount_;
        }

        try {
            fn();
        } catch (const std::exception& ex) {
            std::cerr << "ThreadPool task threw exception: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "ThreadPool task threw unknown exception" << std::endl;
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (activeCount_ > 0) {
                --activeCount_;
            }
            if (tasks_.empty() && activeCount_ == 0) {
                idleCv_.notify_all();
            }
        }
    }
}

void ThreadPoolModule::delayLoop()
{
    while (true) {
        std::function<void()> fn;

        {
            std::unique_lock<std::mutex> lock(delayMutex_);
            while (!delayStopRequested_ && delayedTasks_.empty()) {
                delayCv_.wait(lock);
            }

            if (delayStopRequested_) {
                return;
            }

            const std::uint64_t now = nowMs();
            const DelayedTask next = delayedTasks_.top();
            if (next.dueMs > now) {
                const std::uint64_t waitMs = next.dueMs - now;
                delayCv_.wait_for(lock, std::chrono::milliseconds(waitMs));
                continue;
            }

            fn = next.fn;
            delayedTasks_.pop();
        }

        try {
            enqueueImmediate(fn);
        } catch (...) {
            return;
        }
    }
}

std::uint64_t ThreadPoolModule::nowMs()
{
    const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    const std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<std::uint64_t>(ms.count());
}

} // namespace mc

MC_DECLARE_MODULE_FACTORY(mc::ThreadPoolModule);
