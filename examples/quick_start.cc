#include "timeline_monitor.h"

// To use `TestSuite::sleep_XX` functions. Not needed in practice.
#include "tests/test_common.h"

#include <cstdlib>
#include <iostream>
#include <thread>

void func_a() {
    // Monitor the elapsed time of `func_a`.
    T_MONITOR_FUNC();

    for (size_t ii=0; ii<2; ++ii) {
        // Monitor the elapsed time of this block,
        // in the name of "func_a_inner".
        T_MONITOR_BLOCK("func_a_inner");
        TestSuite::sleep_us(1);
    }
    TestSuite::sleep_ms(3);
}

void func_b() {
    // Monitor the elapsed time of `func_b`.
    T_MONITOR_FUNC();
    for (size_t ii=0; ii<2; ++ii) {
        func_a();
        TestSuite::sleep_ms(1);
    }
    TestSuite::sleep_ms(7);
}

void func_c() {
    // Monitor the elapsed time of `func_c`
    // (beginning of monitoring in this thread).
    T_MONITOR_FUNC();

    for (size_t ii=0; ii<3; ++ii) {
        func_b();
        TestSuite::sleep_ms(13);
    }
    TestSuite::sleep_ms(107);

    // Check elapsed time since the beginning.
    std::cout << T_MONITOR_ELAPSED_TIME() << " us elapsed" << std::endl;

    // Stop monitoring of `func_c`, and export the current contents.
    timeline_monitor::Timeline exported = T_MONITOR_EXPORT();

    // Dump and print.
    std::cout << timeline_monitor::TimelineDump::toString(exported)
              << std::endl;
}

void func_d(timeline_monitor::Timeline* src) {
    // Monitor the elasped time of `func_d`, continuing on given timeline `src`.
    T_MONITOR_FUNC_CUSTOM(*src);
    TestSuite::sleep_ms(27);

    // Check elapsed time since the beginning.
    std::cout << T_MONITOR_ELAPSED_TIME() << " us elapsed" << std::endl;

    // Stop monitoring of `func_d`, and export the current contents.
    timeline_monitor::Timeline exported = T_MONITOR_EXPORT();

    // Dump and print.
    std::cout << timeline_monitor::TimelineDump::toString(exported)
              << std::endl;
}

void func_e() {
    // Monitor the elapsed time of `func_e`
    // (beginning of monitoring).
    T_MONITOR_FUNC();
    TestSuite::sleep_ms(13);

    // Stop monitoring of `func_e`, and export the current contents.
    timeline_monitor::Timeline exported = T_MONITOR_EXPORT();

    // Pass the exported timeline to other thread.
    std::thread t(func_d, &exported);
    if (t.joinable()) t.join();
}

int main(int argc, char** argv) {
    // Timeline for hierarchical function calls in the same thread.
    func_c();

    // Timeline across different threads.
    func_e();

    return 0;
}

