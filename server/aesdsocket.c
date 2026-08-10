#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <strings.h>

#define OUTPUT "/var/tmp/aesdsocketdata"
#define SIGINT_BREAK(res) if(res == SIGINT) break; 

bool should_close = false;
void handle_close_signal(int signal) {
    should_close = true;
}


int main(size_t argc, char** argv) {
    bool deamonize = argc >= 2 && !strcmp(argv[1], "-d");
    

    openlog(NULL, 0, LOG_USER);

    struct sigaction sa = {handle_close_signal};
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    setlogmask(LOG_UPTO(LOG_INFO));

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    // QUIRK: Quickly restarting this application causes it to crash because it cannot bind. 
    // This is because the OS is holding back the Port for some time...
    // Fix this by setting the socket option for allowing rebinding
    int reuse = 1; 
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));


    struct addrinfo hints = {};
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    struct addrinfo* addrinfo = NULL;
    int err = getaddrinfo(NULL, "9000", &hints, &addrinfo);
    if (err) {
        syslog(LOG_ERR, "Unable to obtain network configuration: %s", gai_strerror(err));
        return -1;
    }

    if (bind(socketfd, addrinfo->ai_addr, addrinfo->ai_addrlen)) {
        syslog(LOG_ERR, "Bind failed: %s\n", strerror(errno));
        freeaddrinfo(addrinfo);
        return -1;
    }
    freeaddrinfo(addrinfo);
    addrinfo = NULL;
    if (deamonize) {
        switch(fork()) {
            case -1: 
                syslog(LOG_ERR, "Cannot fork. %s", strerror(errno));
                break; 
            case 0:
                close(0);
                close(1);
                close(2);
                open("/dev/null", O_WRONLY);
                open("/dev/null", O_WRONLY);
                open("/dev/null", O_RDONLY);
                break;
            default: 
                syslog(LOG_INFO, "Daemonized");
                return 0;
        }
    }


    if (listen(socketfd, 0xfff)) {
        syslog(LOG_ERR, "Listen failed: %s\n", strerror(errno));
    }

    int confd;

    FILE* logfile = fopen(OUTPUT, "w+");
    if (logfile < 0) {
        syslog(LOG_ERR, "Unable to open file /var/tmp/aesdsocketdata: %s", strerror(errno));
        return -1;
    }

    while(!should_close) {
        syslog(LOG_DEBUG, "Accepting connections...");
        struct sockaddr peer_addr = {};
        socklen_t peer_addrlen = sizeof(peer_addr);
        confd = accept(socketfd, &peer_addr, &peer_addrlen);
        if (confd == -1) {
            if (errno == SIGINT) break;
            else continue;
        }
        FILE* con = fdopen(confd, "r");

        char peer_host[NI_MAXHOST] = {0};
        char peer_service[NI_MAXSERV] = {0};
        int err = getnameinfo(&peer_addr, peer_addrlen, peer_host, NI_MAXHOST, peer_service, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
        syslog(LOG_INFO, "Accepted connection from %s\n", peer_host);

        char* package = NULL;
        size_t package_size = 0;
        size_t line_len = getline(&package, &package_size, con);
        syslog(LOG_DEBUG, "Got package: %s", package);

        // Write to file
        fwrite(package, sizeof(char), line_len, logfile);
        free(package);
        
        // Read from file
        fseek(logfile, 0, SEEK_SET);
        char* buf[BUFSIZ] = {0};
        size_t num_read = 0;
        while ((num_read = fread(buf, sizeof(char), BUFSIZ, logfile)) > 0) {
            syslog(LOG_DEBUG, "Writing: %ld bytes to client", num_read);
            write(confd, buf, num_read * sizeof(char));
        }
        
        fseek(logfile, 0, SEEK_END);
        
        syslog(LOG_INFO, "Closed connection from %s", peer_host);
        close(confd);

    }
    fclose(logfile);
    close(socketfd);
    unlink(OUTPUT);
    syslog(LOG_INFO, "Caught signal, exiting");
    closelog();

    return 0;
}