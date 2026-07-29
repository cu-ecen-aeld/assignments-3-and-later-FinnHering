#include <syslog.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(size_t argc, char** argv) {

    openlog(NULL, 0, LOG_USER);

    if (argc < 3) {
        printf("USAGE: writer [file-path] [str]");
        exit(1);
    }

    char* file_path = argv[1];
    char* str = argv[2];

    int fd = creat(file_path, 0644);
    int openerr = errno;
    if (openerr) {
        syslog(LOG_ERR, "Unable to open file at: %s: %s", file_path, strerror(openerr));
        exit(1);
    }

    syslog(LOG_DEBUG, "Writing %s to %s", file_path, str);

    // This is actually correct. We dont want to write the last bit (NUL).
    // If we'd copy the NUL byte grep would concider the text file to be a binary..
    write(fd, str, strlen(str));
    int writeerr = errno;

    if (writeerr) {
        syslog(LOG_ERR, "Unable to write to file opened at: %s: %s", file_path, strerror(writeerr));
        exit(1);
    }

    close(fd);
    return 0;
}