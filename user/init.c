// init: The initial user-level program


#include "ulib/user.h"
char *argv[] = {"shell", 0};

int
main(void) {
    int pid, wpid;

    open("console", O_RDWR);
    open("console", O_RDWR);
    open("console", O_RDWR);

    for (;;) {
        printf("init: starting shell\n");
        pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            exec("shell", argv);
            printf("init: exec shell failed\n");
            exit(1);
        }

        for (;;) {
            // this call to wait() returns if the shell exits,
            // or if a parentless process exits.
            wpid = wait((int *) 0);
            if (wpid == pid) {
                // the shell exited; restart it.
                printf("shell exit");
                break;
            } else if (wpid < 0) {
                printf("init: wait returned an error\n");
                exit(1);
            } else {
                // it was a parentless process; do nothing.
            }
        }
    }
}
