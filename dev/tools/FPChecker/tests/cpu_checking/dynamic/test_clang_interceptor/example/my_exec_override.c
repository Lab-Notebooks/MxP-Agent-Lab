#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>

// Function pointer to the original execve
int (*original_execve)(const char *filename, char *const argv[], char *const envp[]);

// Your custom execve function
int execve(const char *filename, char *const argv[], char *const envp[]) {
    printf("[PRELOADED] Intercepted execve call for: %s\n", filename);
    // You can add your custom logic here

    // Call the original execve using the function pointer
    if (!original_execve) {
        original_execve = (int (*)(const char *, char *const *, char *const *))dlsym(RTLD_NEXT, "execve");
        if (!original_execve) {
            fprintf(stderr, "[PRELOADED] Error getting original execve: %s\n", dlerror());
            return -1; // Or handle the error appropriately
        }
    }
    int result = original_execve(filename, argv, envp);
    return result;
}

// Optional: Initialization function
__attribute__((constructor)) void my_library_init(void) {
    printf("[PRELOADED] My execve library loaded and initialized.\n");
}
