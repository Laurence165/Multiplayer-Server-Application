/*
 * socket demonstrations:
 * This is the server side of an "internet domain" socket connection, for
 * communicating over the network.
 *
 * In this case we are willing to wait for chatter from the client
 * _or_ for a new connection.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
    #define PORT 54890
#endif

#define SECONDS 10
#define MAX_BUF 140
#define MAX_NAME 40


struct client {
    int fd;
    struct in_addr ipaddr;
    char name[MAX_NAME];
    struct client *next;
};

static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
// static void broadcast(struct client *top, char *s, int size);
static void broadcast(struct client *top, char *s, int size, int exclude_fd);
int handleclient(struct client *p, struct client *top);


int bindandlisten(void);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;


    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = SECONDS;
        tv.tv_usec = 0;  /* and microseconds */

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %d seconds\n", SECONDS);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        printf("Received %d bytes: %s", len, buf);
        if (strncmp(buf, "attack", strlen("attack")) == 0) {
            // Implement attack logic
            attack();
        }
        else if (strncmp(buf, "powermove", strlen("powermove")) == 0) {
            // Implement power move logic
            logic();
        }
        return 0;
    } else if (len <= 0) { 
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        // broadcast(top, outbuf, strlen(outbuf));
        broadcast(top, outbuf, strlen(outbuf), -1);
        return -1;
    }
}

int attack(void){

}
int logic(void){

}
 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    char buf[MAX_BUF];
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    // Ask for the client's name
    strcpy(buf, "Please enter your name:\n");
    write(fd, buf, strlen(buf));

    // Read the client's name
    int nbytes = read(fd, buf, sizeof(buf) - 1);
    if (nbytes > 0) {
        buf[nbytes - 1] = '\0';  // Replace newline character
        strncpy(p->name, buf, sizeof(p->name));
        p->name[sizeof(p->name) - 1] = '\0';  // Ensure null-termination
    } else {
        free(p);
        return top;  // If reading name fails, abort adding the client
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    top = p;

    // Notify the new client and others
    sprintf(buf, "You are awaiting an opponent.\n");
    write(fd, buf, strlen(buf));
    // broadcast(top, "Someone new has entered the arena.\n", strlen("Someone new has entered the arena.\n"), new_client_fd);
    broadcast(top, "Someone new has entered the arena.\n", strlen("Someone new has entered the arena.\n"), fd);

    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

static void broadcast(struct client *top, char *s, int size, int exclude_fd) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != exclude_fd) {
            write(p->fd, s, size);
        }
        /* should probably check write() return value and perhaps remove client */
    }
}

// static void broadcast(struct client *top, char *s, int size) {
//     struct client *p;
//     for (p = top; p; p = p->next) {
//         write(p->fd, s, size);
//     }
//     /* should probably check write() return value and perhaps remove client */
// }