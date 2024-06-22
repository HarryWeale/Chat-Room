#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/types.h>
#include <sys/un.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

// Listening port
#define PORT "9034"

// Get the listener
int get_listener_socket() {
        struct addrinfo hints, *res, *p;
        int listener;
        int yes = 1;
        int rv;
        memset(&hints, 0, sizeof hints);
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
                perror("Error retriving socket address information");
                exit(1);
        }
        for (p = res; p != NULL; p = p->ai_next) {
                listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (listener < 0) {
                        continue;
                }
                setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
                if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
                        close(listener);
                        continue;
                }
                break;
        }
        if (p == NULL) {
                return -1;
        }
        // Allowing 10 connections in backlog
        if (listen(listener, 10) == -1) {
                return -1;
        }
        freeaddrinfo(res);
        return listener;
}

// Add connected socket to list of sockets
void add_to_pfds(struct pollfd *pfds[], int *pfds_size, int *fd_count, int new_fd) {
        if (*fd_count == *pfds_size) {
                // Double the size of pfds
                *pfds_size *= 2;
                *pfds = realloc(*pfds, (sizeof (**pfds))*(*pfds_size));
        }
        (*pfds)[*fd_count].fd = new_fd;
        (*pfds)[*fd_count].events = POLLIN;
        (*fd_count)++;
}

// Delete from list of sockets
void del_from_pfds(struct pollfd pfds[], int *fd_count, int i_del) {
        pfds[i_del] = pfds[(*fd_count) - 1];
        (*fd_count)--;
}

// Send a join message when a new user joins the chat
void send_join_message(int listener, struct sockaddr_storage* their_addr, struct pollfd* pfds, int fd_count) {
        for (int j = 0; j < fd_count; j++) {
                struct sockaddr_in *sin = (struct sockaddr_in *) their_addr;
                char *address = inet_ntoa((sin)->sin_addr);
                char connectMessage[] = " just joined!\n";
                char *finalMessage = strcat(address, connectMessage);
                if (pfds[j].fd != listener) {
                        send(pfds[j].fd, finalMessage, strlen(finalMessage), 0);
                }
        }
}

// Send a leave message when a user leaves the chat
void send_leave_message(int listener, char* delAddr, struct pollfd* pfds, int fd_count) {
        for (int j = 0; j < fd_count; j++) {
                if (pfds[j].fd != listener) {
                        char connectMessage[] = " just left!\n";
                        char *finalMessage = strcat(delAddr, connectMessage);
                        send(pfds[j].fd, finalMessage, strlen(finalMessage), 0);
                }
        }
}

// Get the name of the socket we are going to delete
char* del_sock_string(int sender) {
        struct sockaddr_in del_sock_addr;
        socklen_t delSockSize = sizeof del_sock_addr;
        int intAddress = getsockname(sender, (struct sockaddr *) &del_sock_addr, &delSockSize);
        char *delAddr = inet_ntoa(del_sock_addr.sin_addr);
}

// Send message to everyone in the chat
void send_message(int listener, int sender, int numBytes, int fd_count, struct pollfd* pfds, char* buf) {
        for (int k = 0; k < fd_count; k++) {
                if ((pfds[k].fd != sender) && (pfds[k].fd != listener)) {
                        // Send message to client sockets that aren't the sender
                        send(pfds[k].fd, buf, numBytes, 0);
                }
        }
}


int main(void) {
        int new_fd;
        int listener = get_listener_socket();
        if (listener == -1) {
                printf("Error getting listener socket");
                return 1;
        }
        char buf[256];
        int pfds_size = 5;
        int fd_count = 1;
        struct pollfd *pfds = malloc(sizeof *pfds);
        pfds[0].fd = listener;
        pfds[0].events = POLLIN;
        struct sockaddr_storage their_addr;
        socklen_t addrlen;
        memset(&their_addr, 0, sizeof (struct sockaddr_storage));
        memset(&addrlen, 0, sizeof addrlen);
        for (;;) {
                // Polling connected sockets for received messages
                int poll_events = poll(pfds, fd_count, -1);
                if (poll_events < 0) {
                        perror("Error polling");
                        return 1;
                }
                for (int i = 0; i < fd_count; i++) {
                        // This socket is ready to receive
                        if (pfds[i].revents & POLLIN) {
                                // If it's the listener, it's a client connecting
                                if (pfds[i].fd == listener) {
                                        // Accept the connection
                                        if ((new_fd = accept(listener, (struct sockaddr*) &their_addr, &addrlen)) == -1) {
                                                perror("Error establishing new client socket connection");
                                                return 1;
                                        }
                                        // Send join message to everyone in chat
                                        send_join_message(listener, &their_addr, pfds, fd_count);
                                        // Add to list of struct pollfd's
                                        add_to_pfds(&pfds, &pfds_size, &fd_count, new_fd);
                                // If it's not a client, it's a message
                                } else {
                                        int numBytes = recv(pfds[i].fd, buf, sizeof buf, 0);
                                        int sender = pfds[i].fd;
                                        // Bad client
                                        if (numBytes <= 0) {
                                                if (numBytes == 0) {
                                                        printf("Socket hung up");
                                                } else {
                                                        perror("recv error");
                                                }
                                                char* delAddr = del_sock_string(sender);
                                                // Remove the socket from pfds
                                                del_from_pfds(pfds, &fd_count, i);
                                                // Send leave message to other users in the chat
                                                send_leave_message(listener, delAddr, pfds, fd_count);
                                                // Close the bad/disconnected socket
                                                close(sender);
                                        // Good message
                                        } else {
                                                // Send message to everyone in the chat
                                                send_message(listener, sender, numBytes, fd_count, pfds, buf);
                                        }
                                }
                        }
                }
        }
        return 0;
}
