#include <err.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/sched.h>    /* 定義 struct clone_args */
#include <iostream>


#include <stdio.h>
#include <stdlib.h>

#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */

extern char etext, edata, end; /* The symbols must have some type,
                                    or "gcc -Wall" complains */

int print_id(const char *msg)
{
    std::cout << msg << std::endl;
    std::cout << "pid: " << getpid() << std::endl;
    std::cout << "ppid: " << getppid() << std::endl;
    std::cout << "tid: " << syscall(SYS_gettid) << std::endl;
    return 0;
}

auto print_maps()
{
    auto fd = open("/proc/self/maps", O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    char buf[4096];
    auto n = read(fd, buf, sizeof(buf) - 1);
    if (n == -1) {
        perror("read");
        return 1;
    }

    buf[n] = '\0';
    printf("maps:\n%s\n", buf);
    return 0;
}

int childFunc(void *arg)
{
    printf("Child stack top: %p\n", &arg);

    auto child_stack = (char*)&arg;
    auto stack = (char*)arg;
    auto stackTop = stack + STACK_SIZE;  /* Assume stack grows downward */

    // print_maps();
    
    const char *argv[] = { "clone/args.sh", "-l", nullptr };
    auto res = execve(argv[0], const_cast<char**>(argv), nullptr);

    if (res == -1) {
        perror("execve");
    }

    return 0;           /* Child terminates now */
}

int main(int argc, char *argv[])
{
    printf("First address past:\n");
    printf("    program text (etext)      %10p\n", &etext);
    printf("    initialized data (edata)  %10p\n", &edata);
    printf("    uninitialized data (end)  %10p\n", &end);

    char *stack = (char*)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED)
        err(EXIT_FAILURE, "mmap");

    // print_maps();

    char *stackTop = stack + STACK_SIZE;  /* Assume stack grows downward */
    std::cout << "stackTop: " << (void*)stackTop << std::endl;
    std::cout << "stack:    " << (void*)stack << std::endl;

    pid_t pid = clone(childFunc, stackTop, SIGCHLD, stack);
    if (pid == -1)
        err(EXIT_FAILURE, "clone");
    printf("clone() returned %jd\n", (intmax_t) pid);

    // if (waitpid(pid, NULL, 0) == -1)    /* Wait for child */
    //     err(EXIT_FAILURE, "waitpid");
    // printf("child has terminated\n");

    printf("Parent has terminated\n");

    return 0;
}
