//
// Created by admin on 2018/3/29.
//

#include "FrameQueue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 释放帧的引用
 * @param vp
 */
void frame_queue_unref_item(Frame *vp) {
    av_frame_unref(vp->frame);
    avsubtitle_free(&vp->sub);
}

/**
 * 帧队列初始化
 * @param f
 * @param pktq
 * @param max_size
 * @param keep_last
 * @return
 */
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last) {
    int i;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = MutexCreate())) {
        av_log(NULL, AV_LOG_FATAL, "MutexCreate(): Failed\n");
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = CondCreate())) {
        av_log(NULL, AV_LOG_FATAL, "CondCreate(): Failed\n");
        return AVERROR(ENOMEM);
    }
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = (keep_last != 0);
    for (i = 0; i < f->max_size; i++) {
        if (!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    }
    return 0;
}

/**
 * 销毁帧队列
 * @param f
 */
void frame_queue_destory(FrameQueue *f) {
    int i;
    for (i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        frame_queue_unref_item(vp);
        av_frame_free(&vp->frame);
    }
    MutexDestroy(f->mutex);
    CondDestroy(f->cond);
}

/**
 * 通知
 * @param f
 */
void frame_queue_signal(FrameQueue *f) {
    MutexLock(f->mutex);
    CondSignal(f->cond);
    MutexUnlock(f->mutex);
}

/**
 * 查找当前帧
 * @param f
 * @return
 */
Frame *frame_queue_peek(FrameQueue *f) {
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

/**
 * 查找下一帧
 * @param f
 * @return
 */
Frame *frame_queue_peek_next(FrameQueue *f) {
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

/**
 * 查找上一帧
 * @param f
 * @return
 */
Frame *frame_queue_peek_last(FrameQueue *f) {
    return &f->queue[f->rindex];
}

/**
 * 查找可写帧
 * @param f
 * @return
 */
Frame *frame_queue_peek_writable(FrameQueue *f) {
/* wait until we have space to put a new frame */
    MutexLock(f->mutex);
    while (f->size >= f->max_size &&
           !f->pktq->abort_request) {
        CondWait(f->cond, f->mutex);
    }
    MutexUnlock(f->mutex);

    if (f->pktq->abort_request) {
        return NULL;
    }

    return &f->queue[f->windex];
}

/**
 * 查找可读帧
 * @param f
 * @return
 */
Frame *frame_queue_peek_readable(FrameQueue *f) {
/* wait until we have a readable a new frame */
    MutexLock(f->mutex);
    while (f->size - f->rindex_shown <= 0 &&
           !f->pktq->abort_request) {
        CondWait(f->cond, f->mutex);
    }
    MutexUnlock(f->mutex);

    if (f->pktq->abort_request) {
        return NULL;
    }

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

/**
 * 入队
 * @param f
 */
void frame_queue_push(FrameQueue *f) {
    if (++f->windex == f->max_size) {
        f->windex = 0;
    }
    MutexLock(f->mutex);
    f->size++;
    CondSignal(f->cond);
    MutexUnlock(f->mutex);
}

/**
 * 下一帧
 * @param f
 */
void frame_queue_next(FrameQueue *f) {
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size) {
        f->rindex = 0;
    }
    MutexLock(f->mutex);
    f->size--;
    CondSignal(f->cond);
    MutexUnlock(f->mutex);
}

/**
 * 剩余帧
 * @param f
 * @return
 */
int frame_queue_nb_remaining(FrameQueue *f) {
    return f->size - f->rindex_shown;
}

/**
 * 上一个位置
 * @param f
 * @return
 */
int64_t frame_queue_last_pos(FrameQueue *f) {
    Frame *fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == f->pktq->serial) {
        return fp->pos;
    } else {
        return -1;
    }
}

#ifdef __cplusplus
};
#endif