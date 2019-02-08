#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "server.h"
#include "directory.h"
#include "thread_counter.h"

#include "csapp.h"

static void terminate();
static void *thread();
THREAD_COUNTER *thread_counter;
int listenFD;

int main(int argc, char* argv[]) {
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the thread counter and directory.
    thread_counter = tcnt_init();
    dir_init();

    int *connfd;
    int portNum = 0;
    char option;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    for (int i = 0; optind < argc; i++){
        if ((option = getopt(argc, argv, "+h:p:q")) != -1) {
            switch (option) {
                // hostname flag
                case 'h': {
                    break;
                }
                // port # flag
                case 'p': {
                    char *num = optarg;
                    //one digit port
                    portNum = (*num) - '0';
                    int k = 1;
                    while(*(num + k) != 0){
                        portNum = portNum * 10;
                        portNum = portNum + *(num + k) - '0';
                        k++;
                    }
                    listenFD = Open_listenfd(optarg);

                    /*if (*(num + 1) == 0){
                        portNum = (*num) - '0';
                    }
                    // two digit port #
                    else if (*(num + 2) == 0){
                        portNum = (*num) - '0';
                        portNum = portNum * 10;
                        portNum = portNum + *(num + 1) - '0';
                    }*/

                    break;
                }
                // run without prompting
                case 'q': {

                    break;
                }
            }
        }
    }

    struct sigaction action, old_action;
    action.sa_handler = terminate;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(SIGHUP, &action, &old_action) < 0)
        unix_error("Signal error");

    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenFD, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function bvd_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.


    close(listenFD);
    terminate();
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int sig) {
    close(listenFD);
    // Shut down the directory.
    // This will trigger the eventual termination of service threads.
    dir_shutdown();

    debug("Waiting for service threads to terminate...");
    tcnt_wait_for_zero(thread_counter);
    debug("All service threads terminated.");

    tcnt_fini(thread_counter);
    dir_fini();
    exit(EXIT_SUCCESS);
}

void *thread(void *vargp) {

 //int connfd = *((int *)vargp);
 //Pthread_detach(pthread_self());
 //Free(vargp);

 bvd_client_service(vargp);
 //printf("thread made\n");
 //echo(connfd);
 //Close(connfd);
 return NULL;

}



