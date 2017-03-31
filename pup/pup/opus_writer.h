//
//  opus_writer.h
//  pup
//
//  Created by Charley Robinson on 3/20/17.
//
//

#ifndef opus_writer_h
#define opus_writer_h

/**
 * Writes opus packets to a file.
 */

struct opus_writer_s;

typedef void (opus_chunk_written_callback)(const char* path, void* arg);

struct opus_writer_config {
  const char* path_prefix;
  const char* stream_id;
  int chunk_length;
  opus_chunk_written_callback* callback;
  void* callback_arg;
};

int opus_writer_alloc(struct opus_writer_s** writer_out);
void opus_writer_free(struct opus_writer_s* writer);
int opus_writer_load_config(struct opus_writer_s* writer,
                            struct opus_writer_config* config);
int opus_writer_push(struct opus_writer_s* writer,
                           struct rtp_header* header,
                           struct mbuf* packet);
int opus_writer_close(struct opus_writer_s* writer);
const char* opus_writer_get_outfile_path(struct opus_writer_s* pthis);

#endif /* opus_writer_h */
