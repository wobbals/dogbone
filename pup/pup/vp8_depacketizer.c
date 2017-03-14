//
//  vp8_depacketizer.c
//  pup
//
//  Created by Charley Robinson on 3/13/17.
//
//

#include <stdlib.h>
#include <assert.h>
#include "vp8_depacketizer.h"
#include "rtp_packet_buffer.h"

enum FrameType {
  kEmptyFrame = 0,
  kAudioFrameSpeech = 1,
  kAudioFrameCN = 2,
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

const int16_t kNoPictureId = -1;
const int16_t kNoTl0PicIdx = -1;
const uint8_t kNoTemporalIdx = 0xFF;
const int kNoKeyIdx = -1;

struct RTPVideoHeaderVP8 {
    bool nonReference;          // Frame is discardable.
    int16_t pictureId;          // Picture ID index, 15 bits;
    // kNoPictureId if PictureID does not exist.
    int16_t tl0PicIdx;          // TL0PIC_IDX, 8 bits;
    // kNoTl0PicIdx means no value provided.
    uint8_t temporalIdx;        // Temporal layer index, or kNoTemporalIdx.
    bool layerSync;             // This frame is a layer sync frame.
    // Disabled if temporalIdx == kNoTemporalIdx.
    int keyIdx;                 // 5 bits; kNoKeyIdx means not used.
    int partitionId;            // VP8 partition ID
    bool beginningOfPartition;  // True if this packet is the first
    // in a VP8 partition. Otherwise false
};

struct ParsedPayload {
  const uint8_t* payload;
  size_t payload_length;
  enum FrameType frame_type;
  struct RTPVideoHeaderVP8 vp8_header;
};

int ParseVP8PictureID(struct RTPVideoHeaderVP8* vp8,
                      const uint8_t** data,
                      size_t* data_length,
                      size_t* parsed_bytes) {
  assert(vp8 != NULL);
  if (*data_length == 0)
    return -1;

  vp8->pictureId = (**data & 0x7F);
  if (**data & 0x80) {
    (*data)++;
    (*parsed_bytes)++;
    if (--(*data_length) == 0)
      return -1;
    // PictureId is 15 bits
    vp8->pictureId = (vp8->pictureId << 8) + **data;
  }
  (*data)++;
  (*parsed_bytes)++;
  (*data_length)--;
  return 0;
}

int ParseVP8Tl0PicIdx(struct RTPVideoHeaderVP8* vp8,
                      const uint8_t** data,
                      size_t* data_length,
                      size_t* parsed_bytes) {
  assert(vp8 != NULL);
  if (*data_length == 0)
    return -1;

  vp8->tl0PicIdx = **data;
  (*data)++;
  (*parsed_bytes)++;
  (*data_length)--;
  return 0;
}

int ParseVP8TIDAndKeyIdx(struct RTPVideoHeaderVP8* vp8,
                         const uint8_t** data,
                         size_t* data_length,
                         size_t* parsed_bytes,
                         bool has_tid,
                         bool has_key_idx) {
  assert(vp8 != NULL);
  if (*data_length == 0)
    return -1;

  if (has_tid) {
    vp8->temporalIdx = ((**data >> 6) & 0x03);
    vp8->layerSync = (**data & 0x20) ? true : false;  // Y bit
  }
  if (has_key_idx) {
    vp8->keyIdx = (**data & 0x1F);
  }
  (*data)++;
  (*parsed_bytes)++;
  (*data_length)--;
  return 0;
}

int64_t ParseVP8Extension(struct RTPVideoHeaderVP8* vp8,
                      const uint8_t* data,
                      size_t data_length) {
  assert(vp8 != NULL);
  assert(data_length > 0);
  size_t parsed_bytes = 0;
  // Optional X field is present.
  bool has_picture_id = (*data & 0x80) ? true : false;   // I bit
  bool has_tl0_pic_idx = (*data & 0x40) ? true : false;  // L bit
  bool has_tid = (*data & 0x20) ? true : false;          // T bit
  bool has_key_idx = (*data & 0x10) ? true : false;      // K bit

  // Advance data and decrease remaining payload size.
  data++;
  parsed_bytes++;
  data_length--;

  if (has_picture_id) {
    if (ParseVP8PictureID(vp8, &data, &data_length, &parsed_bytes) != 0) {
      return -1;
    }
  }

  if (has_tl0_pic_idx) {
    if (ParseVP8Tl0PicIdx(vp8, &data, &data_length, &parsed_bytes) != 0) {
      return -1;
    }
  }

  if (has_tid || has_key_idx) {
    if (ParseVP8TIDAndKeyIdx(
                             vp8, &data, &data_length, &parsed_bytes, has_tid, has_key_idx) !=
        0) {
      return -1;
    }
  }
  return parsed_bytes;
}

int ParseVP8FrameSize(struct ParsedPayload* parsed_payload,
                      const uint8_t* data,
                      size_t data_length) {
  assert(parsed_payload != NULL);
  if (parsed_payload->frame_type != kVideoFrameKey) {
    // Included in payload header for I-frames.
    return 0;
  }
  if (data_length < 10) {
    // For an I-frame we should always have the uncompressed VP8 header
    // in the beginning of the partition.
    return -1;
  }
  return 0;
}

size_t PictureIdLength(struct RTPVideoHeaderVP8* header) {
  if (header->pictureId == kNoPictureId) {
    return 0;
  }
  if (header->pictureId <= 0x7F) {
    return 1;
  }
  return 2;
}

bool TIDFieldPresent(struct RTPVideoHeaderVP8* header) {
  assert((header->layerSync == false) ||
         (header->temporalIdx != kNoTemporalIdx));
  return (header->temporalIdx != kNoTemporalIdx);
}

bool KeyIdxFieldPresent(struct RTPVideoHeaderVP8* header) {
  return (header->keyIdx != kNoKeyIdx);
}

bool TL0PicIdxFieldPresent(struct RTPVideoHeaderVP8* header) {
  return (header->tl0PicIdx != kNoTl0PicIdx);
}

bool PictureIdPresent(struct RTPVideoHeaderVP8* header) {
  return (PictureIdLength(header) > 0);
}

size_t PayloadDescriptorExtraLength(struct RTPVideoHeaderVP8* header) {
  size_t length_bytes = PictureIdLength(header);
  if (TL0PicIdxFieldPresent(header))
    ++length_bytes;
  if (TIDFieldPresent(header) || KeyIdxFieldPresent(header))
    ++length_bytes;
  if (length_bytes > 0)
    ++length_bytes;  // Include the extension field.
  return length_bytes;
}

bool XFieldPresent(struct RTPVideoHeaderVP8* header) {
  return (TIDFieldPresent(header) || TL0PicIdxFieldPresent(header) ||
          PictureIdPresent(header) ||
          KeyIdxFieldPresent(header));
}

//
// VP8 format:
//
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header (considered part of the actual payload, sent to decoder)
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |      ...      |
//      +               +
bool Parse(struct ParsedPayload* parsed_payload,
           const uint8_t* payload_data,
           size_t payload_data_length) {
  assert(parsed_payload != NULL);
  if (payload_data_length == 0) {
    printf("Empty payload!");
    return false;
  }

  // Parse mandatory first byte of payload descriptor.
  bool extension = (*payload_data & 0x80) ? true : false;               // X bit
  bool beginning_of_partition = (*payload_data & 0x10) ? true : false;  // S bit
  int partition_id = (*payload_data & 0x0F);  // PartID field

//  parsed_payload->vp8_header.width = 0;
//  parsed_payload->type.Video.height = 0;
//  parsed_payload->type.Video.is_first_packet_in_frame =
//  beginning_of_partition && (partition_id == 0);
//  parsed_payload->type.Video.simulcastIdx = 0;
//  parsed_payload->type.Video.codec = kRtpVideoVp8;
  parsed_payload->vp8_header.nonReference =
  (*payload_data & 0x20) ? true : false;  // N bit
  parsed_payload->vp8_header.partitionId = partition_id;
  parsed_payload->vp8_header.beginningOfPartition =
  beginning_of_partition;
  parsed_payload->vp8_header.pictureId = kNoPictureId;
  parsed_payload->vp8_header.tl0PicIdx = kNoTl0PicIdx;
  parsed_payload->vp8_header.temporalIdx = kNoTemporalIdx;
  parsed_payload->vp8_header.layerSync = false;
  parsed_payload->vp8_header.keyIdx = kNoKeyIdx;

  if (partition_id > 8) {
    // Weak check for corrupt payload_data: PartID MUST NOT be larger than 8.
    return false;
  }

  // Advance payload_data and decrease remaining payload size.
  payload_data++;
  if (payload_data_length <= 1) {
    printf("Error parsing VP8 payload descriptor!");
    return false;
  }
  payload_data_length--;

  if (extension) {
    const int64_t parsed_bytes =
    ParseVP8Extension(&parsed_payload->vp8_header,
                      payload_data,
                      payload_data_length);
    if (parsed_bytes < 0) {
      return false;
    }
    payload_data += parsed_bytes;
    payload_data_length -= parsed_bytes;
    if (payload_data_length == 0) {
      printf("Error parsing VP8 payload descriptor!");
      return false;
    }
  }

  // Read P bit from payload header (only at beginning of first partition).
  if (beginning_of_partition && partition_id == 0) {
    parsed_payload->frame_type =
    (*payload_data & 0x01) ? kVideoFrameDelta : kVideoFrameKey;
  } else {
    parsed_payload->frame_type = kVideoFrameDelta;
  }

  if (ParseVP8FrameSize(parsed_payload, payload_data, payload_data_length) !=
      0) {
    return false;
  }

  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;
  return true;
}

# pragma mark - vp8_depacketizer_s

struct vp8_depacketizer_s {
  struct rtp_buffer_s* packet_buffer;
  struct list encoded_fifo;
};

// walks the packet fifo, summing the total number of bytes needed to contain
// a single frame
bool tally_packet_size(struct rtp_packet_s* packet, void *arg) {
  size_t* size = (size_t*)arg;
  // nb this adds more bytes than we need in total -- rtp headers will not be
  // copied over to destination, nor will vp8 payload descriptor
  *size += packet->packet->end;
  return packet->header.m;
};

size_t calculate_frame_size(struct vp8_depacketizer_s* this,
                            struct ParsedPayload* parsed_payload)
{
  size_t frame_size = 0;
  rtp_buffer_walk(this->packet_buffer, tally_packet_size, &frame_size);
  return frame_size;
}

static void try_parse(struct vp8_depacketizer_s* this) {
  struct rtp_packet_s* packet = rtp_buffer_pop(this->packet_buffer);
  struct mbuf* payload = packet->packet;
  struct rtp_header* header = &(packet->header);
  struct ParsedPayload parsed_payload;
  int ret = Parse(&parsed_payload, payload->buf, payload->end);
  if (!ret) {
    // ?
    printf("failed to parse vp8 header. i don't know what to do.");
    return;
  }
  size_t frame_size = calculate_frame_size(this, &parsed_payload);
  // don't forget: you popped a frame before calling the method above this line
  frame_size += parsed_payload.payload_length;
  // allocate new buffer
  struct mbuf* frame_buffer = mbuf_alloc(frame_size);
  // copy all the packet payloads over
  while (1) {
    // consume the payload
    mbuf_write_mem(frame_buffer, parsed_payload.payload,
                   parsed_payload.payload_length);
    if (header->m) {
      // once the last packet is copied out, we're done
      break;
    }
    rtp_packet_free(packet);
    packet = rtp_buffer_pop(this->packet_buffer);
    payload = packet->packet;
    header = &(packet->header);
    ret = Parse(&parsed_payload,
                payload->buf + RTP_HEADER_SIZE,
                payload->end - RTP_HEADER_SIZE);
    if (!ret) {
      // ?
      printf("failed to parse vp8 header after beginning depacketization. "
             "this is even worse!\n");
      return;
    }
  }
  rtp_packet_free(packet);
  struct le* element = (struct le*) calloc(1, sizeof(struct le));
  list_append(&this->encoded_fifo, element, frame_buffer);
}

int vp8_depacketizer_alloc(struct vp8_depacketizer_s** depacketizer_out) {
  struct vp8_depacketizer_s* depacketizer = (struct vp8_depacketizer_s*)
  calloc(1, sizeof(struct vp8_depacketizer_s));
  int ret = rtp_buffer_alloc(&depacketizer->packet_buffer);
  list_init(&depacketizer->encoded_fifo);
  *depacketizer_out = depacketizer;
  return ret;
}

void vp8_depacketizer_free(struct vp8_depacketizer_s* depacketizer) {
  rtp_buffer_free(depacketizer->packet_buffer);
  free(depacketizer);
}

void vp8_depacketizer_push(struct vp8_depacketizer_s* depacketizer,
                           struct mbuf* buffer)
{
    struct rtp_packet_s* packet = rtp_packet_from_buffer(buffer);
    rtp_buffer_push(depacketizer->packet_buffer, packet);

    if (packet->header.m) {
        // should have a complete frame on the buffer at this point.
        try_parse(depacketizer);
    }
}

struct mbuf* vp8_depacketizer_pop(struct vp8_depacketizer_s* this)
{
  if (this->encoded_fifo.head == NULL) {
    return NULL;
  }
  struct le* front = this->encoded_fifo.head;
  this->encoded_fifo.head = front->next;
  struct mbuf* result = (struct mbuf*)front->data;
  free(front);
  return result;
}

