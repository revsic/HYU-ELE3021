#include "types.h"
#include "stat.h"
#include "user.h"

void*
start_routine(void *arg) {
    printf(1, "ptr %p\n", arg);
    thread_exit(arg);
    return 0;
}

int
main(int argc, char* argv[])
{
    void *retval;
    char *str = "asdf";
    struct thread_t thread;
    int ret1 = thread_create(&thread, start_routine, str);
    int ret2 = thread_join(&thread, &retval);
    printf(1, "ret1 %d, ret2 %d\n", ret1, ret2);
    printf(1, "arg %p, fptr %p\n", str, start_routine);
    exit();
}