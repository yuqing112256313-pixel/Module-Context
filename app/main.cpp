#include "core/api/modules/ithread_pool.h"
#include "core/framework/context.h"
#include "core/api/framework/imodule_manager.h"

#include <future>
#include <iostream>

int main(int argc, char* argv[])
{
    try {
        mc::Context ctx;

        const std::string libraryPath = argc > 1 ? argv[1] : std::string(MC_THREADPOOL_MODULE_PATH);
        if (!ctx.moduleMgr()->loadModule("ThreadPool", libraryPath)) {
            std::cerr << "failed to load module from: " << libraryPath << std::endl;
            return 1;
        }

        ctx.init();
        ctx.start();

        std::promise<void> done;
        std::future<void> future = done.get_future();

        mc::IThreadPool* pool = ctx.moduleMgr()->moduleAs<mc::IThreadPool>("ThreadPool");
        if (!pool) {
            std::cerr << "thread pool module cast failed" << std::endl;
            return 1;
        }

        pool->post([&done]() {
            std::cout << "hello from pure c++ thread pool" << std::endl;
            done.set_value();
        });

        future.wait();
        ctx.stop();
        ctx.fini();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "demo failed: " << ex.what() << std::endl;
        return 1;
    }
}
