#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    printf("STARTING INIT!\n");

    printf("Mounting /proc\n");
    system("/busybox-x86_64 mount -t proc /proc");

    printf("Opening /proc/fb\n");
    int fbfd = open("/proc/fb", O_RDWR);
    if (fbfd < 0) {
        printf("Couldn't open FB device!\n");
    } else {
        printf("Ok\n");
        close(fbfd);
    }
    return 0;
}
