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
//static pthread_mutex_t listMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sendListMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t recListMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t messageReadyCond = PTHREAD_COND_INITIALIZER;

void *get_in_addr(struct sockaddr *sa);
void* inputThread(void* arg);
void* receiveMessages(void* arg);
void* sendMessages(void* arg);
void* displayThread(void* arg);

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
    pthread_t recvThread, sendThread;
    thread_args_t commonArgs; 
    commonArgs.socket_fd = sockfd;
    commonArgs.send_list = sendList;
    commonArgs.rec_list = recList;
    //commonArgs.input_mutex = &listMutex;
    commonArgs.sendList_mutex = &sendListMutex;
    commonArgs.recList_mutex = &recListMutex;
    commonArgs.message_ready_cond = &messageReadyCond;
    strncpy(commonArgs.remote_hostname, remoteHostName, sizeof(commonArgs.remote_hostname));
    strncpy(commonArgs.remote_port, remotePort, sizeof(commonArgs.remote_port));

    // Create threads
    pthread_create(&recvThread, NULL, receiveAndDisplayMessages, &commonArgs);
    pthread_create(&sendThread, NULL, inputAndSendMessages, &commonArgs);

    // Wait for threads to finish
    pthread_join(recvThread, NULL);
    pthread_join(sendThread, NULL);

    // Cleanup
    //pthread_mutex_destroy(&listMutex);
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

void* receiveAndDisplayMessages(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char buf[MAXBUFLEN];
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    
    while (!shouldTerminate) {
            pthread_mutex_lock(args->recList_mutex);
            // Wait for a signal if the list is empty
            while (List_count(args->rec_list) == 0 && !shouldTerminate) {
                pthread_cond_wait(args->message_ready_cond, args->recList_mutex);
            }
            if (shouldTerminate) {
                pthread_mutex_unlock(args->recList_mutex);
                break;
            }

            char* message = (char*)List_first(args->rec_list);
            if (message != NULL) {
                List_remove(args->rec_list); // Assuming this removes and returns the first item
                pthread_mutex_unlock(args->recList_mutex);
                printf("Received: %s\n", message);
                free(message); // Free the message after displaying
            } else {
                pthread_mutex_unlock(args->recList_mutex);
            }
        }

        return NULL;
}

void* inputAndSendMessages(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char inputBuf[1024];
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(args->remote_hostname, args->remote_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }
    while (!shouldTerminate) {
        pthread_mutex_lock(args->sendList_mutex);
        // Assuming List_first returns NULL if the list is empty
        char* message = (char*)List_first(args->send_list);
        if (message != NULL) {
            List_remove(args->send_list); // Assuming this removes the first item
            pthread_mutex_unlock(args->sendList_mutex);

            // Iterate through all the results and send the message to the first we can
            for (p = servinfo; p != NULL; p = p->ai_next) {
                if (sendto(args->socket_fd, message, strlen(message), 0, p->ai_addr, p->ai_addrlen) == -1) {
                    perror("talker: sendto");
                    continue;
                }
                break; // Sent successfully
            }
            free(message); // Free the message after sending
        } else {
            pthread_mutex_unlock(args->sendList_mutex);
            // If the list is empty, wait for a signal
            pthread_cond_wait(args->message_ready_cond, args->sendList_mutex);
        }
    }

    freeaddrinfo(servinfo); // Ensure this is called to free network information
    return NULL;
}

