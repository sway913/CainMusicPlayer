//
// Created by admin on 2018/3/29.
//

#ifndef MUSICPLAYER_PACKETQUEUE_H
#define MUSICPLAYER_PACKETQUEUE_H

#include "ffplay_def.h"

#ifdef __cplusplus
extern "C" {
#endif

// 入队裸数据包
int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
// 入队裸数据包
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
// 入队空的裸数据包
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
// 初始化队列
int packet_queue_init(PacketQueue *q);
// 刷出剩余裸数据包
void packet_queue_flush(PacketQueue *q);
// 销毁裸数据包队列
void packet_queue_destroy(PacketQueue *q);
// 停止入队
void packet_queue_abort(PacketQueue *q);
// 开始入队
void packet_queue_start(PacketQueue *q);
// 获取裸数据包
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);

#ifdef __cplusplus
};
#endif



#endif //MUSICPLAYER_PACKETQUEUE_H
