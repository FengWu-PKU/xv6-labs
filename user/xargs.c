//
// Created by sakura on 2023/3/19.
//
#include "../kernel/param.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    char buf[64], *args[32];
    for (int i = 1; i < argc; i++)
        args[i - 1] = argv[i];
    while (1) {
        int idx = argc - 1;
        gets(buf, 64);
        if (buf[0] == 0)
            break;
        args[idx++] = buf;
        for (char *p = buf; *p; p++) {
            if (*p == ' ') {
                while(*p==' ') p++;
                args[idx++] = p;
            } else if (*p == '\n') {
                *p = 0;
            }
        }
        if (fork() == 0) {
            exec(argv[1], args);
        }
    }
    wait(0);
    exit(0);
}