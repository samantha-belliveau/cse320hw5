#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#include "protocol.h"
#include "debug.h"


/*
 * The msgid field in the packet header should contain a
 * client-generated value that uniquely identifies a particular
 * message.  The client may use any convenient technique to produce
 * the value stored in this field.  Possibilities are: message
 * sequence numbers, timestamps, and hashes of message content.  These
 * unique identifiers are not used by the bavarde server; they are
 * simply passed through in order to permit a client to associate
 * notifications from the server (such as delivery and nondelivery
 * notifications) with the messages to which they pertain.
 *
 * The actual body of a message is sent in the variable-length payload
 * part of a packet, which immediately follows the fixed-length header
 * and which consists of exactly payload_length bytes of data.  The
 * first line of a message contains either the handle of the intended
 * receiver (in case of a message being sent), or the handle of the
 * sender (in case of a message being delivered).  This line is
 * terminated by the \r\n line termination sequence.  Following the
 * line termination sequence is the body of the message, which may be
 * in an arbitrary format (even binary).  The total number of bytes
 * contained in the first line of the message, plus the line
 * termination sequence, plus the message body must be exactly equal
 * to the payload_length specified in the packet header.
 *
 * Format of message sent by client:
 *   (handle of receiver)\r\n(message body)
 *
 * Format of message delivered to client:
 *   (handle of sender)\r\n(message body)
 *
 * In the case of a login request, the payload part of the packet
 * contains just the requested handle and the message body is omitted.
 */

/*
 * Send a packet with a specified header and payload.
 *   fd - file descriptor on which packet is to be sent
 *   hdr - the packet header, with multi-byte fields in host byte order
 *   payload - pointer to packet payload, or NULL, if none.
 *
 * Multi-byte fields in the header are converted to network byte order
 * before sending.  The header structure passed to this function may
 * be modified as a result.
 *
 * On success, 0 is returned.
 * On error, -1 is returned and errno is set.
 */
int proto_send_packet(int fd, bvd_packet_header *hdr, void *payload){

    int payloadL = hdr->payload_length;

    //hdr->type = htonl(hdr->type);
    hdr->payload_length = htonl(hdr->payload_length);
    hdr->msgid = htonl(hdr->msgid);
    hdr->timestamp_sec = htonl(hdr->timestamp_sec);
    hdr->timestamp_nsec = htonl(hdr->timestamp_nsec);

    // need to check for short counts
    if (write(fd, hdr, sizeof(bvd_packet_header)) != sizeof(bvd_packet_header)){
        return -1;
    }

    if (payloadL != 0){
        if (write(fd, payload, payloadL) != payloadL){
            return -1;
        }
    }

    return 0;

}


/*
 * Receive a packet, blocking until one is available.
 *  fd - file descriptor from which packet is to be received
 *  hdr - pointer to caller-supplied storage for packet header
 *  payload - variable into which to store payload pointer
 *
 * The returned header has its multi-byte fields in host byte order.
 *
 * If the returned payload pointer is non-NULL, then the caller
 * is responsible for freeing the storage.
 *
 * On success, 0 is returned.
 * On error, -1 is returned, payload and length are left unchanged,
 * and errno is set.
 */
int proto_recv_packet(int fd, bvd_packet_header *hdr, void **payload){


    if (read(fd, hdr, sizeof(bvd_packet_header)) != sizeof(bvd_packet_header)){
        return -1;
    }


   // hdr->type = ntohl(hdr->type);
    hdr->payload_length = ntohl(hdr->payload_length);
    hdr->msgid = ntohl(hdr->msgid);
    hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
    hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);

    void *p = calloc(sizeof(char), hdr->payload_length);

    if (hdr->payload_length > 0){

        if (read(fd, p, hdr->payload_length) != hdr->payload_length){
            return -1;
        }

    }


    *payload = p;


    return 0;

}
