#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../include/list.h"
#include "../include/thread.h"

#define MYPORT 1218 // the port users will be connecting to
#define MAXBUFLEN 100

static List *messageList;
static pthread_mutex_t listMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t messageReadyCond = PTHREAD_COND_INITIALIZER;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);
void* inputThread(void* arg);
void* receiveMessages(void* arg);
void* sendMessages(void* arg);
void* displayThread(void* arg);


int main(int argc, char *argv[]){
    if(argc != 4){
        exit(1);
    }
    
    const char* myPort = argv[1];
    const char* remoteHostName = argv[2];
    const char* remotePort = argv[3];

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    messageList = List_create();
    if (messageList == NULL) { 
        perror("Failed to create message List");
        exit(EXIT_FAILURE);
    }

    // int numbytes;
    // struct sockaddr_storage their_addr;
    // char buf[MAXBUFLEN];
    // socklen_t addr_len; 
    // char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // either IPv4 or 6
    hints.ai_socktype = SOCK_DGRAM; // Datagram
    // hints.ai_flag = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, myPort, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    /// receiving part??
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
    pthread_t recvThread, sendThread, inputThreadId, displayThreadId;
    thread_args_t commonArgs; // Assuming all threads can use a structure with common args
    commonArgs.socket_fd = sockfd;
    commonArgs.message_list = messageList;
    commonArgs.input_mutex = &listMutex;
    commonArgs.message_ready_cond = &messageReadyCond;
    strncpy(commonArgs.remote_hostname, remoteHostName, sizeof(commonArgs.remote_hostname));
    strncpy(commonArgs.remote_port, remotePort, sizeof(commonArgs.remote_port));

    // Create threads
    pthread_create(&recvThread, NULL, receiveMessages, &commonArgs);
    pthread_create(&sendThread, NULL, sendMessages, &commonArgs);
    pthread_create(&inputThreadId, NULL, inputThread, &commonArgs);
    pthread_create(&displayThreadId, NULL, displayThread, &commonArgs);

    // Wait for threads to finish
    pthread_join(recvThread, NULL);
    pthread_join(sendThread, NULL);
    pthread_join(inputThreadId, NULL);
    pthread_join(displayThreadId, NULL);

    // Cleanup
    pthread_mutex_destroy(&listMutex);
    pthread_cond_destroy(&messageReadyCond);
    List_free(messageList, free); // Assumes free is an appropriate deallocation function

    close(sockfd);
    return 0;

}



// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* receiveMessages(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char buf[MAXBUFLEN];
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    while (1) {
        addr_len = sizeof their_addr;
        int numbytes = recvfrom(args->socket_fd, buf, MAXBUFLEN-1, 0, 
                                (struct sockaddr*)&their_addr, &addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            continue;
        }

        buf[numbytes] = '\0'; // NULL-terminator

        // lock the mutex before accessing list
        pthread_mutex_lock(args->input_mutex);
        List_append(args->message_list, strdup(buf)); // add message into list
        pthread_cond_signal(args->message_ready_cond); 
        pthread_mutex_unlock(args->input_mutex);
    }

    

    return NULL;
}



void* sendMessages(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(args->remote_hostname, args->remote_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    // Assuming you have a mechanism to fetch messages from the list
    // Example: pseudo-code, you need to implement actual fetching
    while (1) { // Replace with a proper condition to exit
        char* message; // Assume you fetch this from the shared list

        pthread_mutex_lock(args->input_mutex);
        if (List_count(args->message_list) > 0) {
            message = (char*)List_trim(args->message_list);
        } else {
            pthread_cond_wait(args->message_ready_cond, args->input_mutex);
            pthread_mutex_unlock(args->input_mutex);
            continue;
        }
        pthread_mutex_unlock(args->input_mutex);

        // Send the message using the first result
        if ((rv = sendto(args->socket_fd, message, strlen(message), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
            perror("talker: sendto");
        }

        free(message); // Remember to free the fetched message
    }

    freeaddrinfo(servinfo);
    return NULL;
}



// Global or shared flag to indicate program should terminate
volatile int shouldTerminate = 0;

void* inputThread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char inputBuf[1024]; // Adjust size as necessary

    while (!shouldTerminate) {
        if (fgets(inputBuf, sizeof(inputBuf), stdin) == NULL) {
            // Handle error or EOF
            break;
        }
        if (strcmp(inputBuf, "!\n") == 0) {
            shouldTerminate = 1; // Signal termination
            pthread_cond_broadcast(args->message_ready_cond); // Wake up any waiting threads
            break;
        }

        // Lock mutex and add input to shared list
        pthread_mutex_lock(args->input_mutex);
        List_append(args->message_list, strdup(inputBuf));
        pthread_cond_signal(args->message_ready_cond);
        pthread_mutex_unlock(args->input_mutex);
    }
    return NULL;
}


void* displayThread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

    while (!shouldTerminate) {
        pthread_mutex_lock(args->input_mutex);
        // Wait for a message to be available in the list
        while (List_count(args->message_list) == 0 && !shouldTerminate) {
            pthread_cond_wait(args->message_ready_cond, args->input_mutex);
        }

        if (shouldTerminate) {
            pthread_mutex_unlock(args->input_mutex);
            break; // Exit loop if termination signal is received
        }

        // Retrieve the message from the list
        void* message = List_trim(args->message_list); // Assuming FIFO; adjust as needed

        pthread_mutex_unlock(args->input_mutex);

        if (message != NULL) {
            // Display the message to the user
            printf("Received: %s\n", (char*)message);
            free(message); // Assuming dynamic allocation; adjust as needed
        }
    }

    return NULL;
}
