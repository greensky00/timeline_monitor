#include "timeline_monitor.h"

#include "event_awaiter.h"
#include "test_common.h"

#include <thread>

#include <stdio.h>

int hierarchical_test() {
    TestSuite::Msg mm;

    const size_t A = 5;
    const size_t B = 10;
    auto func_1 = [&]() {
        T_MONITOR_FUNC();
        for (size_t ii = 0; ii < A; ++ii) {
            T_MONITOR_BLOCK("func_1_inner");
        }
    };
    auto func_2 = [&]() -> timeline_monitor::Timeline {
        T_MONITOR_FUNC();
        for (size_t ii = 0; ii < B; ++ii) {
            func_1();
        }
        return T_MONITOR_EXPORT();
    };

    timeline_monitor::Timeline ret = func_2();
    mm << timeline_monitor::TimelineDump::toString(ret) << std::endl;

    CHK_EQ( (1 + B + A * B) * 2, ret.getElems().size() );
    CHK_EQ( 0, ret.getDepth() );

    return 0;
}

int thread_transfer_test() {
    TestSuite::Msg mm;

    EventAwaiter ea;

    std::function<void(std::shared_ptr<timeline_monitor::Timeline>, size_t)>
        func_1 = [&](std::shared_ptr<timeline_monitor::Timeline> src,
                     size_t depth)
    {
        T_MONITOR_FUNC_CUSTOM(*src);
        if (depth < 10) {
            std::thread t(func_1, src, depth + 1);
            t.detach();
        } else {
            ea.invoke();
        }
    };

    auto func_2 = [&]() -> std::shared_ptr<timeline_monitor::Timeline> {
        T_MONITOR_FUNC();
        std::shared_ptr<timeline_monitor::Timeline> ret =
            std::make_shared<timeline_monitor::Timeline>( T_MONITOR_EXPORT() );

        std::thread t(func_1, ret, 0);
        t.detach();

        ea.wait();

        return ret;
    };

    std::shared_ptr<timeline_monitor::Timeline> ret = func_2();
    mm << timeline_monitor::TimelineDump::toString(*ret) << std::endl;

    CHK_EQ( 24, ret->getElems().size() );
    CHK_EQ( 0, ret->getDepth() );

    return 0;
}

int multi_thread_test() {
    TestSuite::Msg mm;
    std::atomic<uint64_t> total_calls(0);

    const size_t A = 5;
    const size_t B = 10;
    auto func_1 = [&]() {
        T_MONITOR_FUNC();
        for (size_t ii = 0; ii < A; ++ii) {
            T_MONITOR_BLOCK("func_1_inner");
        }
    };
    auto func_2 = [&]() -> int {
        T_MONITOR_FUNC();
        for (size_t ii = 0; ii < B; ++ii) {
            func_1();
        }
        timeline_monitor::Timeline ret = T_MONITOR_EXPORT();
        CHK_EQ( (1 + B + A * B) * 2, ret.getElems().size() );
        CHK_EQ( 0, ret.getDepth() );
        return 0;
    };

    struct WorkerArgs : TestSuite::ThreadArgs {
        WorkerArgs() : seconds(1) {}
        size_t seconds;
    };
    auto t_worker = [&](TestSuite::ThreadArgs* ta) -> int {
        WorkerArgs* args = static_cast<WorkerArgs*>(ta);
        TestSuite::Timer tt(args->seconds * 1000);
        while (!tt.timeout()) {
            CHK_Z( func_2() );
            total_calls.fetch_add(1, std::memory_order_relaxed);
        }
        return 0;
    };

    const size_t NUM_THREADS = 8;
    std::vector<TestSuite::ThreadHolder> holders(NUM_THREADS);
    WorkerArgs wargs;
    wargs.seconds = 2;
    for (TestSuite::ThreadHolder& tt: holders) {
        tt.spawn(&wargs, t_worker, nullptr);
    }
    for (TestSuite::ThreadHolder& tt: holders) {
        tt.join();
        CHK_Z(tt.getResult());
    }

    mm << "total " << total_calls << " calls" << std::endl;

    return 0;
}

int export_in_the_middle_test() {
    TestSuite::Msg mm;

    timeline_monitor::Timeline ret;
    auto func_1 = [&](bool do_middle_export) {
        T_MONITOR_FUNC();
        {
            T_MONITOR_BLOCK("func_1_inner");
        }
        if (do_middle_export) {
            ret = T_MONITOR_EXPORT();
        }
    };
    auto func_2 = [&](bool do_middle_export) {
        T_MONITOR_FUNC();
        func_1(do_middle_export);
        if (!do_middle_export) {
            ret = T_MONITOR_EXPORT();
        }
    };

    func_2(true);
    mm << timeline_monitor::TimelineDump::toString(ret) << std::endl;
    CHK_EQ( 5, ret.getElems().size() );
    CHK_EQ( 1, ret.getDepth() );

    func_2(false);
    mm << timeline_monitor::TimelineDump::toString(ret) << std::endl;
    CHK_EQ( 6, ret.getElems().size() );
    CHK_EQ( 0, ret.getDepth() );

    return 0;
}

int main(int argc, char** argv) {
    TestSuite ts(argc, argv);
    ts.options.printTestMessage = true;

    ts.doTest("hierarchical test", hierarchical_test);
    ts.doTest("thread transfer test", thread_transfer_test);
    ts.doTest("multi thread test", multi_thread_test);
    ts.doTest("export in the middle test", export_in_the_middle_test);

    return 0;
}
