#include <stdio.h>

#include "timeline_monitor.h"

void dummy_function() {
    // Do nothing (to check if two different files that include
    // header files can be linked well without any conflicts).
    T_MONITOR_FUNC();
}

