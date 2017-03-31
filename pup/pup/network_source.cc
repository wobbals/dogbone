//
//  network_source.c
//  pup
//
//  Created by Charley Robinson on 3/26/17.
//
//

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include "vp8_decode.h"
#include "network_source.h"
#include <uv.h>
}

#include <queue>

struct network_source_s {
  const char* stream_id;
  enum AVCodecID video_codec_id;
  enum AVCodecID audio_codec_id;
  AVCodecContext* video_codec_context;
  AVCodecContext* audio_codec_context;

  int64_t first_audio_pts;
  int64_t first_video_pts;

  AVAudioFifo* audio_fifo;
  uv_mutex_t audio_fifo_mutex;
  std::queue<AVFrame*> video_frame_fifo;
  uv_mutex_t video_codec_mutex;
  struct media_stream_s* media_stream;
};

static int setup_streams(struct network_source_s* pthis);
static int media_stream_audio_callback(struct media_stream_s* stream,
                                       AVFrame* frame, double time_clock,
                                       void* p);
static int media_stream_video_callback(struct media_stream_s* stream,
                                       struct smart_frame_t** frame_out,
                                       double time_clock, void* p);

void network_source_alloc(struct network_source_s** stream_out) {
  struct network_source_s* pthis = (struct network_source_s*)
  calloc(1, sizeof(struct network_source_s));
  media_stream_alloc(&pthis->media_stream);
  media_stream_set_name(pthis->media_stream, "NET");
  media_stream_set_class(pthis->media_stream, "");
  media_stream_set_video_read(pthis->media_stream,
                              media_stream_video_callback, pthis);
  media_stream_set_audio_read(pthis->media_stream,
                              media_stream_audio_callback, pthis);
  uv_mutex_init(&pthis->audio_fifo_mutex);
  uv_mutex_init(&pthis->video_codec_mutex);
  // TODO: reduce buffer size -- this is just for preloaded stream testing
  pthis->audio_fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16, 1, 48000*5);
  pthis->video_frame_fifo = std::queue<AVFrame*>();
  *stream_out = pthis;
}

void network_source_free(struct network_source_s* pthis) {
  avcodec_free_context(&pthis->video_codec_context);
  avcodec_free_context(&pthis->audio_codec_context);
  uv_mutex_destroy(&pthis->audio_fifo_mutex);
  uv_mutex_destroy(&pthis->video_codec_mutex);
  free(pthis);
}

int network_source_load_config(struct network_source_s* pthis,
                               struct network_source_config_s* config)
{
  pthis->stream_id = config->stream_id;
  pthis->video_codec_id = config->video_codec_id;
  pthis->audio_codec_id = config->audio_codec_id;
  int ret = setup_streams(pthis);
  return ret;
}

int network_source_push_video(struct network_source_s* pthis,
                              AVPacket* encoded_frame)
{
  if (pthis->first_video_pts > 0) {
    encoded_frame->pts -= pthis->first_video_pts;
  } else {
    pthis->first_video_pts = encoded_frame->pts;
    encoded_frame->pts = 0;
  }
  encoded_frame->dts = encoded_frame->pts;
  int ret = avcodec_send_packet(pthis->video_codec_context, encoded_frame);
  if (ret) {
    printf("cannot push video frame: %s\n", av_err2str(ret));
  }
  AVFrame* frame = av_frame_alloc();
  ret = avcodec_receive_frame(pthis->video_codec_context, frame);
  while (!ret && frame) {
    uv_mutex_lock(&pthis->video_codec_mutex);
    pthis->video_frame_fifo.push(frame);
    uv_mutex_unlock(&pthis->video_codec_mutex);
    frame = av_frame_alloc();
    ret = avcodec_receive_frame(pthis->video_codec_context, frame);
  }
  return ret;
}

int network_source_push_audio(struct network_source_s* pthis,
                              AVPacket* encoded_frame)
{
  // consume the frame
  int ret = avcodec_send_packet(pthis->audio_codec_context, encoded_frame);
  AVFrame* decoded_frame = av_frame_alloc();
  // decode whatever is in the decoder and push it to the sample fifo
  ret = avcodec_receive_frame(pthis->audio_codec_context, decoded_frame);
  while (!ret) {
    uv_mutex_lock(&pthis->audio_fifo_mutex);
    av_audio_fifo_write(pthis->audio_fifo,
                        (void**)decoded_frame->data,
                        decoded_frame->nb_samples);
    uv_mutex_unlock(&pthis->audio_fifo_mutex);
    ret = avcodec_receive_frame(pthis->audio_codec_context, decoded_frame);
  }
  return ret;
}

struct media_stream_s* network_source_get_stream
(struct network_source_s* pthis)
{
  return pthis->media_stream;
}

#pragma mark - Internal utilites

static int media_stream_video_callback(struct media_stream_s* stream,
                                       struct smart_frame_t** frame_out,
                                       double time_clock, void* p)
{
  struct network_source_s* pthis = (struct network_source_s*)p;
  // TODO: inspect time_clock and make sure there's a healthy buffer.
  // This may also be an entry point for implementing lipsync
  AVFrame* frame = NULL;
  int ret;
  uv_mutex_lock(&pthis->video_codec_mutex);
  if (pthis->video_frame_fifo.size() > 0) {
    frame = pthis->video_frame_fifo.front();
    pthis->video_frame_fifo.pop();
    ret = 0;
  } else {
    ret = EAGAIN;
  }
  uv_mutex_unlock(&pthis->video_codec_mutex);
  if (!ret && frame) {
    smart_frame_create(frame_out, frame);
  }
  return ret;
}

static int media_stream_audio_callback(struct media_stream_s* stream,
                                       AVFrame* frame, double time_clock,
                                       void* p)
{
  struct network_source_s* pthis = (struct network_source_s*)p;
  // TODO: inspect time_clock and make sure there's a healthy buffer.
  // This may also be an entry point for implementing lipsync
  uv_mutex_lock(&pthis->audio_fifo_mutex);
  int ret = av_audio_fifo_read(pthis->audio_fifo,
                               (void**)frame->data,
                               frame->nb_samples);
  uv_mutex_unlock(&pthis->audio_fifo_mutex);
  return ret;
}

// TODO: Lots of hardcoded decisions in here. Clean up and allow flexibility.
static int alloc_audio_context(AVCodecContext** context_out,
                               enum AVCodecID codec_id)
{
  AVCodec* codec = avcodec_find_decoder(codec_id);
  if (AV_CODEC_ID_OPUS == codec->id &&
      strcmp("libopus", codec->name))
  {
    printf("Switch from %s to libopus\n", codec->name);
    codec = avcodec_find_decoder_by_name("libopus");
  }
  *context_out = avcodec_alloc_context3(codec);
  if (NULL == codec || NULL == *context_out) {
    return 1;
  }
  (*context_out)->channels = 1;
  (*context_out)->channel_layout = AV_CH_LAYOUT_MONO;
  (*context_out)->sample_fmt = AV_SAMPLE_FMT_S16;
  (*context_out)->request_sample_fmt = AV_SAMPLE_FMT_S16;

  av_opt_set_int(*context_out, "refcounted_frames", 1, 0);
  // should we be using the opts dictionary (third arg)?
  int ret = avcodec_open2(*context_out, NULL, NULL);
  return ret;
}

static int alloc_video_context(AVCodecContext** context_out,
                               enum AVCodecID codec_id)
{
  AVCodec* codec = avcodec_find_decoder(codec_id);
  *context_out = avcodec_alloc_context3(codec);
  if (NULL == codec || NULL == *context_out) {
    return 1;
  }
  av_opt_set_int(*context_out, "refcounted_frames", 1, 0);
  // should we be using the opts dictionary (third arg)?
  int ret = avcodec_open2(*context_out, NULL, NULL);
  return ret;
}

static int setup_streams(struct network_source_s* pthis) {
  avcodec_register_all();
  int vret = 0;
  int aret = 0;

  if (AV_CODEC_ID_NONE != pthis->video_codec_id) {
    vret = alloc_video_context(&pthis->video_codec_context,
                               pthis->video_codec_id);
  }
  if (vret) {
    printf("unable to find video decoder %d. check ffmpeg configuraion\n",
           pthis->video_codec_id);
  }
  if (AV_CODEC_ID_NONE != pthis->audio_codec_id) {
    aret = alloc_audio_context(&pthis->audio_codec_context,
                               pthis->audio_codec_id);
  }
  if (aret) {
    printf("unable to find audio decoder %d. check ffmpeg configuraion\n",
           pthis->audio_codec_id);
  }
  return vret & aret;
}
