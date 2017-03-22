//
//  opus_depacketizer.c
//  pup
//
//  Created by Charley Robinson on 3/20/17.
//
//

#include <stdlib.h>
#include <stdio.h>
#include <re/re.h>
#include <libavformat/avformat.h>
#include "opus_writer.h"

struct opus_writer_s {
  AVFormatContext* format_context;
  AVCodec* audio_codec;
  AVStream* audio_stream;
  AVCodecContext* audio_ctx_out;
  uint32_t first_ts;

  const char* outfile_prefix;
  const char* stream_id;
  char* outfile_path;
};

static int setup_outfile(struct opus_writer_s* pthis,
                         const char* filename);

int opus_writer_alloc(struct opus_writer_s** depacketizer_out)
{
  av_register_all();
  struct opus_writer_s* pthis = (struct opus_writer_s*)
  calloc(1, sizeof(struct opus_writer_s));
  *depacketizer_out = pthis;
  return 0;
}

void opus_depacketizer_free(struct opus_writer_s* depacketizer)
{
  free(depacketizer);
}

int opus_writer_load_config(struct opus_writer_s* writer,
                            struct opus_writer_config* config)
{
  writer->outfile_prefix = config->path_prefix;
  writer->stream_id = config->stream_id;
  size_t path_length = strlen(writer->stream_id) +
  strlen(writer->outfile_prefix) + strlen("/.opus");
  writer->outfile_path = malloc(path_length + 1);
  writer->outfile_path[path_length] = '\0';
  sprintf(writer->outfile_path, "%s/%s.opus",
          writer->outfile_prefix, writer->stream_id);
  int ret = setup_outfile(writer, writer->outfile_path);
  return ret;
}


static int setup_outfile(struct opus_writer_s* pthis,
                         const char* filename)
{
  int ret = avformat_alloc_output_context2(&pthis->format_context,
                                           NULL, NULL, filename);
  av_dump_format(pthis->format_context, 0, filename, 1);
  AVOutputFormat* fmt = pthis->format_context->oformat;

  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) {
    ret = avio_open(&pthis->format_context->pb,
                    filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open '%s': %s\n", filename,
              av_err2str(ret));
      exit(1);
    }
  }
  // even though we won't use it, create an encoding context to feed the file
  // writer
  pthis->audio_codec = avcodec_find_encoder(fmt->audio_codec);
  if (!pthis->audio_codec) {
    printf("Audio codec not found\n");
    exit(1);
  }
  pthis->audio_stream =
  avformat_new_stream(pthis->format_context,
                      pthis->audio_codec);

  pthis->audio_ctx_out = pthis->audio_stream->codec;

  pthis->audio_stream->time_base.num = 1;
  pthis->audio_stream->time_base.den = 48000;

  // Codec configuration
  pthis->audio_ctx_out->bit_rate = 40000;
  pthis->audio_ctx_out->sample_fmt = AV_SAMPLE_FMT_S16;
  pthis->audio_ctx_out->sample_rate = 48000;
  pthis->audio_ctx_out->channels = 1;
  pthis->audio_ctx_out->channel_layout = AV_CH_LAYOUT_MONO;

  /* open the context */
  if (avcodec_open2(pthis->audio_ctx_out,
                    pthis->audio_codec, NULL) < 0) {
    printf("Could not open audio codec\n");
    exit(1);
  }

  ret = avformat_write_header(pthis->format_context, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error occurred when opening output file: %s\n",
            av_err2str(ret));
    exit(1);
  }

  printf("Ready to encode video file %s\n", filename);
  return 0;
}

static const char *get_error_text(const int error)
{
  static char error_buffer[255];
  av_strerror(error, error_buffer, sizeof(error_buffer));
  return error_buffer;
}

int opus_depacketizer_push(struct opus_writer_s* pthis,
                           struct rtp_header* header,
                           struct mbuf* packet)
{
  // repackage rtp packet as encoded opus frame and write to ogg
  AVPacket pkt = { 0 };
  av_init_packet(&pkt);
  pkt.stream_index = pthis->audio_stream->index;
  if (!pthis->first_ts) {
    pthis->first_ts = header->ts;
  }
  pkt.pts = header->ts - pthis->first_ts;
  pkt.dts = pkt.pts;
  pkt.buf = NULL;
  pkt.data = &(packet->buf[packet->pos]);
  pkt.size = (int)(packet->end - packet->pos);
  int ret = av_interleaved_write_frame(pthis->format_context, &pkt);
  mem_deref(packet);
//  if (header->ts - pthis->first_ts > 480000) {
//    int error;
//    if ((error = av_write_trailer(pthis->format_context)) < 0) {
//      fprintf(stderr, "Could not write output file trailer (error '%s')\n",
//              get_error_text(error));
//      return error;
//    }
//    exit(0);
//  }
  return ret;
}

const char* opus_depacketizer_get_outfile_path(struct opus_writer_s* pthis) {
  return (const char*)pthis->outfile_path;
}
