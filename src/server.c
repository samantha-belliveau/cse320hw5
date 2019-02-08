
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "protocol.h"
#include "directory.h"
#include "thread_counter.h"
#include "server.h"
#include "csapp.h"

/*
 * Thread function for the thread that handles client requests.
 *
 * The arg pointer point to the file descriptor of client connection.
 * This pointer must be freed after the file descriptor has been
 * retrieved.
 */
THREAD_COUNTER *thread_counter;
int msgid = 0;


typedef struct entryQueue {
    MAILBOX_ENTRY *this;
    void *next;
    void *prev;
}QUEUE;

struct mailbox {
    QUEUE *queue;
    char *handle;
    sem_t semaphore;
    int refCount;
    int defunct;
    MAILBOX_DISCARD_HOOK *hook;

};

void *bvd_client_service(void *arg){
    tcnt_incr(thread_counter);
    pthread_t tid;


    int fd = *(int *)arg;
    free(arg);
    bvd_packet_header *hdr = malloc(sizeof(bvd_packet_header));
    void **payload = malloc(sizeof(void *));
    int signIn = 0;
    struct timespec *time = malloc(sizeof(struct timespec));

    MAILBOX *mb;

    bvd_packet_header *hdrSend = malloc(sizeof(bvd_packet_header));
    //void **payloadSend = malloc(sizeof(void *));
    while(1){
        if (proto_recv_packet(fd, hdr, payload) == 0){
            if (signIn == 0){
                if (hdr->type == BVD_SEND_PKT){
                    hdrSend->type = BVD_NACK_PKT;
                    hdrSend->payload_length = 0;
                    hdrSend->msgid = msgid;
                    msgid++;
                    clock_gettime(CLOCK_REALTIME, time);
                    hdrSend->timestamp_sec = time->tv_sec;
                    hdrSend->timestamp_nsec = time->tv_nsec;

                    proto_send_packet(fd, hdrSend, NULL);
                }
                if (hdr->type == BVD_LOGOUT_PKT){
                    close(fd);
                    free(payload);
                    free(time);
                    free(hdrSend);
                    free(hdr);

                    tcnt_decr(thread_counter);

                    break;
                }
                if (hdr->type == BVD_USERS_PKT){
                    char **list = dir_all_handles();
                    char *users = NULL;
                    int i = 0;
                    if (*list != NULL){
                        users = *list;
                        list++;
                        while (*list != NULL){
                            strcat(users, *list);
                            list++;
                        }
                        i = 0;
                        char *temp = users;
                        for (i = 0; *temp; i++){
                            temp++;
                        }
                    }

                    hdrSend->type = BVD_ACK_PKT;
                    clock_gettime(CLOCK_REALTIME, time);

                    hdrSend->timestamp_sec = time->tv_sec;
                    hdrSend->timestamp_nsec = time->tv_nsec;

                    hdrSend->payload_length = i;
                    hdrSend->msgid = msgid;
                    msgid++;
                    proto_send_packet(fd, hdrSend, users);



                }
                if (hdr->type == BVD_LOGIN_PKT){
                    char *temp = *payload;
                    for (; *temp; temp++){

                    }
                    mb = dir_register(*payload, fd);
                    if (mb == NULL){
                        hdrSend->type = BVD_NACK_PKT;
                        clock_gettime(CLOCK_REALTIME, time);

                        hdrSend->timestamp_sec = time->tv_sec;
                        hdrSend->timestamp_nsec = time->tv_nsec;

                        hdrSend->payload_length = 0;
                        hdrSend->msgid = msgid;
                        msgid++;
                        proto_send_packet(fd, hdrSend, NULL);
                        continue;
                    }
                    struct fd_and_mb *input = malloc(sizeof(struct fd_and_mb));
                    input->fd = fd;
                    input->mb = mb;
                    signIn = 1;
                    Pthread_create(&tid, NULL, bvd_mailbox_service, input);


                }
            }
            else{
                if (hdr->type == BVD_LOGOUT_PKT){
                    char *handle = mb_get_handle(mb);
                    dir_unregister(handle);

                    mb_unref(mb);
                    mb_unref(mb);


                    close(fd);

                    free(payload);
                    free(time);
                    free(hdrSend);
                    free(hdr);

                    tcnt_decr(thread_counter);

                    break;
                }
                if (hdr->type == BVD_USERS_PKT){
                    char **list = dir_all_handles();
                    char *users = *list;
                    list++;
                    while (*list != NULL){
                        strcat(users, *list);
                        list++;
                    }
                    int i = 0;
                    char *temp = users;
                    for (i = 0; *temp; i++){
                        temp++;
                    }

                    NOTICE_TYPE type = ACK_NOTICE_TYPE;
                    mb_add_notice(mb, type, msgid, users, i);
                    msgid++;


                }
                if (hdr->type == BVD_SEND_PKT){

                    int i = 0;
                    char *dest = malloc(sizeof(char));
                    strcpy(dest, *payload);
                    char *temp = dest;
                    for (i = 0; *temp != '\n'; i++){
                        temp++;
                        (*payload)++;
                    }
                    (*payload)++;
                    *temp = '\n';
                    *(temp + 1) = 0;
                    temp++;
                    temp++;
                    for (i = 1; *temp; i++){
                        temp++;
                    }

                    MAILBOX *destmb = dir_lookup(dest);
                    if (destmb == NULL){
                        NOTICE_TYPE type = NACK_NOTICE_TYPE;
                        mb_add_notice(mb, type, msgid, NULL, 0);
                        msgid++;
                        free(dest);
                        continue;
                    }
                    NOTICE_TYPE type = ACK_NOTICE_TYPE;
                    mb_add_notice(mb, type, msgid, NULL, 0);
                    msgid++;

                    free(dest);

                    char *body = malloc(sizeof(char));
                    strcpy(body, *payload);
                    mb_add_message(destmb, msgid, mb, body, i);
                    msgid++;

                    type = RRCPT_NOTICE_TYPE;
                    mb_add_notice(mb, type, msgid, NULL, 0);
                    msgid++;

                }
            }

        }


    }
    return NULL;

}


/*
 * Once the file descriptor and mailbox have been retrieved,
 * this structure must be freed.
 */
void *bvd_mailbox_service(void *arg){
    tcnt_incr(thread_counter);


    MAILBOX *mb = ((struct fd_and_mb *)arg)->mb;
    int fd = ((struct fd_and_mb *)arg)->fd;

    free(arg);
    struct timespec *time = malloc(sizeof(struct timespec));

    bvd_packet_header *hdrSend = malloc(sizeof(bvd_packet_header));

    hdrSend->type = BVD_ACK_PKT;
    clock_gettime(CLOCK_REALTIME, time);

    hdrSend->timestamp_sec = time->tv_sec;
    hdrSend->timestamp_nsec = time->tv_nsec;
    hdrSend->payload_length = 0;
    hdrSend->msgid = msgid;
    msgid++;

    proto_send_packet(fd, hdrSend, NULL);

    while(1){
        MAILBOX_ENTRY *entry = mb_next_entry(mb);
        if (entry == NULL){
            free(time);
            free(hdrSend);
            tcnt_decr(thread_counter);
            break;
        }
        if (entry->type == NOTICE_ENTRY_TYPE){
            if (((entry->content).notice).type == ACK_NOTICE_TYPE){
                hdrSend->type = BVD_ACK_PKT;
            }
            if (((entry->content).notice).type == NACK_NOTICE_TYPE){
                hdrSend->type = BVD_NACK_PKT;
            }
            if (((entry->content).notice).type == RRCPT_NOTICE_TYPE){
                hdrSend->type = BVD_RRCPT_PKT;
            }
            clock_gettime(CLOCK_REALTIME, time);

            hdrSend->timestamp_sec = time->tv_sec;
            hdrSend->timestamp_nsec = time->tv_nsec;
            hdrSend->payload_length = entry->length;
            hdrSend->msgid = ((entry->content).notice).msgid;
            proto_send_packet(fd, hdrSend, entry->body);

            //free(&(entry->content).notice);
            //free(entry);

        }
        else{
            hdrSend->type = BVD_DLVR_PKT;
            clock_gettime(CLOCK_REALTIME, time);

            hdrSend->timestamp_sec = time->tv_sec;
            hdrSend->timestamp_nsec = time->tv_nsec;
            hdrSend->payload_length = entry->length;
            hdrSend->msgid = ((entry->content).notice).msgid;
            proto_send_packet(fd, hdrSend, entry->body);

            free(entry->body);


        }

    }

    return NULL;
}

