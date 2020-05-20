#include "types.h"
#include "stat.h"
#include "user.h"

void*
start_routine(void *arg) {
    printf(1, "ptr %p\n", arg);
    thread_exit(arg);
    return 0;
}

void*
counter(void* arg) {
    int i;
    for (i = 0; i < 100; ++i)
        printf(1, "%d\n", i);

    thread_exit(0);
    return 0;
}

void*
counter10(void* arg) {
    int i;
    for (i = 0; i < 1000; i += 10)
        printf(1, "%d\n", i);

    thread_exit(0);
    return 0;
}

int
main(int argc, char* argv[])
{
    // int ret1, ret2;
    // char *str = "asdf";
    void *retval;
    struct thread_t thread, thread2;

    // printf(1, "arg %p, fptr %p\n", str, start_routine);

    // ret1 = thread_create(&thread, start_routine, str);
    // ret2 = thread_join(&thread, &retval);
    // printf(1, "ret1 %d, ret2 %d\n", ret1, ret2);
    // printf(1, "retval: %p\n", retval);

    thread_create(&thread, counter, 0);
    thread_create(&thread2, counter10, 0);

    thread_join(&thread, &retval);
    thread_join(&thread2, &retval);

    exit();
}