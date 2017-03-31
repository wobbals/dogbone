//
//  opus_parser.h
//  pup
//
//  Created by Charley Robinson on 3/30/17.
//
//

#ifndef opus_parser_h
#define opus_parser_h

#include <re/re.h>
#include <libavcodec/avcodec.h>

/**
 * Turns opus RTP packets into AVPacket instances. Copy-free: AVPackets are
 * backed by the rtp mbuf passed in.
 */
void opus_parse(struct mbuf* rtp,
                struct rtp_header* header,
                AVPacket* packet);

#endif /* opus_parser_h */
