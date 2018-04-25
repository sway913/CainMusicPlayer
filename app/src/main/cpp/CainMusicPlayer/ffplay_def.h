//
// Created by admin on 2018/3/28.
//

#ifndef MUSICPLAYER_FFPLAYE_DEF_H
#define MUSICPLAYER_FFPLAYE_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff_config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#include "../common/Mutex.h"
#include "../common/Thread.h"

#define MIX_MAXVOLUME 128

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* no AV correction audioState done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

typedef uint16_t AudioFormat;

typedef void (*AudioCallback) (void *userdata, uint8_t * stream, int len);

#define AUDIO_INVALID   0x0000
#define AUDIO_U8        0x0008  /**< Unsigned 8-bit samples */
#define AUDIO_S8        0x8008  /**< Signed 8-bit samples */
#define AUDIO_U16LSB    0x0010  /**< Unsigned 16-bit samples */
#define AUDIO_S16LSB    0x8010  /**< Signed 16-bit samples */
#define AUDIO_U16MSB    0x1010  /**< As above, but big-endian byte order */
#define AUDIO_S16MSB    0x9010  /**< As above, but big-endian byte order */
#define AUDIO_U16       AUDIO_U16LSB
#define AUDIO_S16       AUDIO_S16LSB

#define AUDIO_S32LSB    0x8020  /**< 32-bit integer samples */
#define AUDIO_S32MSB    0x9020  /**< As above, but big-endian byte order */
#define AUDIO_S32       AUDIO_S32LSB

#define AUDIO_F32LSB    0x8120  /**< 32-bit floating point samples */
#define AUDIO_F32MSB    0x9120  /**< As above, but big-endian byte order */
#define AUDIO_F32       AUDIO_F32LSB

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define AUDIO_U16SYS    AUDIO_U16LSB
#define AUDIO_S16SYS    AUDIO_S16LSB
#define AUDIO_S32SYS    AUDIO_S32LSB
#define AUDIO_F32SYS    AUDIO_F32LSB
#else
#define AUDIO_U16SYS    AUDIO_U16MSB
#define AUDIO_S16SYS    AUDIO_S16MSB
#define AUDIO_S32SYS    AUDIO_S32MSB
#define AUDIO_F32SYS    AUDIO_F32MSB
#endif

typedef struct AudioSpec {
    int sampleRate;             // 采样率
    AudioFormat format;         // 音频格式
    uint8_t channels;           // 声道
    uint16_t samples;           /**< Audio buffer size in samples (power of 2) */
    uint32_t size;              /**< Audio buffer size in bytes (calculated) */
    AudioCallback callback;     // 音频回调
    void *userdata;             // 音频数据
} AudioSpec;


typedef struct MyAVPacketList {
   AVPacket pkt;
   struct MyAVPacketList *next;
   int serial;
} MyAVPacketList;

typedef struct PacketQueue {
   MyAVPacketList *first_pkt, *last_pkt;
   int nb_packets;
   int size;
   int64_t duration;
   int abort_request;
   int serial;
   Mutex *mutex;
   Cond *cond;
} PacketQueue;

#define SAMPLE_QUEUE_SIZE 2
#define FRAME_QUEUE_SIZE 2

typedef struct AudioParams {
    int sampleRate;             // 采样率
    int channels;               // 声道
    int64_t channel_layout;     // 声道设计
    enum AVSampleFormat fmt;    // 采样格式
    int frame_size;             // 帧大小
    int bytes_per_sec;          // 每秒字节数
} AudioParams;

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock audioState based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
} Frame;

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    Mutex *mutex;
    Cond *cond;
    PacketQueue *pktq;
} FrameQueue;

typedef struct Decoder {
    AVPacket pkt;
    AVPacket pkt_temp;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    Cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    Thread *decoder_tid;
} Decoder;

typedef struct AudioState {
    int abort_request;          // 停止
    int paused;                 // 暂停标志
    int last_paused;            // 上一次暂停状态标志
    int seek_req;               // 定位请求
    int seek_flags;             // 定位标志
    int64_t seek_pos;           // 定位位置(秒)
    int64_t seek_rel;           //
    AVFormatContext *pFormatContext;        // 解复用上下文

    Clock audioClock;           // 音频时钟
    Clock extClock;             // 外部时钟

    FrameQueue audioFrameQueue; // 音频帧队列
    Decoder audioDecoder;       // 音频解码器

    int audioStreamIdx;         // 音频流索引
    AVStream *audioStream;      // 音频流
    PacketQueue audioQueue;     // 音频裸数据包队列

    double audio_clock;
    int audio_clock_serial;
    int audio_hw_buf_size;
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    short *audio_new_buf; // soundtouch
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    unsigned int audio_new_buf_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;           // 写入大小
    struct AudioParams audio_src;       // 原音频参数
    struct AudioParams audio_tgt;       // 目标参数
    struct SwrContext *swr_ctx;         // 音频转码上下文

    int eof;                // 结尾标志
    char *filename;         // 文件名
    Thread *demuxThread;    // 解复用线程
    Cond *demuxCondition;   // 解复用条件锁
} AudioState;

inline static void *mallocz(size_t size) {
    void *mem = malloc(size);
    if (!mem) {
        return mem;
    }

    memset(mem, 0, size);
    return mem;
}


inline static void freep(void **mem) {
    if (mem && *mem) {
        free(*mem);
        *mem = NULL;
    }
}

inline static const char *
SDL_GetError(void) {
    return NULL;
}


#ifdef __cplusplus
};
#endif

#endif //MUSICPLAYER_FFPLAYE_DEF_H
