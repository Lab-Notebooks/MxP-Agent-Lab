#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char *args[] = {"/bin/ls", "-l", NULL};
    char *env[] = {NULL}; // Minimal environment
    printf("Calling execve...\n");
    int result = execve("/bin/ls", args, env);
    perror("execve failed"); // This will only be printed if execve doesn't succeed
    printf("Should not reach here if execve succeeds.\n");
    return 0;
}
