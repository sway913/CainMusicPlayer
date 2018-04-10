//
// Created by admin on 2018/3/29.
//

#include "MediaDecoder.h"
#include "PacketQueue.h"
#include "FrameQueue.h"

#ifdef __cplusplus
extern "C" {
#endif

extern AVPacket flush_pkt;

/**
 * 解码器初始化
 * @param d
 * @param avctx
 * @param queue
 * @param empty_queue_cond
 */
void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, Cond *empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
}

/**
 * 解码器解码帧
 * @param d
 * @param frame
 * @param sub
 * @return
 */
int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    int got_frame = 0;

    do {
        int ret = -1;

        if (d->queue->abort_request) {
            return -1;
        }

        if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
            AVPacket pkt;
            do {
                if (d->queue->nb_packets == 0)
                    CondSignal(d->empty_queue_cond);
                if (packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial) < 0)
                    return -1;
                if (pkt.data == flush_pkt.data) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            } while (pkt.data == flush_pkt.data || d->queue->serial != d->pkt_serial);
            av_packet_unref(&d->pkt);
            d->pkt_temp = d->pkt = pkt;
            d->packet_pending = 1;
        }

        switch (d->avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                ret = avcodec_decode_video2(d->avctx, frame, &got_frame, &d->pkt_temp);
                if (got_frame) {
                    frame->pts = av_frame_get_best_effort_timestamp(frame);
                }
                break;

            case AVMEDIA_TYPE_AUDIO:
                ret = avcodec_decode_audio4(d->avctx, frame, &got_frame, &d->pkt_temp);
                if (got_frame) {
                    AVRational tb = (AVRational){1, frame->sample_rate};
                    if (frame->pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(frame->pts, av_codec_get_pkt_timebase(d->avctx), tb);
                    else if (d->next_pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                    if (frame->pts != AV_NOPTS_VALUE) {
                        d->next_pts = frame->pts + frame->nb_samples;
                        d->next_pts_tb = tb;
                    }
                }
                break;

            case AVMEDIA_TYPE_SUBTITLE:
                ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, &d->pkt_temp);
                break;
        }

        if (ret < 0) {
            d->packet_pending = 0;
        } else {
            d->pkt_temp.dts =
            d->pkt_temp.pts = AV_NOPTS_VALUE;
            if (d->pkt_temp.data) {
                if (d->avctx->codec_type != AVMEDIA_TYPE_AUDIO)
                    ret = d->pkt_temp.size;
                d->pkt_temp.data += ret;
                d->pkt_temp.size -= ret;
                if (d->pkt_temp.size <= 0)
                    d->packet_pending = 0;
            } else {
                if (!got_frame) {
                    d->packet_pending = 0;
                    d->finished = d->pkt_serial;
                }
            }
        }
    } while (!got_frame && !d->finished);

    return got_frame;
}

/**
 * 销毁解码器
 * @param d
 */
void decoder_destroy(Decoder *d) {
    av_packet_unref(&d->pkt);
    avcodec_free_context(&d->avctx);
}

/**
 * 解码器开始解码
 * @param d
 * @param fn
 * @param arg
 * @return
 */
int decoder_start(Decoder *d, int (*fn)(void *), void *arg) {
    packet_queue_start(d->queue);
    d->decoder_tid = ThreadCreate(fn, arg, "decoder");
    if (!d->decoder_tid) {
        av_log(NULL, AV_LOG_ERROR, "ThreadCreate(): Failed\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

/**
 * 解码器取消解码
 * @param d
 * @param fq
 */
void decoder_abort(Decoder *d, FrameQueue *fq) {
    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    ThreadWait(d->decoder_tid, NULL);
    d->decoder_tid = NULL;
    packet_queue_flush(d->queue);
}

#ifdef __cplusplus
};
#endif