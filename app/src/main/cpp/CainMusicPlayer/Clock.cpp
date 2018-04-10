//
// Created by admin on 2018/3/29.
//

#include "Clock.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 获取时钟
 * @param c
 * @return
 */
double get_clock(Clock *c) {
    if (*c->queue_serial != c->serial) {
        return NAN;
    }
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

/**
 * 设置时钟
 * @param c
 * @param pts
 * @param serial
 * @param time
 */
void set_clock_at(Clock *c, double pts, int serial, double time) {
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

/**
 * 设置时钟
 * @param c
 * @param pts
 * @param serial
 */
void set_clock(Clock *c, double pts, int serial) {
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

/**
 * 设置时钟速度
 * @param c
 * @param speed
 */
void set_clock_speed(Clock *c, double speed) {
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

/**
 * 初始化时钟
 * @param c
 * @param queue_serial
 */
void init_clock(Clock *c, int *queue_serial) {
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

/**
 * 同步到从属时钟
 * @param c
 * @param slave
 */
void sync_clock_to_slave(Clock *c, Clock *slave) {
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD)) {
        set_clock(c, slave_clock, slave->serial);
    }
}

#ifdef __cplusplus
};
#endif