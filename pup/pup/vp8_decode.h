//
//  vp8_decode.h
//  pup
//
//  Created by Charley Robinson on 3/26/17.
//
//

#ifndef vp8_decode_h
#define vp8_decode_h

struct vp8_decode;

void vp8_decode_alloc(struct vp8_decode** decoder_out);
void vp8_decode_free(struct vp8_decode*);

#endif /* vp8_decode_h */
