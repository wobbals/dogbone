//
//  vp8_depacketizer.h
//  pup
//
//  Created by Charley Robinson on 3/13/17.
//
//

#ifndef vp8_depacketizer_h
#define vp8_depacketizer_h

#include <libavcodec/avcodec.h>
#include <re/re.h>

struct vp8_depacketizer_s;

int vp8_depacketizer_alloc(struct vp8_depacketizer_s** depacketizer_out);
void vp8_depacketizer_free(struct vp8_depacketizer_s* depacketizer);

void vp8_depacketizer_push(struct vp8_depacketizer_s* depacketizer,
                           struct mbuf* packet);
AVPacket* vp8_depacketizer_pop(struct vp8_depacketizer_s* depacketizer);


#endif /* vp8_depacketizer_h */
