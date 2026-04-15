#include "core/api/modules/ithread_pool.h"
#include "core/framework/context.h"

#include <future>
#include <iostream>

int main(int argc, char* argv[])
{
    try {
        mc::Context ctx;

        const std::string libraryPath = argc > 1 ? argv[1] : std::string(MC_THREADPOOL_MODULE_PATH);
        ctx.loadModule(libraryPath);
        ctx.init();
        ctx.start();

        std::promise<void> done;
        std::future<void> future = done.get_future();

        ctx.module<mc::IThreadPool>().post([&done]() {
            std::cout << "hello from pure c++ thread pool" << std::endl;
            done.set_value();
        });

        future.wait();
        ctx.stop();
        ctx.destroy();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "demo failed: " << ex.what() << std::endl;
        return 1;
    }
}
