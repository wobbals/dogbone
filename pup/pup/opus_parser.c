//
//  opus_parser.c
//  pup
//
//  Created by Charley Robinson on 3/30/17.
//
//

#include "opus_parser.h"

void opus_parse(struct mbuf* rtp_packet,
                struct rtp_header* header,
                AVPacket* pkt)
{
  av_init_packet(pkt);
  pkt->pts = header->ts;
  pkt->dts = pkt->pts;
  pkt->buf = NULL;
  pkt->data = &(rtp_packet->buf[rtp_packet->pos]);
  pkt->size = (int)(rtp_packet->end - rtp_packet->pos);
}
