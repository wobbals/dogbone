//
//  pup_dogbone.cc
//  pup
//
//  Created by Charley Robinson on 3/11/17.
//
//

extern "C" {
#include <stdlib.h>
#include <zmq.h>
#include <re/re.h>
#include <uv.h>

#include "pup_dogbone.h"
#include "vp8_depacketizer.h"
}

#include <map>
#include <string>
#include <vector>

struct pup_message_s {
    std::string name;
    std::string stream_id;
    uint8_t* payload;
    size_t payload_size;
};

struct stream_attributes_s {
    std::string stream_id;
    // maps generated from rtpmap -- indexed by payload type
    std::map<int16_t, std::string> codec_names;
    std::map<int16_t, int64_t> sample_rates;
    std::map<int16_t, int8_t> channel_counts;
};

struct pup_dogbone_s {
    void* zmq_context;
    void* control_socket;
    uv_thread_t control_thread;

    std::map<std::string, struct stream_attributes_s*> stream_attrs;
  struct vp8_depacketizer_s* vp8;
};

static bool attr_walk(const char *name, const char *value, void *arg) {
    printf("%s : %s\n", name, value);
    return 1;
}
int print_f (const char *p, size_t size, void *arg) {
    printf("re: %s\n", p);
    return 0;

    /* 
     struct re_printf print;
     print.vph = print_f;
     print.arg = NULL;
     */
}

static struct stream_attributes_s*
dogbone_stream_attrs(struct pup_dogbone_s* dogbone, std::string& stream_id)
{
    auto search = dogbone->stream_attrs.find(stream_id);
    if (search == dogbone->stream_attrs.end()) {
        struct stream_attributes_s* attrs = (struct stream_attributes_s*)
        calloc(1, sizeof(stream_attributes_s));
        attrs->stream_id = stream_id;
        attrs->codec_names = std::map<int16_t, std::string>();
        attrs->sample_rates = std::map<int16_t, int64_t>();
        attrs->channel_counts = std::map<int16_t, int8_t>();
        dogbone->stream_attrs[stream_id] = attrs;
    }
    return dogbone->stream_attrs[stream_id];
}

static void build_rtpmap(struct pup_dogbone_s* dogbone,
                         std::string& stream_id,
                         struct sdp_session* sdp_session)
{
    struct stream_attributes_s* attrs =
    dogbone_stream_attrs(dogbone, stream_id);
    const struct list* media_list = sdp_session_medial(sdp_session, 0);
    if (!media_list) {
        return;
    }
    struct le* media_cursor = list_head(media_list);
    while (media_cursor) {
        struct sdp_media* media = (struct sdp_media*) media_cursor->data;
        const struct list* format_list = sdp_media_format_lst(media, 0);
        struct le* format_cursor = format_list->head;
        while (format_cursor) {
            struct sdp_format* format = (struct sdp_format*)format_cursor->data;
            int id = atoi(format->id);
            attrs->codec_names[id] = format->name;
            attrs->sample_rates[id] = format->srate;
            attrs->channel_counts[id] = format->ch;
            format_cursor = format_cursor->next;
        }
        media_cursor = media_cursor->next;
    }
}

static void handle_offer(struct pup_dogbone_s* dogbone,
                         struct pup_message_s* message)
{
    struct re_printf print;
    print.vph = print_f;
    print.arg = NULL;

    struct sdp_session* sdp_session;
    struct sa laddr; // garbage is fine -- this should not be used.
    sdp_session_alloc(&sdp_session, &laddr);
    struct mbuf mbuf;
    mbuf.buf = message->payload;
    mbuf.size = strlen((char*)message->payload);
    mbuf.pos = 0;
    mbuf.end = mbuf.size;
    sdp_decode(sdp_session, &mbuf, 1);
    build_rtpmap(dogbone, message->stream_id, sdp_session);
    mem_deref(sdp_session);
}

#pragma mark - dogbone handlers

static void handle_rtp(struct pup_dogbone_s* dogbone,
                       struct pup_message_s* message)
{
  struct mbuf* mbuf = mbuf_alloc(message->payload_size);
  mbuf_write_mem(mbuf, message->payload, message->payload_size);
  mbuf->pos = 0;
  struct stream_attributes_s* attrs =
  dogbone->stream_attrs[message->stream_id];
  struct rtp_header hdr;
  rtp_hdr_decode(&hdr, mbuf);
  printf("ssrc: %d pt: %d seqno: %u ts: %u codec: %s\n",
         hdr.ssrc, hdr.pt, hdr.seq, hdr.ts,
         attrs->codec_names[hdr.pt].c_str());

  if (!attrs->codec_names[hdr.pt].compare("VP8")) {
    vp8_depacketizer_push(dogbone->vp8, mbuf);
  } else {
    // TODO: add other depacketizers here and make this multi-stream-able
    mem_deref(mbuf);
  }
}

static void handle_message(struct pup_dogbone_s* dogbone,
                           struct pup_message_s* message)
{
    if (!message->name.compare("offer")) {
        handle_offer(dogbone, message);
    } else if (!message->name.compare("data")) {
        handle_rtp(dogbone, message);
    } else {
        printf("unknown message name %s", message->name.c_str());
    }
}

#pragma mark - Message handling

static void pup_message_reset(struct pup_message_s* msg) {
    free(msg->payload);
    memset(msg, 0, sizeof(struct pup_message_s));
}

static int receive_message(void* socket, struct pup_message_s* msg) {
    pup_message_reset(msg);
    while (1) {
        zmq_msg_t message;
        zmq_msg_init (&message);
        zmq_msg_recv (&message, socket, 0);
        //  Process the message frame
        uint8_t* sz_msg = (uint8_t*)malloc(zmq_msg_size(&message) + 1);
        memcpy(sz_msg, zmq_msg_data(&message), zmq_msg_size(&message));
        sz_msg[zmq_msg_size(&message)] = '\0';

        if (msg->name.empty()) {
            msg->name = (char*)sz_msg;
            free(sz_msg);
        } else if (msg->stream_id.empty()) {
            msg->stream_id = (char*)sz_msg;
            free(sz_msg);
        } else if (NULL == msg->payload) {
            msg->payload = sz_msg;
            msg->payload_size = zmq_msg_size(&message);
        } else {
            printf("unknown extra message part received. freeing.");
            free(sz_msg);
        }

        zmq_msg_close (&message);
        if (!zmq_msg_more (&message))
            break;      //  Last message frame
    }
    return 0;
}

static void control_main(void* p) {
    struct pup_dogbone_s* dogbone = (struct pup_dogbone_s*)p;
    printf("sup\n");
    // connect to dogbone ipc
    int ret = zmq_connect(dogbone->control_socket, "ipc:///tmp/dogbone");
    if (ret < 0) {
        printf("failed to connect to dogbone. errno: %d", errno);
        return;
    }
    printf("connected to dogbone ipc\n");
    struct pup_message_s* message = (struct pup_message_s*)
    calloc(1, sizeof(struct pup_message_s));
    ret = receive_message(dogbone->control_socket, message);
    while (!ret) {
        handle_message(dogbone, message);
        ret = receive_message(dogbone->control_socket, message);
    }
}

#pragma mark - Memory lifecycle

int pup_dogbone_alloc(struct pup_dogbone_s** dogbone_out) {
  struct pup_dogbone_s* dogbone =
  (struct pup_dogbone_s*) calloc(1, sizeof(struct pup_dogbone_s));
  dogbone->stream_attrs =
  std::map<std::string, struct stream_attributes_s*>();
  vp8_depacketizer_alloc(&dogbone->vp8);

  //  Prepare our context and socket
  dogbone->zmq_context = zmq_ctx_new();
  dogbone->control_socket = zmq_socket (dogbone->zmq_context, ZMQ_PULL);
  uv_thread_create(&dogbone->control_thread, control_main, dogbone);
  return 0;
}

void pup_dogbone_free(struct pup_dogbone_s* dogbone) {
    for (auto attrs : dogbone->stream_attrs) {
        free(attrs.second);
    }
    dogbone->stream_attrs.clear();

    // this will crash. implement an interrupt and join on the thread
    free(dogbone);
}
