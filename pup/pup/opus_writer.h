//
//  opus_depacketizer.h
//  pup
//
//  Created by Charley Robinson on 3/20/17.
//
//

#ifndef opus_depacketizer_h
#define opus_depacketizer_h

struct opus_writer_s;

typedef void (opus_chunk_written_callback)(const char* path, void* arg);

struct opus_writer_config {
  const char* path_prefix;
  const char* stream_id;
  int chunk_length;
  opus_chunk_written_callback* callback;
  void* callback_arg;
};

int opus_writer_alloc(struct opus_writer_s** depacketizer_out);
void opus_depacketizer_free(struct opus_writer_s* depacketizer);
int opus_writer_load_config(struct opus_writer_s* writer,
                            struct opus_writer_config* config);
int opus_depacketizer_push(struct opus_writer_s* depacketizer,
                           struct rtp_header* header,
                           struct mbuf* packet);
const char* opus_depacketizer_get_outfile_path(struct opus_writer_s* pthis);

#endif /* opus_depacketizer_h */
