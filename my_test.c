#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <backtrace.h>
#include <inttypes.h>
#include <string.h>

//gcc my_test.c -o backtrace -I./include -L./lib -lbacktrace -g -rdynamic

// Global backtrace state (initialized in main)
static struct backtrace_state *g_state = NULL;

// Error callback: handles libbacktrace internal errors
static void error_callback(void *data, const char *msg, int errnum) {
    if (msg != NULL) {
        write(STDERR_FILENO, "Backtrace Error: ", 15);
        write(STDERR_FILENO, msg, strlen(msg));
        write(STDERR_FILENO, "\n", 1);
    }
}

// Full callback: called for each stack frame
static int full_callback(void *data, uintptr_t pc, const char *filename, 
                         int lineno, const char *function) {
    char buf[512];
    int len;
    
    if (function == NULL) {
        len = snprintf(buf, sizeof(buf), "[0x%0" PRIxPTR "]\n", pc);
    } else {
        len = snprintf(buf, sizeof(buf), "%s at %s:%d\n", function, filename, lineno);
    }
    
    // Use write() for async-signal safety
    write(STDERR_FILENO, buf, len);
    
    return 0; // Continue stack walk
}

// Signal handler
void signal_handler(int sig) {
    char msg[64];
    int len = snprintf(msg, sizeof(msg), "\n>>> Signal %d (%s) caught <<<\nStack Trace:\n", sig, strsignal(sig));
    write(STDERR_FILENO, msg, len);

    // Print stack trace
    backtrace_full(g_state, 0, full_callback, error_callback, NULL);
    
    write(STDERR_FILENO, ">>> End of Stack Trace <<<\n", 27);
    
    _exit(1); // Use _exit to avoid flushing stdio buffers (deadlock risk)
}

// --- Test Functions ---

void trigger_crash() {
    int *ptr = NULL;
    *ptr = 42; // Triggers SIGSEGV
}

void function_b() { trigger_crash(); }
void function_a() { function_b(); }

int main() {
    // Initialize state early (Async-Signal-Safe practice)
    g_state = backtrace_create_state(NULL, 0, error_callback, NULL);
    
    // Register signal handler
    signal(SIGSEGV, signal_handler);
    
    printf("Program started. About to crash...\n");
    function_a();
    
    return 0;
}
