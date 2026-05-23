#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdatomic.h> // 引入原子操作库
#include <backtrace.h>

extern void trigger_crash_in_lib();

static struct backtrace_state *global_state = NULL;
// 使用原子变量作为“防重入锁”，确保崩溃处理逻辑只执行一次
static atomic_flag crash_lock = ATOMIC_FLAG_INIT;

static int full_callback(void *data, uintptr_t pc, const char *filename, int lineno, const char *function) {
    // 注意：在极端严格的生产环境中，printf 也有微小风险。
    // 最安全的做法是将信息写入静态缓冲区，再用 write() 输出。
    printf("  %s:%d: %s (PC=%p)\n", filename ? filename : "??", lineno, function ? function : "??", (void*)pc);
    return 0;
}

static void error_callback(void *data, const char *msg, int errnum) {
    fprintf(stderr, "Error: %s (errnum=%d)\n", msg, errnum);
}

void crash_handler(int sig) {
    // 【核心优化1】尝试获取锁。如果已经被其他线程持有（说明正在处理崩溃），
    // 则当前线程直接退出，避免打断正在进行的堆栈打印。
    //if (atomic_flag_test_and_set(&crash_lock)) {
        //// 为了避免线程卡死，这里直接使用 _exit 退出整个进程
        //_exit(1); 
    //}

    // 【核心优化2】在打印堆栈前，屏蔽掉所有常见崩溃信号，防止自身被再次打断
    sigset_t mask;
    sigfillset(&mask); // 屏蔽所有信号
    sigprocmask(SIG_SETMASK, &mask, NULL);

    // 为了绝对安全，建议将 printf 替换为 write(STDERR_FILENO, str, len)
    printf("\n=====================================\n");
    printf("捕获到崩溃信号: %d (SIGSEGV)\n", sig);
    printf("程序调用栈回溯如下:\n");
    
    backtrace_full(global_state, 0, full_callback, error_callback, NULL);
    
    printf("=====================================\n\n");

    // 【核心优化3】处理完毕后，直接调用 _exit 终止整个进程。
    // 绝对不要调用 exit() 或 return，因为这会触发全局析构函数，
    // 在多线程崩溃场景下极易引发二次崩溃或死锁。
    _exit(1);
}

int main() {
    global_state = backtrace_create_state(NULL, 1, NULL, NULL); // 建议开启多线程支持(参数设为1)
    
    // 建议使用 sigaction 替代 signal，它能更精确地控制信号掩码。
    struct sigaction sa;
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGSEGV, &sa, NULL);
    
    trigger_crash_in_lib();
    return 0;
}
