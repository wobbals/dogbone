//
//  network_stream.h
//  pup
//
//  Created by Charley Robinson on 3/26/17.
//
//

#ifndef network_stream_h
#define network_stream_h

#include "media_stream.h"

struct network_source_s;

struct network_source_config_s {
  const char* stream_id;
  enum AVCodecID video_codec_id;
  enum AVCodecID audio_codec_id;
};

void network_source_alloc(struct network_source_s** stream_out);
void network_source_free(struct network_source_s* stream);
// Config may be reloaded at any time
int network_source_load_config(struct network_source_s* stream,
                               struct network_source_config_s* config);
int network_source_push_video(struct network_source_s* source,
                              AVPacket* encoded_frame);
int network_source_push_audio(struct network_source_s* source,
                              AVPacket* encoded_frame);

struct media_stream_s* network_source_get_stream
(struct network_source_s* stream);

#endif /* network_stream_h */
