#include <stdio.h>

void trigger_crash_in_lib() {
    printf("即将在动态库中触发崩溃...\n");
    int *p = NULL;
    *p = 100; // 故意制造空指针解引用，触发 SIGSEGV
}
