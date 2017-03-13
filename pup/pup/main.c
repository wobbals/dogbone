//
//  main.c
//  pup
//
//  Created by Charley Robinson on 3/11/17.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <re/re.h>
#include <zmq.h>

#include "peer_connection_dumper.h"

static int receive_msg(void* socket, struct mbuf* mbuf) {
    int ret = zmq_recv(socket, mbuf->buf, mbuf->size, 0);
    if (ret >= mbuf->size) {
        printf("Warning: message of length %d truncated\n", ret);
    } else {
        // null terminate the zmq message to give a c string
        mbuf->buf[ret] = '\0';
        mbuf->pos = 0;
        mbuf->end = ret + 1;
    }
    return ret;
}


int print_f (const char *p, size_t size, void *arg) {
    printf("re: %s\n", p);
    return 0;
}

int main(int argc, const char * argv[]) {
    struct re_printf print;
    print.vph = print_f;
    print.arg = NULL;

    //  Prepare our context and socket
    void *context = zmq_ctx_new ();
    void *receiver = zmq_socket (context, ZMQ_PULL);
    printf("sup\n");
    // connect to dogbone ipc
    int ret = zmq_connect (receiver, "ipc:///tmp/dogbone");
    if (ret < 0) {
        printf("failed to connect to dogbone. errno: %d", errno);
        return ret;
    }
    printf("connected to dogbone ipc\n");
    const size_t buf_size = 4096;
    struct mbuf* mbuf = mbuf_alloc(buf_size);
    ret = receive_msg(receiver, mbuf);
    struct rtp_header hdr;
    while (ret > 0) {
        printf("received packet len: %ld ", mbuf->end);
        rtp_hdr_decode(&hdr, mbuf);
        printf("ssrc: %d pt: %d seqno: %u ts: %u\n",
               hdr.ssrc, hdr.pt, hdr.seq, hdr.ts);
        ret = receive_msg(receiver, mbuf);
    }

    return 0;
}
