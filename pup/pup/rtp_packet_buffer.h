//
//  rtp_packet_buffer.h
//  pup
//
//  Created by Charley Robinson on 3/13/17.
//
//

#ifndef rtp_packet_buffer_h
#define rtp_packet_buffer_h

#include <re/re.h>

struct rtp_packet_s {
    struct mbuf* packet;
    struct rtp_header header;
};

struct rtp_buffer_s;

/* return true to break the walk, false to continue */
typedef bool (rtp_buffer_walk_h)(struct rtp_packet_s*, void *arg);
void rtp_buffer_walk(struct rtp_buffer_s* buffer, rtp_buffer_walk_h* cb,
                     void *arg);

int rtp_buffer_alloc(struct rtp_buffer_s** buffer_out);
void rtp_buffer_free(struct rtp_buffer_s* buffer);

struct rtp_packet_s* rtp_buffer_pop(struct rtp_buffer_s* buffer);
void rtp_buffer_push(struct rtp_buffer_s* buffer,
                     struct rtp_packet_s* packet);

struct rtp_packet_s* rtp_packet_from_buffer(struct mbuf* buffer);
void rtp_packet_free(struct rtp_packet_s* packet);

#endif /* rtp_packet_buffer_h */
