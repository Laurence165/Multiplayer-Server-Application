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
    #define PORT 54770
#endif
 
#define SECONDS 2
#define MAX_BUF 160
#define MAX_NAME 40
#define MAX_HP 20
#define MIN_HP 10
#define MAX_POWERMOVES 3
#define MIN_POWERMOVES 1
#define REGULAR_DAMAGE_MIN 2
#define REGULAR_DAMAGE_MAX 6
#define POWERMOVE_DAMAGE_MULTIPLIER 3
#define POWERMOVE_MISS_CHANCE 50
#define BATTLE 2
#define WAITING 1
#define IN_LOBBY 0
struct client {
    int fd;
    struct in_addr ipaddr;
    char name[MAX_NAME];
    struct client *next;
    int status; // 0 is in lobby, 1 is waiting, 2 is battle
    struct client *opp;
    struct client *prev_opp;
    int hp;
    int powermoves;
    int isActive; //0 for inactive, 1 is active
};
 
//struct client *active_player = NULL; // Pointer to the currently active player
//struct client *opponent = NULL; // Pointer to the opponent of the active player
 
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
void enterLobby(struct client *p);
struct client *removeclient(struct client *top, int fd);
int bindandlisten(void);
 
void match_clients(struct client *head) {
 
    struct client *current = head;
    struct client *p1 = NULL;
    struct client *p2 = NULL;
    while (current != NULL) {
        if (current->status == WAITING) {
            if (p1 == NULL) {
                p1 = current;
            } else if (p1->prev_opp!=current) {
                p2 = current;
                
                start_match(p1, p2);  // start the match between p1 and p2
                p1 = NULL;  // reset p1 for the next match
            }
        }
        current = current->next;  // properly move to the next client
    }
}
 
void start_match(struct client *client1, struct client *client2) {
    char buf[MAX_BUF];
 
 
    client1->status = BATTLE;
    client2->status = BATTLE;
    client1->opp = client2;
    client2->opp = client1;
    // Initialize hitpoints and powermoves for both players
    client1->hp = rand_range(MIN_HP, MAX_HP);
    client2->hp = rand_range(MIN_HP, MAX_HP);
    client1->powermoves = rand_range(MIN_POWERMOVES, MAX_POWERMOVES);
    client2->powermoves = rand_range(MIN_POWERMOVES, MAX_POWERMOVES);
 
    // Notify both clients that the match is starting
    sprintf(buf, "\nMatch is starting between %s and %s\n\n", client1->name, client2->name);
    write(client1->fd, buf, strlen(buf));
    write(client2->fd, buf, strlen(buf));
 
    sprintf(buf, "\nYou have %d hp and %s has %d hp. \n\n", client1->hp, client2->name, client2->hp);
    write(client1->fd, buf, strlen(buf));
    sprintf(buf, "\nYou have %d hp and %s has %d hp. \n\n", client2->hp, client1->name, client1->hp);
    write(client2->fd, buf, strlen(buf));
 
    // Set the active player to be the first player
    client1->isActive = true;
    
 
    //active_player = client1;
    //opponent = client2;
    
    // send waiting for the inactive player
    sprintf(buf, "\nPlayer %s starts. Waiting for Opponent. \n\n", client1->name);
    write(client2->fd, buf, strlen(buf));
    // Send menu of valid commands to the active player
    send_menu(client1);
}
 
void end_match(struct client *client1, struct client *client2,struct client *top){
    //client 1 is the winner and client 2 is the loser
    char buf[MAX_BUF];
 
    //write to players
    
    sprintf(buf, "\nYou have been defeated by %s!\n\n", client1->name);
    write(client2->fd, buf, strlen(buf));
 
    sprintf(buf,"\nGoing back to arena\r\n\n");
 
    write(client1->fd, buf, strlen(buf));
    write(client2->fd, buf, strlen(buf));
    
    client1->prev_opp = client2;
    client2->prev_opp = client1;
    client1->opp = NULL;
    client2->opp = NULL;
    client1->isActive = 0;
    client2->isActive = 0;
    client1->status = IN_LOBBY;
    client2->status = IN_LOBBY;
 
    enterLobby(client1);
    enterLobby(client2);
    //match_clients(top);
    //match_clients(top);
}
 
void send_menu(struct client *p) {
    char menu[MAX_BUF];
    sprintf(menu, "\nValid commands:\n- attack\n- powermove (if available)\n\n");
    if (p!=NULL){
        write(p->fd, menu, strlen(menu));
    }
 
 
}
int attack(struct client *p, struct client *top) {
    char outbuf[512];
    int damage_amount = rand() % 5 + 2; // Regular attack: 2-6 hitpoints damage
    struct client * opponent = p->opp;  // Set opponent
    if (opponent != NULL) {
        opponent->hp -= damage_amount;
        if (opponent->hp <= 0) {
            // Opponent defeated
            sprintf(outbuf, "\nYou defeated %s!\r\n\n", opponent->name);
            write(p->fd, outbuf, strlen(outbuf));
            // Remove defeated opponent
            end_match(p,opponent,top);
            //top = removeclient(top, opponent->fd);
            return 0;
        } else {
            // Inform both clients about the attack and damage inflicted
            sprintf(outbuf, "\nYou attacked %s and inflicted %d damage!\r\n\n", opponent->name, damage_amount);
            write(p->fd, outbuf, strlen(outbuf));
            sprintf(outbuf, "\n%s attacked you and inflicted %d damage!\r\n\n", p->name, damage_amount);
            write(opponent->fd, outbuf, strlen(outbuf));
            sprintf(outbuf, "\nYou have %d health remaining !\r\n\n", opponent->hp);
            write(opponent->fd, outbuf, strlen(outbuf));
 
            sprintf(outbuf, "\nYou have %d hp and %s has %d hp. \n\n", p->hp, opponent->name, opponent->hp);
            write(p->fd, outbuf, strlen(outbuf));
        }
    } else {
        // No opponent found
        sprintf(outbuf, "\nNo opponent found!\r\n\n");
        write(p->fd, outbuf, strlen(outbuf));
    }
    return 1;
}
 
int powermove(struct client *p, struct client *top) {
    char outbuf[512];
    if (p->powermoves > 0) {
        // 50% chance of hit
        bool hit = rand() % 2 == 0;
        if (hit) {
            int damage_amount = (rand() % 5 + 2) * 3; // Powermove: 3 times regular attack damage
            struct client * opponent = p->opp;  // Set opponent
            if (opponent != NULL) {
                opponent->hp -= damage_amount;
                if (opponent->hp <= 0) {
                    // Opponent defeated
                    sprintf(outbuf, "\nYou defeated %s with a powermove!\r\n\n", opponent->name);
                    write(p->fd, outbuf, strlen(outbuf));
                    // Remove defeated opponent
                    //top = removeclient(top, opponent->fd);
                    end_match(p,opponent,top);
                    return 0;
                } else {
                    // Inform both clients about the powermove and damage inflicted
                    sprintf(outbuf, "\nYou used a powermove against %s and inflicted %d damage!\r\n\n", opponent->name, damage_amount);
                    write(p->fd, outbuf, strlen(outbuf));
                    sprintf(outbuf, "\n%s used a powermove against you and inflicted %d damage!\r\n\n", p->name, damage_amount);
                    write(opponent->fd, outbuf, strlen(outbuf));
                    sprintf(outbuf, "\nYou have %d health remaining !\r\n\n", opponent->hp);
                    write(opponent->fd, outbuf, strlen(outbuf));
 
                    sprintf(outbuf, "\nYou have %d hp and %s has %d hp. \n\n", p->hp, opponent->name, opponent->hp);
                    write(p->fd, outbuf, strlen(outbuf));
                }
            } else {
                // No opponent found
                sprintf(outbuf, "\nNo opponent found!\r\n\n");
                write(p->fd, outbuf, strlen(outbuf));
            }
        } else {
            // Powermove missed
            sprintf(outbuf, "\nYour powermove missed!\r\n\n");
            write(p->fd, outbuf, strlen(outbuf));
        }
        // Decrease the number of powermoves after using one
        p->powermoves--;
    } else {
        // No powermoves remaining
        sprintf(outbuf, "\nYou have no powermoves remaining!\r\n\n");
        write(p->fd, outbuf, strlen(outbuf));
        return 0;
    }
    return 1;
}
 
 
int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        printf("\nReceived %d bytes: %s\n", len, buf);
 
        if (p->status == IN_LOBBY){
            if (strncmp(buf, "m", strlen("m")) == 0){
                //Player joint match making
                p->status = WAITING;
 
                // Notify the new client and others
                sprintf(buf, "\nYou are awaiting an opponent.\n\n"); 
                write(p->fd, buf, strlen(buf));
            }
            else if (strncmp(buf, "l", 1) == 0) { // If you're checking for "p", comparing the first character is sufficient
                char m[MAX_BUF] = ""; // Initialize the message buffer
                struct client *n;
 
                for (n = top; n != NULL; n = n->next) {
                    if (n->status == IN_LOBBY && n!=p) {
                        char value[200];
                        // Use n->name and n->fd since you're iterating with n
                        sprintf(value, "\n%s: %d\n\n", n->name, n->fd);
                        // Check if appending this client would overflow the buffer
                        if (strlen(m) + strlen(value) < MAX_BUF) {
                            strcat(m, value); // Append the current client's info to the message
                        } else {
                            // Handle overflow, maybe by sending the message in parts or logging an error
                            break;
                        }
                    }
                }
 
                // After the loop, send the accumulated message to the client who sent "p"
                if (strlen(m) > 0) { // Check if there's anything to send
                    write(p->fd, m, strlen(m)); // Assume p->fd is the file descriptor for the client who sent "p"
                } else {
                    // Optionally handle the case where no players are waiting
                    char *noPlayersMsg = "\nNo players in the lobby.\n\n";
                    write(p->fd, noPlayersMsg, strlen(noPlayersMsg));
                }
 
 
 
 
                
            }
 
            else if (strncmp(buf, "p", 1) == 0){
                char *message = buf + strlen("p") + 1; // Skip the "s" command and the following space
                // Iterate through the message and replace newline characters with null terminators
                int found = 0;
                struct client *opp;
                int fd = atoi(message);
                struct client *n;
                for (n = top; n != NULL; n = n->next) {
                    if (n->fd == fd && n!=p) {
                        found = 1;
                        opp = n;
                    }
                }
                if (found){
                    start_match(p,opp);

                }
                else{
                    sprintf(outbuf, "\nNo player with ID %d\n\n", fd);
                    write(p->fd,outbuf,strlen(outbuf));
                }
                
            }
            else {
                // Invalid command, discard
                sprintf(outbuf, "\nInvalid command.\n\n");
                write(p->fd, outbuf, strlen(outbuf));
                }
            return 0;
        }
 
 
 
        else if (p->status == BATTLE){
            if (strncmp(buf, "a", strlen("a")) == 0) {
                // Player wants to perform a regular attack
                if (p->isActive) {
                    if (attack(p, top)==1)
                    {p->opp->isActive = 1; p->isActive = 0;
                    
                        sprintf(buf, "\nWaiting for Opponent\n\n");
                        write(p->fd, buf, strlen(buf));
 
                        send_menu(p->opp);
                    }
                } else {
                    // Inactive player's turn or invalid command, discard
                    sprintf(outbuf, "\nIt's not your turn to attack.\n\n");
                    write(p->fd, outbuf, strlen(outbuf));
                }
            } else if (strncmp(buf, "p", strlen("p")) == 0) {
                // Player wants to perform a powermove
                if (p->isActive) {
                    if (powermove(p, top)==1)
                    {p->opp->isActive = 1; p->isActive = 0;
                    
                        sprintf(buf, "\nWaiting for Opponent\n\n");
 
                        write(p->fd, buf, strlen(buf));
 
                        send_menu(p->opp);
                    }
                    
                } else {
                    // Inactive player's turn or invalid command, discard
                    sprintf(outbuf, "\nIt's not your turn to attack.\n\n");
                    write(p->fd, outbuf, strlen(outbuf));
                }
            } else if (strncmp(buf, "s", strlen("s")) == 0) {
                // Player wants to say something
                if (p->isActive) {
                    char *message = buf + strlen("s") + 1; // Skip the "s" command and the following space
                    // Iterate through the message and replace newline characters with null terminators
                    char *newline = strchr(message, '\n');
                    if (newline != NULL) {
                        *newline = '\0';
                    }
                    // Prepare the message to be sent
                    char outbuf[MAX_BUF];
                    sprintf(outbuf, "\n%s says: %s\n\n", p->name, message);
                    // Send the message to both players
                    write(p->fd, outbuf, strlen(outbuf));
                    write(p->opp->fd, outbuf, strlen(outbuf));
                    } 
                else {
                    // Inactive player's turn or invalid command, discard
                    sprintf(outbuf, "\nIt's not your turn to talk.\n\n");
                    write(p->fd, outbuf, strlen(outbuf));
                }
 
 
                } 
                else {
                // Invalid command, discard
                sprintf(outbuf, "\nInvalid command.\n\n");
                write(p->fd, outbuf, strlen(outbuf));
                }
        }
        // Send the menu of valid commands to the active player
        // if (p == active_player) {
        //     sprintf(outbuf, "Menu:\n(a) Attack\n(p) Powermove\n(s) Say something\n");
        //     write(p->fd, outbuf, strlen(outbuf));
        // }
        if (p!=NULL && p->isActive){
            send_menu(p);
        }
        return 0;
        } else  { 
        printf("\nDisconnect from %s\n\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "\nGoodbye %s\r\n\n", inet_ntoa(p->ipaddr));
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
    
    // Initialize the name to be empty
    p->name[0] = '\0';  
 
    int nbytes;
    char ch;
    int name_len = 0;
    
    // Read the client's name character by character
    while ((nbytes = read(fd, &ch, 1)) > 0) {
        // Stop reading if we hit newline or if the name is about to overflow
        if (ch == '\n' || name_len >= (MAX_NAME - 1)) {
            break;
        }
        // Append character to name
        p->name[name_len++] = ch;
    }
 
    // Null-terminate the name
    p->name[name_len] = '\0';
 
    if (nbytes <= 0) {
        // If read fails or no characters were read, don't add the client
        free(p);
        return top;
    }
    // char ch;
    // char name[MAX_NAME]; // Assuming name is defined similarly
    // int buffer_index = 0;
    // //struct client *p; // Assuming p is properly initialized somewhere
 
    // while (1) {
    //     read(STDIN_FILENO, &ch, 1); // Read character by character
 
    //     if (ch == '\n') {
    //         name[buffer_index] = '\0';  // Correctly null-terminate the name before resetting index
 
    //         // Assume p is a valid pointer to a client structure
    //         strncpy(p->name, name, sizeof(p->name) - 1);
    //         p->name[sizeof(p->name) - 1] = '\0';  // Ensure null-termination
 
    //         // Reset the buffer
    //         memset(name, 0, sizeof(name));
    //         printf("NAME: %s\n", p->name); // Added a newline for clarity
    //         break; // Exit the loop
    //     } else {
    //         // Append character to buffer
    //         if (buffer_index < sizeof(name) - 1) {
    //             name[buffer_index++] = ch;
    //         }
    //     }
    // }
 
    printf("\nAdding client %s\n", inet_ntoa(addr));
 
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->status = IN_LOBBY;
    p->opp = NULL;
    p->prev_opp = NULL;
    top = p;
 
    
 
    char message[MAX_NAME+strlen(" has entered the arena")];
    sprintf(message, "\n%s has entered the arena\n\n", p->name);
    enterLobby(p);
    
    broadcast(top, message, strlen(message), fd);
 
    // Attempt to match clients
    //match_clients(top);
 
    return top;
}
 
void enterLobby(struct client * p){
 
    char buf[MAX_BUF];
 
    sprintf(buf, "\nWelcome to the Arena, please press m to join random matchmaking, l to list players in lobby and p with player number to play aginst friend\r\n\n");
    write(p->fd,buf,strlen(buf));
 
}
 
struct client *removeclient(struct client *top, int fd) {
    struct client **p;
 
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "\nRemoving client %d %s\n\n", fd, inet_ntoa((*p)->ipaddr));
 
        if ((*p)->status == BATTLE && (*p)->opp != NULL) {
            //(*p)->opp->in_match = 0; // Clear the in_match flag for the opponent
            //(*p)->opp->opp = NULL; // Clear the in_match flag for the opponent
            // Correctly send the message to the opponent using their fd
            write((*p)->opp->fd, "Opponent Disconnected, Going back to arena\n", strlen("Opponent Disconnected, Going back to arena\n"));
            
            (*p)->opp->prev_opp = NULL;
            (*p)->opp->status = IN_LOBBY;
            (*p)->opp->opp = NULL;
            (*p)->opp->isActive = 0;
 
            enterLobby((*p)->opp);
        }
 
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "\nTrying to remove fd %d, but I don't know about it\n\n",
                 fd);
    }
    return top;
}
 
void broadcast(struct client *top, char *s, int size, int exclude_fd) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != exclude_fd && p->status == BATTLE) { 
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
 
        match_clients(head);
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = SECONDS;
        tv.tv_usec = 0;  /* and microseconds */
 
        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("\nNo response from clients in %d seconds\n\n", SECONDS);
            for (p = head; p != NULL; p = p->next){
                if (p->status == 1 ){
                    printf("\n%s in lobby\n\n", p->name);
                }
            }
            continue;
        }
 
        if (nready == -1) {
            perror("select");
            continue;
        }
 
        if (FD_ISSET(listenfd, &rset)){
            printf("\na new client is connecting\n\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            
 
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("\nconnection from %s\n\n", inet_ntoa(q.sin_addr));
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
