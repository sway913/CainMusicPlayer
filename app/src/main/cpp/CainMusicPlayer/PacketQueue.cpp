//
// Created by admin on 2018/3/29.
//

#include "PacketQueue.h"

#ifdef __cplusplus
extern "C" {
#endif

extern AVPacket flush_pkt;

/**
 * 入队裸数据包
 * @param q
 * @param pkt
 * @return
 */
int packet_queue_put_private(PacketQueue *q, AVPacket *pkt) {
    MyAVPacketList *pkt1;

    if (q->abort_request)
        return -1;

    pkt1 = (MyAVPacketList *) av_malloc(sizeof(MyAVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if (pkt == &flush_pkt)
        q->serial++;
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;
    /* XXX: should duplicate packet data in DV case */
    CondSignal(q->cond);
    return 0;
}

/**
 * 入队裸数据包
 * @param q
 * @param pkt
 * @return
 */
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    int ret;

    MutexLock(q->mutex);
    ret = packet_queue_put_private(q, pkt);
    MutexUnlock(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_packet_unref(pkt);

    return ret;
}

/**
 * 入队空数据
 * @param q
 * @param stream_index
 * @return
 */
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index) {
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

/**
 * 初始化裸数据队列
 * @param q
 * @return
 */
int packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = MutexCreate();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "MutexCreate(): Failed\n");
        return AVERROR(ENOMEM);
    }
    q->cond = CondCreate();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "CondCreate(): Failed\n");
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;
    return 0;
}

/**
 * 刷出剩余帧
 * @param q
 */
void packet_queue_flush(PacketQueue *q) {
    MyAVPacketList *pkt, *pkt1;

    MutexLock(q->mutex);
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    MutexUnlock(q->mutex);
}

/**
 * 销毁队列
 * @param q
 */
void packet_queue_destroy(PacketQueue *q) {
    packet_queue_flush(q);
    MutexDestroy(q->mutex);
    CondDestroy(q->cond);
}

/**
 * 裸数据队列停止
 * @param q
 */
void packet_queue_abort(PacketQueue *q) {
    MutexLock(q->mutex);

    q->abort_request = 1;

    CondSignal(q->cond);

    MutexUnlock(q->mutex);
}

/**
 * 裸数据包队列开始
 * @param q
 */
void packet_queue_start(PacketQueue *q) {
    MutexLock(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt);
    MutexUnlock(q->mutex);
}

/**
 * 取出裸数据包
 * @param q
 * @param pkt
 * @param block
 * @param serial
 * @return
 */
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial) {
    MyAVPacketList *pkt1;
    int ret;

    MutexLock(q->mutex);

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            q->duration -= pkt1->pkt.duration;
            *pkt = pkt1->pkt;
            if (serial)
                *serial = pkt1->serial;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            CondWait(q->cond, q->mutex);
        }
    }
    MutexUnlock(q->mutex);
    return ret;
}

#ifdef __cplusplus
};
#endif