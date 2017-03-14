//
//  rtp_packet_buffer.cc
//  pup
//
//  Created by Charley Robinson on 3/13/17.
//
//
extern "C" {
#include <stdlib.h>
#include "rtp_packet_buffer.h"
}

#include <queue>

struct rtp_buffer_s {
  std::vector<struct rtp_packet_s*> fifo;
};

int rtp_buffer_alloc(struct rtp_buffer_s** buffer_out) {
  struct rtp_buffer_s* buffer = (struct rtp_buffer_s*)
  calloc(1, sizeof(rtp_buffer_s));
  buffer->fifo = std::vector<struct rtp_packet_s*>();

  *buffer_out = buffer;
  return 0;
}

void rtp_buffer_free(struct rtp_buffer_s* buffer) {
  while (!buffer->fifo.empty()) {
    struct rtp_packet_s* packet = buffer->fifo.front();
    buffer->fifo.erase(buffer->fifo.begin());
    free(packet);
  }
  free(buffer);
}

struct rtp_packet_s* rtp_buffer_pop(struct rtp_buffer_s* buffer) {
  if (buffer->fifo.empty()) {
    return NULL;
  }
  struct rtp_packet_s* val = buffer->fifo.front();
  buffer->fifo.erase(buffer->fifo.begin());
  return val;
}

void rtp_buffer_push(struct rtp_buffer_s* buffer,
                     struct rtp_packet_s* packet)
{
  buffer->fifo.push_back(packet);
}

void rtp_buffer_walk(struct rtp_buffer_s* buffer, rtp_buffer_walk_h* cb,
                     void *arg)
{
  for (struct rtp_packet_s* packet : buffer->fifo) {
    if (cb(packet, arg)) {
      return;
    }
  }
}

struct rtp_packet_s* rtp_packet_from_buffer(struct mbuf* buffer) {
  struct rtp_packet_s* packet = (struct rtp_packet_s*)
  calloc(1, sizeof(struct rtp_packet_s));
  packet->packet = buffer;
  rtp_hdr_decode(&packet->header, buffer);
  return packet;
}

void rtp_packet_free(struct rtp_packet_s* packet) {
  mem_deref(packet->packet);
  free(packet);
}
