//
// Created by admin on 2018/3/29.
//

#ifndef MUSICPLAYER_CLOCK_H
#define MUSICPLAYER_CLOCK_H

#include "ffplay_def.h"

#ifdef __cplusplus
extern "C" {
#endif
// 获取时钟
double get_clock(Clock *c);

// 设置时钟
void set_clock_at(Clock *c, double pts, int serial, double time);
// 设置时钟
void set_clock(Clock *c, double pts, int serial);

// 设置时钟速度
void set_clock_speed(Clock *c, double speed);
// 初始化时钟
void init_clock(Clock *c, int *queue_serial);
// 同步到从属时钟
void sync_clock_to_slave(Clock *c, Clock *slave);

#ifdef __cplusplus
};
#endif

#endif //MUSICPLAYER_CLOCK_H
