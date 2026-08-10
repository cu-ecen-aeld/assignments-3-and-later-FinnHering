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

#define OUTPUT "/var/tmp/aesdsocketdata"

bool should_close = false;
void handle_close_signal(int signal) {
    should_close = true;
}


int main(size_t argc, char** argv) {
    bool deamonize = argc >= 2 && argv[1] == "-d";

    openlog(NULL, 0, LOG_USER);
    syslog(LOG_DEBUG, "setting up signal handlers...");
    
    struct sigaction sa = {handle_close_signal};
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo hints = {};
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    struct addrinfo* addrinfo = NULL;
    int err = getaddrinfo(NULL, "9000", &hints, &addrinfo);
    if (err) {
        syslog(LOG_ERR, "Unable to obtain network configuration: %s", gai_strerror(err));
        return -1;
    }
    syslog(LOG_DEBUG, "Binding...");
    if (bind(socketfd, addrinfo->ai_addr, addrinfo->ai_addrlen)) {
        syslog(LOG_ERR, "Bind failed: %s\n", strerror(errno));
        freeaddrinfo(addrinfo);
        return -1;
    }
    freeaddrinfo(addrinfo);
    addrinfo = NULL;

    switch(fork()) {
        case -1: 
            syslog(LOG_ERR, "Cannot fork. %s", strerror(errno));
            break; 
        case 0:
            close(0);
            close(1);
            close(2);
            break;
        default: 
            syslog(LOG_INFO, "Daemonized");
            return 0;
    }

    syslog(LOG_DEBUG, "Listening...");
    if (listen(socketfd, 0xfff)) {
        syslog(LOG_ERR, "Listen failed: %s\n", strerror(errno));
    }

    int confd;
    struct sockaddr peer_addr = {};
    socklen_t peer_addrlen;
    int logfilefd = open(OUTPUT, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (logfilefd < 0) {
        syslog(LOG_ERR, "Unable to open file /var/tmp/aesdsocketdata: %s", strerror(errno));
        return -1;
    }

    syslog(LOG_DEBUG, "Waiting for incoming connections...");
    while (!should_close && (confd = accept(socketfd, &peer_addr, &peer_addrlen)) >= 0) {
        char peer_host[NI_MAXHOST];
        char peer_service[NI_MAXSERV];
        int err = getnameinfo(&peer_addr, peer_addrlen, peer_host, NI_MAXHOST, peer_service, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
        
        syslog(LOG_INFO, "Accepted connection from %s\n", peer_host);
        char buf[BUFSIZ];
        
        size_t package_len = 0;
        char* package_content = NULL;
        size_t package_offset = 0;

        // Recv data until exhausted (connection closed...)
        int recv_status; 
        while ((recv_status = recv(confd, buf, BUFSIZ, 0)) > 0) {
            syslog(LOG_DEBUG, "Reading %d bytes from Peer %s...", recv_status, peer_host);
            package_content = realloc(package_content, package_len + recv_status);
            package_len += recv_status;
            
            // Read buffer byte-by-byte
            for (int i = 0; i < recv_status; i++) {
                // Copy the byte no matter what
                syslog(LOG_DEBUG, "Got byte %x from Peer %s", buf[i], peer_host);
                package_content[package_offset] = buf[i];
                package_offset++;
                
                // Handle package boundary
                if (buf[i] == '\n') {
                    syslog(LOG_DEBUG, "Package boundary recieved! Writing %ld bytes to file", package_offset);
                    // Write a line into the file. Since the package_content already holds the \n we dont need to append one

                    if (write(logfilefd, package_content, package_offset) < 0) {
                        free(package_content);
                        package_content = NULL;
                        if (should_close) goto shutdown_gracefully;
                        syslog(LOG_ERR, "Unable to write file: %s", strerror(errno));
                        return -1;
                    }
                    
                    syslog(LOG_DEBUG, "Sending back file content...");
                    char sendbuf[BUFSIZ];
                    size_t data_len;
                    for (int i = 0; data_len = pread(logfilefd, sendbuf, BUFSIZ, i); i += data_len) {
                        syslog(LOG_DEBUG, "Sending %ld bytes...", data_len);
                        if (send(confd, sendbuf, data_len, 0) < 0) {
                            free(package_content);
                            package_content = NULL;
                            if (should_close) goto shutdown_gracefully;
                            syslog(LOG_ERR, "Unable to send data: %s", strerror(errno));
                            return -1;
                        }
                    }
                    if (data_len < 0) {
                        free(package_content);
                        package_content = NULL;
                        if (should_close) goto shutdown_gracefully;
                        syslog(LOG_ERR, "Unable to read file: %s", strerror(errno));
                        return -1;
                    }
                    

                    // Realocate the package content to hold the rest of the data...f
                    size_t new_len = recv_status - (i + 1);
                    package_content = realloc(package_content, new_len);
                    package_len = new_len;
                    package_offset = 0;
                }
            }
        }
        if (recv_status < 0) {
            if (errno == EINTR && should_close) {
                free(package_content);
                package_content = NULL;
                package_len = 0;
                goto shutdown_gracefully;
            } else {
                syslog(LOG_ERR, "Unable to recv: %s", strerror(errno));
                return -1;
            }
        }
        syslog(LOG_INFO, "Closed connection from %s", peer_host);
        close(confd);
    }
    close(logfilefd);

    if (confd < 0 && should_close) {
        goto shutdown_gracefully;
    } else {
        syslog(LOG_ERR,"Accept failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
shutdown_gracefully:
    syslog(LOG_INFO, "Caught signal, exiting");
    close(confd);
    close(socketfd);
    close(logfilefd);
    unlink(OUTPUT);
    return 0;
}