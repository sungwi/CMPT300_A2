/*
    Simon Fraser University, CMPT300 Assignment#2
    Name: Sungwi Kim
    ID: 301592065
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define __USE_XOPEN2K
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../include/list.h"
#include "../include/thread.h"

#define MAXBUFLEN 256

static List *sendList;
static List *recList;

static pthread_mutex_t sendListMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t recListMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t messageReadyCond = PTHREAD_COND_INITIALIZER;

void *get_in_addr(struct sockaddr *sa);
void* keyboardInputThread(void* arg);
void* udpOutputThread(void* arg);
void* udpInputThread(void* arg);
void* screenOutputThread(void* arg);

void* receiveAndDisplayMessages(void* arg);
void* inputAndSendMessages(void* arg);

int main(int argc, char *argv[]){
    if(argc != 4){
        exit(1);
    }
    
    const char* myPort = argv[1];
    const char* remoteHostName = argv[2];
    const char* remotePort = argv[3];
    printf("\nMy Port Number: %s\n", myPort);
    printf("Remote Host Name: %s\n", remoteHostName);
    printf("Remote Port Number: %s\n\n", remotePort);

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    // define shared lists
    sendList = List_create();
    recList = List_create();
    if (sendList == NULL || recList == NULL) { 
        perror("Failed to create message List");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // either IPv4 or 6
    hints.ai_socktype = SOCK_DGRAM; // Datagram
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, myPort, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    /// binding part
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("s-talk: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("s-talk: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "s-talk: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);


    // Initialize thread arguments
    pthread_t keyInputThr, udpOutputThr, udpInputThr,displayThr;
    thread_args_t commonArgs; 
    commonArgs.socket_fd = sockfd;
    commonArgs.send_list = sendList;
    commonArgs.rec_list = recList;
    commonArgs.sendList_mutex = &sendListMutex;
    commonArgs.recList_mutex = &recListMutex;
    commonArgs.message_ready_cond = &messageReadyCond;
    strncpy(commonArgs.remote_hostname, remoteHostName, sizeof(commonArgs.remote_hostname));
    strncpy(commonArgs.remote_port, remotePort, sizeof(commonArgs.remote_port));

    // Create threads
    pthread_create(&keyInputThr, NULL, keyboardInputThread, &commonArgs);
    pthread_create(&udpOutputThr, NULL, udpOutputThread, &commonArgs);
    pthread_create(&udpInputThr, NULL, udpInputThread, &commonArgs);
    pthread_create(&displayThr, NULL, screenOutputThread, &commonArgs);

    // Wait for threads to finish
    pthread_join(keyInputThr, NULL);
    pthread_join(udpOutputThr, NULL);
    pthread_join(udpInputThr, NULL);
    pthread_join(displayThr, NULL);

    // Cleanup
    pthread_mutex_destroy(&sendListMutex);
    pthread_mutex_destroy(&recListMutex);
    pthread_cond_destroy(&messageReadyCond);
    List_free(sendList, free); 
    List_free(recList, free);

    close(sockfd);
    return 0;

}



volatile int shouldTerminate = 0;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* keyboardInputThread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char inputBuf[1024];

    while (!shouldTerminate) {
        if (fgets(inputBuf, sizeof(inputBuf), stdin) == NULL) {
            continue;
        }
        pthread_mutex_lock(args->sendList_mutex);
        List_append(args->send_list, strdup(inputBuf));
        pthread_mutex_unlock(args->sendList_mutex);

    }
    return NULL;
}

void* udpOutputThread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_DGRAM; 

    // Get address info for remote host and port
    if ((rv = getaddrinfo(args->remote_hostname, args->remote_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    while (!shouldTerminate) {
        pthread_mutex_lock(args->sendList_mutex);
        if (List_count(args->send_list) > 0) {
            char* message = (char*)List_first(args->send_list); // get message from send list
            List_remove(args->send_list); 
            pthread_mutex_unlock(args->sendList_mutex);

            // Iterate through all the results and send the message to the first we can
            for (p = servinfo; p != NULL; p = p->ai_next) {
                if ((rv = sendto(args->socket_fd, message, strlen(message), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                    perror("talker: sendto");
                    continue;
                }
                break; // sent successfully
            }
            if(strcmp(message, "!\n") == 0){ // terminate program
                printf("Ternimation Command [\"!\"] Implemented. Session Ends. \n");
            }
            free(message); 
        } else {
            pthread_mutex_unlock(args->sendList_mutex);
        }
    }

    freeaddrinfo(servinfo);

    return NULL;
}


void* udpInputThread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char buf[MAXBUFLEN];
    struct sockaddr_storage their_addr;
    socklen_t addr_len;

    while (!shouldTerminate) {
        addr_len = sizeof their_addr;
        int numbytes = recvfrom(args->socket_fd, buf, MAXBUFLEN-1, 0, (struct sockaddr*)&their_addr, &addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            continue;
        }
        if (numbytes > 0) {
            buf[numbytes] = '\0';
            pthread_mutex_lock(args->recList_mutex);
            List_append(args->rec_list, strdup(buf)); // add message to rec list
            pthread_cond_signal(args->message_ready_cond); // condition var
            pthread_mutex_unlock(args->recList_mutex);
        }
    }
    return NULL;
}

void* screenOutputThread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

    while (!shouldTerminate) {
        pthread_mutex_lock(args->recList_mutex);
        while (List_count(args->rec_list) == 0 && !shouldTerminate) {
            pthread_cond_wait(args->message_ready_cond, args->recList_mutex); // Wait for new message
        }
        if (List_count(args->rec_list) > 0) {
            char* message = (char*)List_first(args->rec_list); // get message from rec list, which is added by udpInputThread func
            List_remove(args->rec_list); 
            pthread_mutex_unlock(args->recList_mutex);
            if(strcmp(message, "!\n") == 0){ // terminate program
                shouldTerminate = 1; // Signal termination
                printf("Ternimation Command [\"!\"] Implemented by other client. Session Ends. \n");
            }
            else{
                printf("Received: %s\n", message);
                free(message);
            }
        } else {
            pthread_mutex_unlock(args->recList_mutex);
        }
    }
    return NULL;
}

