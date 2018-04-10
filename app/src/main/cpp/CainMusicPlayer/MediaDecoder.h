//
// Created by admin on 2018/3/29.
//

#ifndef CAINPLAYER_MEDIADECODER_H
#define CAINPLAYER_MEDIADECODER_H

#include "ffplay_def.h"

#ifdef __cplusplus
extern "C" {
#endif
// 解码器初始化
void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, Cond *empty_queue_cond);
// 解码器解码帧
int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);
// 销毁解码器
void decoder_destroy(Decoder *d);
// 解码器开始解码
int decoder_start(Decoder *d, int (*fn)(void *), void *arg);
// 解码器取消解码
void decoder_abort(Decoder *d, FrameQueue *fq);

#ifdef __cplusplus
};
#endif

#endif //CAINPLAYER_MEDIADECODER_H
