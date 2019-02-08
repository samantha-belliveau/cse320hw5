#include "directory.h"
#include "mailbox.h"

#include <pthread.h>
#include <string.h>
#include <sys/socket.h>





/*
 * The directory maintains a mapping from handles (i.e. user names)
 * to client info, which includes the client mailbox and the socket
 * file descriptor over which commands from the client are received.
 * The directory is used when sending a message, in order to find
 * the destination mailbox that corresponds to a particular recipient
 * specified by their handle.
 */


typedef struct {
    int fd;
    char *handle;
    MAILBOX *mailbox;
    void *prev;
    void *next;

} directorySlot;

directorySlot *directory;

pthread_mutex_t mutex;


/*
 * Initialize the directory.
 */
void dir_init(void){

    directory = NULL;
    // initiallize mutex
    pthread_mutex_init(&mutex, NULL);



}

/*
 * Shut down the directory.
 * This marks the directory as "defunct" and shuts down all the client sockets,
 * which triggers the eventual termination of all the server threads.
 */
void dir_shutdown(void){
    pthread_mutex_lock(&mutex);

    directorySlot *temp = directory;
    directorySlot *next = directory;
    while (temp != NULL){
        free(temp->handle);
        mb_unref(temp->mailbox);
        shutdown(temp->fd, SHUT_RDWR);
        next = temp->next;
        free(temp);
        temp = next;
    }

    pthread_mutex_unlock(&mutex);

}

/*
 * Finalize the directory.
 *
 * Precondition: the directory must previously have been shut down
 * by a call to dir_shutdown().
 */
void dir_fini(void){
    pthread_mutex_destroy(&mutex);

}

/*
 * Register a handle in the directory.
 *   handle - the handle to register
 *   sockfd - file descriptor of client socket
 *
 * Returns a new mailbox, if handle was not previously registered.
 * Returns NULL if handle was already registered or if the directory is defunct.
 */
MAILBOX *dir_register(char *handle, int sockfd){

    pthread_mutex_lock(&mutex);
    directorySlot *newSlot;

    if (directory == NULL){
        newSlot = malloc(sizeof(directorySlot));

        char *newHandle = malloc(sizeof(char));
        strcpy(newHandle, handle);
        newSlot->handle = newHandle;

        newSlot->fd = sockfd;
        newSlot->mailbox = mb_init(handle);
        mb_ref(newSlot->mailbox);
        newSlot->next = NULL;
        newSlot->prev = NULL;

        directory = newSlot;
        pthread_mutex_unlock(&mutex);
        return newSlot->mailbox;
    }
    else{
        directorySlot *temp = directory;
        int found = 0;
        while (temp != NULL){
            if (strcmp(temp->handle, handle) == 0){
                // found a slot with this handle
                found = 1;
                break;
            }
            else{
                temp = temp->next;
            }
        }
        if (found == 0){
            newSlot = malloc(sizeof(directorySlot));

            char *newHandle = malloc(sizeof(char));
            strcpy(newHandle, handle);
            newSlot->handle = newHandle;
            newSlot->fd = sockfd;
            newSlot->mailbox = mb_init(handle);
            mb_ref(newSlot->mailbox);
            newSlot->next = directory;
            newSlot->prev = NULL;

            directory->prev = newSlot;
            directory = newSlot;
            pthread_mutex_unlock(&mutex);
            return newSlot->mailbox;
        }
        else{
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
    return NULL;


}

/*
 * Unregister a handle in the directory.
 * The associated mailbox is removed from the directory and shut down.
 */
void dir_unregister(char *handle){
    pthread_mutex_lock(&mutex);

    directorySlot *temp = directory;

    while (temp != NULL){
        if (strcmp(temp->handle, handle) == 0){
            if (temp->prev != NULL){
                ((directorySlot *)temp->prev)->next = temp->next;
            }
            if (temp->next != NULL){
                ((directorySlot *)temp->next)->prev = temp->prev;
            }
            if (temp == directory){
                directory = temp->next;
            }
            free(temp->handle);
            mb_unref(temp->mailbox);
            shutdown(temp->fd, SHUT_RDWR);
            free(temp);
            break;
        }
        else{
            temp = temp->next;
        }
    }

    pthread_mutex_unlock(&mutex);

}

/*
 * Query the directory for a specified handle.
 * If the handle is not registered, NULL is returned.
 * If the handle is registered, the corresponding mailbox is returned.
 * The reference count of the mailbox is increased to account for the
 * pointer that is being returned.  It is the caller's responsibility
 * to decrease the reference count when the pointer is ultimately discarded.
 */
MAILBOX *dir_lookup(char *handle){
    pthread_mutex_lock(&mutex);

    directorySlot *temp = directory;

    while (temp != NULL){
        if (strcmp(temp->handle, handle) == 0){
            // found a slot with this handle
            mb_ref(temp->mailbox);
            pthread_mutex_unlock(&mutex);
            return temp->mailbox;
        }
        else{
            temp = temp->next;
        }
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

/*
 * Obtain a list of all handles currently registered in the directory.
 * Returns a NULL-terminated array of strings.
 * It is the caller's responsibility to free the array and all the strings
 * that it contains.
 */
char **dir_all_handles(void){
    pthread_mutex_lock(&mutex);

    directorySlot *temp = directory;

    char **list = malloc(sizeof(char *));
    char **tempList = list;
    while (temp != NULL){
        *tempList = malloc(sizeof(char));
        strcpy(*tempList, temp->handle);
        tempList = tempList +1;
        temp = temp->next;
    }
    *tempList = NULL;
    pthread_mutex_unlock(&mutex);

    return list;
}