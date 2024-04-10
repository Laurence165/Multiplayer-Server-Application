#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

#ifndef PORT
    #define PORT 54890
#endif

#define SECONDS 10
#define MAX_BUF 140
#define MAX_NAME 40
#define MAX_HP 30
#define MIN_HP 20
#define MAX_POWERMOVES 3
#define MIN_POWERMOVES 1
#define REGULAR_DAMAGE_MIN 2
#define REGULAR_DAMAGE_MAX 6
#define POWERMOVE_DAMAGE_MULTIPLIER 3
#define POWERMOVE_MISS_CHANCE 50

struct client {
    int fd;
    struct in_addr ipaddr;
    char name[MAX_NAME];
    struct client *next;
    int in_match; // 0 is waiting, 1 is in match
    struct client *opp;
    struct client *prev_opp;
    int hp;
    int powermoves;
};

struct client *waiting_clients_queue = NULL; // Pointer to the first client in the waiting queue
struct client *active_player = NULL; // Pointer to the currently active player
struct client *opponent = NULL; // Pointer to the opponent of the active player

// Initialize the random number generator seed
void init_random_seed() {
    srand((unsigned int)time(NULL));
}

// Generate a random integer within a given range
int rand_range(int min, int max) {
    return rand() % (max - min + 1) + min;
}
void start_match(struct client *client1, struct client *client2);
void send_menu(struct client *p);
void broadcast(struct client *top, char *s, int size, int exclude_fd);
struct client *removeclient(struct client *top, int fd);
int bindandlisten(void);
void match_clients(struct client *head) {
    struct client *current = head;
    struct client *p1 = NULL;
    struct client *p2 = NULL;
    while (current != NULL) {
        if (!current->in_match) {
            if (p1 == NULL) {
                p1 = current;
            } else {
                p2 = current;
                p1->in_match = 1;
                p2->in_match = 1;
                p1->opp = p2;
                p2->opp = p1;
                start_match(p1, p2);  // start the match between p1 and p2
                p1 = NULL;  // reset p1 for the next match
            }
        }
        current = current->next;  // properly move to the next client
    }
}

void start_match(struct client *client1, struct client *client2) {
    char buf[MAX_BUF];

    // Initialize hitpoints and powermoves for both players
    client1->hp = rand_range(MIN_HP, MAX_HP);
    client2->hp = rand_range(MIN_HP, MAX_HP);
    client1->powermoves = rand_range(MIN_POWERMOVES, MAX_POWERMOVES);
    client2->powermoves = rand_range(MIN_POWERMOVES, MAX_POWERMOVES);

    // Notify both clients that the match is starting
    sprintf(buf, "Match is starting between %s and %s\n", client1->name, client2->name);
    write(client1->fd, buf, strlen(buf));
    write(client2->fd, buf, strlen(buf));

    // Set the active player to be the first player
    active_player = client1;
    opponent = client2;

    // Send menu of valid commands to the active player
    send_menu(active_player);
}

void send_menu(struct client *p) {
    char menu[MAX_BUF];
    sprintf(menu, "Valid commands:\n- attack\n- powermove (if available)\n");
    write(p->fd, menu, strlen(menu));
}
void attack(struct client *p, struct client *top) {
    char outbuf[512];
    int damage_amount = rand() % 5 + 2; // Regular attack: 2-6 hitpoints damage
    opponent = p->opp;  // Set opponent
    if (opponent != NULL) {
        opponent->hp -= damage_amount;
        if (opponent->hp <= 0) {
            // Opponent defeated
            sprintf(outbuf, "You defeated %s!\r\n", opponent->name);
            write(p->fd, outbuf, strlen(outbuf));
            sprintf(outbuf, "%s has been defeated!\r\n", opponent->name);
            broadcast(top, outbuf, strlen(outbuf), p->fd);
            // Remove defeated opponent
            top = removeclient(top, opponent->fd);
        } else {
            // Inform both clients about the attack and damage inflicted
            sprintf(outbuf, "You attacked %s and inflicted %d damage!\r\n", opponent->name, damage_amount);
            write(p->fd, outbuf, strlen(outbuf));
            sprintf(outbuf, "%s attacked you and inflicted %d damage!\r\n", p->name, damage_amount);
            write(opponent->fd, outbuf, strlen(outbuf));
        }
    } else {
        // No opponent found
        sprintf(outbuf, "No opponent found!\r\n");
        write(p->fd, outbuf, strlen(outbuf));
    }
}

void powermove(struct client *p, struct client *top) {
    char outbuf[512];
    if (p->powermoves > 0) {
        // 50% chance of hit
        bool hit = rand() % 2 == 0;
        if (hit) {
            int damage_amount = (rand() % 5 + 2) * 3; // Powermove: 3 times regular attack damage
            opponent = p->opp;  // Set opponent
            if (opponent != NULL) {
                opponent->hp -= damage_amount;
                if (opponent->hp <= 0) {
                    // Opponent defeated
                    sprintf(outbuf, "You defeated %s with a powermove!\r\n", opponent->name);
                    write(p->fd, outbuf, strlen(outbuf));
                    sprintf(outbuf, "%s has been defeated by a powermove!\r\n", opponent->name);
                    broadcast(top, outbuf, strlen(outbuf), p->fd);
                    // Remove defeated opponent
                    top = removeclient(top, opponent->fd);
                } else {
                    // Inform both clients about the powermove and damage inflicted
                    sprintf(outbuf, "You used a powermove against %s and inflicted %d damage!\r\n", opponent->name, damage_amount);
                    write(p->fd, outbuf, strlen(outbuf));
                    sprintf(outbuf, "%s used a powermove against you and inflicted %d damage!\r\n", p->name, damage_amount);
                    write(opponent->fd, outbuf, strlen(outbuf));
                }
            } else {
                // No opponent found
                sprintf(outbuf, "No opponent found!\r\n");
                write(p->fd, outbuf, strlen(outbuf));
            }
        } else {
            // Powermove missed
            sprintf(outbuf, "Your powermove missed!\r\n");
            write(p->fd, outbuf, strlen(outbuf));
        }
        // Decrease the number of powermoves after using one
        p->powermoves--;
    } else {
        // No powermoves remaining
        sprintf(outbuf, "You have no powermoves remaining!\r\n");
        write(p->fd, outbuf, strlen(outbuf));
    }
}

int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        printf("Received %d bytes: %s", len, buf);
        if (strncmp(buf, "a", strlen("a")) == 0) {
            // Player wants to perform a regular attack
            if (p == active_player) {
                attack(p, top);
            } else {
                // Inactive player's turn or invalid command, discard
                sprintf(outbuf, "It's not your turn to attack.\n");
                write(p->fd, outbuf, strlen(outbuf));
            }
        } else if (strncmp(buf, "p", strlen("p")) == 0) {
            // Player wants to perform a powermove
            if (p == active_player) {
                powermove(p, top);
            } else {
                // Inactive player's turn or invalid command, discard
                sprintf(outbuf, "It's not your turn to attack.\n");
                write(p->fd, outbuf, strlen(outbuf));
            }
        } else if (strncmp(buf, "s", strlen("s")) == 0) {
            // Player wants to say something
            if (p == active_player) {
                char *message = buf + strlen("s") + 1; // Skip the "s" command and the following space
                // Iterate through the message and replace newline characters with null terminators
                char *newline = strchr(message, '\n');
                if (newline != NULL) {
                    *newline = '\0';
                }
                // Prepare the message to be sent
                char outbuf[MAX_BUF];
                sprintf(outbuf, "%s says: %s\n", p->name, message);
                // Send the message to both players
                write(p->fd, outbuf, strlen(outbuf));
                write(p->opp->fd, outbuf, strlen(outbuf));
                } else {
                    // Inactive player's turn or invalid command, discard
                    sprintf(outbuf, "It's not your turn to talk.\n");
                    write(p->fd, outbuf, strlen(outbuf));
                }
            } else {
            // Invalid command, discard
            sprintf(outbuf, "Invalid command.\n");
            write(p->fd, outbuf, strlen(outbuf));
            }
        // Send the menu of valid commands to the active player
        if (p == active_player) {
            sprintf(outbuf, "Menu:\n(a) Attack\n(p) Powermove\n(s) Say something\n");
            write(p->fd, outbuf, strlen(outbuf));
        }
        return 0;
        } else  { 
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        // broadcast(top, outbuf, strlen(outbuf));
        broadcast(top, outbuf, strlen(outbuf), -1);
        return -1;
    }
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
    p->in_match = 0;
    p->opp = NULL;
    p->prev_opp = NULL;
    top = p;

    // Notify the new client and others
    sprintf(buf, "You are awaiting an opponent.\n"); 
    write(fd, buf, strlen(buf));

    char message[MAX_NAME+strlen(" has entered the arena")];
    sprintf(message, "%s has entered the arena\n", p->name);
    broadcast(top, message, strlen(message), fd);

    // Add client to waiting client queue
    // Enqueue the new client
    // enqueue_client(&waiting_clients_queue, p);

    // Attempt to match clients
    match_clients(top);

    return top;
}

struct client *removeclient(struct client *top, int fd) {
    struct client **p;
 
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));

        if ((*p)->in_match && (*p)->opp != NULL) {
            (*p)->opp->in_match = 0; // Clear the in_match flag for the opponent
            (*p)->opp->opp = NULL; // Clear the in_match flag for the opponent
            // Correctly send the message to the opponent using their fd
            write((*p)->opp->fd, "Opponent Disconnected, Going back to arena\n", strlen("Opponent Disconnected, Going back to arena\n"));
            match_clients(top);
        }

        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

void broadcast(struct client *top, char *s, int size, int exclude_fd) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != exclude_fd && !p->in_match) { 
            write(p->fd, s, size);
        }
        /* should probably check write() return value and perhaps remove client */
    }
}

int main(void) {
    // Initialize the random number generator seed
    init_random_seed();
    
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
            //match_clients(head);
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