//
// Created by admin on 2018/3/28.
//

#include "MusicPlayer.h"

#include "PacketQueue.h"
#include "FrameQueue.h"
#include "MediaDecoder.h"
#include "Clock.h"

AVPacket flush_pkt;

MusicPlayer::MusicPlayer() {
    audioState = (AudioState *)av_mallocz(sizeof(AudioState));
    memset(audioState, 0, sizeof(AudioState));
    fileName = NULL;
    mSoundTouchWrapper = new SoundTouchWrapper();
}

MusicPlayer::~MusicPlayer() {
    if (mSoundTouchWrapper) {
        delete(mSoundTouchWrapper);
        mSoundTouchWrapper = NULL;
    }
}


/**
 * 设置数据源
 * @param path
 */
void MusicPlayer::setDataSource(const char *path) {
    if (fileName) {
        free(fileName);
        fileName = NULL;
    }
    fileName = av_strdup(path);
}



/**
 * 获取当前位置
 * @return
 */
long MusicPlayer::getCurrentPosition() {
    if (!audioState || !audioState->ic) {
        return 0;
    }

    int64_t start_time = audioState->ic->start_time;
    int64_t start_diff = 0;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE) {
        start_diff = av_rescale(start_time, 1000, AV_TIME_BASE);
    }

    int64_t pos = 0;
    double pos_clock = get_master_clock(audioState);
    if (isnan(pos_clock)) {
        pos = av_rescale(audioState->seek_pos, 1000, AV_TIME_BASE);
    } else {
        pos = pos_clock * 1000;
    }

    // 备注：这是经过调整的时长，如果不需要调整的话，直接返回pos的位置就好
    if (pos < 0 || pos < start_diff) {
        return 0;
    }
    int64_t adjust_pos = pos - start_diff;
    return (long)adjust_pos;
}

/**
 * 获取时长(ms)
 * @return
 */
long MusicPlayer::getDuration() {
    if (!audioState || !audioState->ic) {
        return 0;
    }
    // 将ffmpeg中的duration 转成毫秒
    int64_t duration = av_rescale(audioState->ic->duration, 1000, AV_TIME_BASE);
    if (duration < 0) {
        return 0;
    }

    return (long)duration;
}

/**
 * 是否循环播放
 * @return
 */
bool MusicPlayer::isLooping() {
    return loop == 1;
}

/**
 * 是否正在播放
 * @return
 */
bool MusicPlayer::isPlaying() {
    if (audioState) {
        return !audioState->abort_request && !audioState->paused;
    }
    return false;
}

/**
 * 是否处于暂停状态
 * @return
 */
bool MusicPlayer::isStopped() {
    if (audioState) {
        return audioState->paused == 1;
    }
    return true;
}

/**
 * 暂停
 */
void MusicPlayer::pause() {
    if (audioState) {
        set_clock(&audioState->extClock, get_clock(&audioState->extClock), audioState->extClock.serial);
        audioState->paused = audioState->audioClock.paused = audioState->extClock.paused = 1;
    }
}

/**
 * 开始
 */
void MusicPlayer::start() {
    if (audioState) {
        audioState->abort_request = 0;
        audioState->paused = 0;
    }
}

/**
 * 停止
 */
void MusicPlayer::stop() {
    if (audioState) {
        audioState->abort_request = 1;
    }
}

/**
 * 准备
 */
int MusicPlayer::prepare() {
    if (fileName != NULL && audioState) {

        av_log_set_flags(AV_LOG_SKIP_REPEATED);

        // 注册所有解码器、解复用器和协议
        av_register_all();
        avformat_network_init();

        // 初始化裸数据包
        av_init_packet(&flush_pkt);
        flush_pkt.data = (uint8_t *)&flush_pkt;

        if (!mAudioOutput) {
            mAudioOutput = new AudioOutput();
        }
        mAudioOutput->initAudioOutput();

        // 打开媒体流
        int ret = stream_open(fileName);
        if (ret != 0) {
            av_log(NULL, AV_LOG_FATAL, "Failed to initialize AudioState!\n");
            do_exit(NULL);
        }
        return ret;
    }
}

/**
 * 重置
 */
void MusicPlayer::reset() {

}

/**
 * 回收资源
 */
void MusicPlayer::release() {
    stop();
}

/**
 * 定位
 * @param msec
 * @return
 */
int MusicPlayer::seekTo(long msec) {
    int64_t start_time = 0;
    // 将毫秒转成ffmpeg的值
    int64_t seek_pos = av_rescale(msec, AV_TIME_BASE, 1000);
    int64_t duration = av_rescale(getDuration(), AV_TIME_BASE, 1000);

    if (duration > 0 && seek_pos >= duration && accurate_seek) {
        // TODO 发送暂停消息

        return 0;
    }

    start_time = audioState->ic->start_time;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE) {
        seek_pos += start_time;
    }
    stream_seek(audioState, seek_pos, 0, 0);
    return 0;
}

/**
 * 设置是否循环播放
 * @param loop
 */
void MusicPlayer::setLooping(bool loop) {
    this->loop = loop ? 1 : 0;
}

/**
 * 设置播放速度
 * @param playbackRate
 */
void MusicPlayer::setPlaybackRate(float playbackRate) {
    this->playbackRate = playbackRate;
}

/**
 * 设置音调
 * @param pitch
 */
void MusicPlayer::setPlaybackPitch(float pitch) {
    this->playbackPitch = pitch;
}

/**
 * 设置精确查找
 * @param accurateSeek
 */
void MusicPlayer::setAccurateSeek(bool accurateSeek) {
    this->accurate_seek = accurateSeek ? 1 : 0;
}

/**
 * 设置自动退出
 * @param autioExit
 */
void MusicPlayer::setAutoExit(bool autoExit) {
    this->autoexit = autoExit ? 1 : 0;
}

/**
 * 设置无线缓冲区
 * @param infinite_buffer
 */
void MusicPlayer::setInfiniteBuffer(bool infinite_buffer) {
    this->infinite_buffer = infinite_buffer ? 1 : 0;
}

/**
 * 打开媒体流
 * @param audioState
 * @param stream_index
 * @return
 */
int MusicPlayer::stream_component_open(AudioState *is, int stream_index) {
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = lowres;

    if (stream_index < 0 || stream_index >= ic->nb_streams) {
        return -1;
    }

    // 创建解码上下文
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx) {
        return AVERROR(ENOMEM);
    }

    // 复制上下文参数
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0) {
        goto fail;
    }

    // 设定时钟基准
    av_codec_set_pkt_timebase(avctx, ic->streams[stream_index]->time_base);

    // 创建解码器
    codec = avcodec_find_decoder(avctx->codec_id);
    // 判断是否成功得到解码器
    if (!codec) {
        av_log(NULL, AV_LOG_WARNING,
               "No codec could be found with id %d\n", avctx->codec_id);
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    if(stream_lowres > av_codec_get_max_lowres(codec)){
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder audioState %d\n",
               av_codec_get_max_lowres(codec));
        stream_lowres = av_codec_get_max_lowres(codec);
    }
    av_codec_set_lowres(avctx, stream_lowres);

#if FF_API_EMU_EDGE
    if(stream_lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif
    if (fast) {
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;
    }
#if FF_API_EMU_EDGE
    if(codec->capabilities & AV_CODEC_CAP_DR1) {
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
    }
#endif

    // 打开解码器
    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) {
        goto fail;
    }

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            // 计算采样率、声道等
            sample_rate    = avctx->sample_rate;
            nb_channels    = avctx->channels;
            channel_layout = avctx->channel_layout;
            /* prepare audio output */
            if ((ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0) {
                goto fail;
            }
            is->audio_hw_buf_size = ret;
            is->audio_src = is->audio_tgt;
            is->audio_buf_size  = 0;
            is->audio_buf_index = 0;

            /* init averaging filter */
            is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
            is->audio_diff_avg_count = 0;
            /* since we do not have a precise anough audio FIFO fullness,
               we correct audio sync only if larger than this threshold */
            is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

            is->audioStreamIdx = stream_index;
            is->audioStream = ic->streams[stream_index];

            // 初始化解码器
            decoder_init(&is->audioDecoder, avctx, &is->audioQueue, is->readCondition);
            if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK))
                && !is->ic->iformat->read_seek) {
                is->audioDecoder.start_pts = is->audioStream->start_time;
                is->audioDecoder.start_pts_tb = is->audioStream->time_base;
            }
            // 开启解码线程
            if ((ret = decoder_start(&is->audioDecoder, audio_thread, this)) < 0) {
                goto out;
            }
            if (mAudioOutput) {
                mAudioOutput->pauseAudio(0);
            }
            break;

        default:
            break;
    }
    goto out;

    fail:
    avcodec_free_context(&avctx);
    out:
    return ret;
}

/**
 * 打开媒体流
 * @param filename
 * @param iformat
 * @return
 */
int MusicPlayer::stream_open(const char *filename) {
    audioState->filename = av_strdup(filename);
    if (!audioState->filename) {
        return -1;
    }

    // 初始化帧队列
    if (frame_queue_init(&audioState->audioFrameQueue, &audioState->audioQueue, SAMPLE_QUEUE_SIZE, 1) < 0) {
        return -1;
    }
    // 初始化裸数据包队列
    if (packet_queue_init(&audioState->audioQueue) < 0) {
        return -1;
    }
    // 创建读文件条件锁
    if (!(audioState->readCondition = CondCreate())) {
        av_log(NULL, AV_LOG_FATAL, "CondCreate(): %s\n", SDL_GetError());
        return -1;
    }
    // 初始化时钟
    init_clock(&audioState->audioClock, &audioState->audioQueue.serial);
    init_clock(&audioState->extClock, &audioState->extClock.serial);
    audioState->audio_clock_serial = -1;

    // 计算起始音量大小
    if (startup_volume < 0) {
        av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", startup_volume);
    }
    if (startup_volume > 100) {
        av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", startup_volume);
    }
    startup_volume = av_clip(startup_volume, 0, 100);
    startup_volume = av_clip(MIX_MAXVOLUME * startup_volume / 100, 0, MIX_MAXVOLUME);
    audioState->audio_volume = startup_volume;
    audioState->muted = 0;
    audioState->av_sync_type = av_sync_type;

    // 创建读文件线程
    audioState->readThread     = ThreadCreate(read_thread,  this, "read_thread");
    if (!audioState->readThread) {
        av_log(NULL, AV_LOG_FATAL, "ThreadCreate(): %s\n", SDL_GetError());
        return -1;
    }
    return 0;
}

/**
 * 关闭媒体流
 * @param audioState
 * @param stream_index
 */
void MusicPlayer::stream_component_close(AudioState *is, int stream_index) {
    AVFormatContext *ic = is->ic;
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            decoder_abort(&is->audioDecoder, &is->audioFrameQueue);
            if (mAudioOutput) {
                mAudioOutput->freeAudio();
            }
            decoder_destroy(&is->audioDecoder);
            swr_free(&is->swr_ctx);
            av_freep(&is->audio_buf1);
            is->audio_buf1_size = 0;
            is->audio_buf = NULL;
            break;

        default:
            break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            is->audioStream = NULL;
            is->audioStreamIdx = -1;
            break;

        default:
            break;
    }
}

/**
 * 关闭媒体流
 * @param audioState
 */
void MusicPlayer::stream_close(AudioState *is) {
    // 等待读文件线程退出
    is->abort_request = 1;
    ThreadWait(is->readThread, NULL);

    /* close each stream */
    if (is->audioStreamIdx >= 0) {
        stream_component_close(is, is->audioStreamIdx);
    }
    // 关闭输入上下文
    avformat_close_input(&is->ic);

    // 销毁裸数据包队列
    packet_queue_destroy(&is->audioQueue);
    frame_queue_destory(&is->audioFrameQueue);

    // 销毁读文件条件锁
    CondDestroy(is->readCondition);
    av_free(is->filename);
    av_free(is);
}

/**
 * 退出播放器
 */
void MusicPlayer::exitPlayer() {
    do_exit(audioState);
}

/**
 * 退出播放器
 * @param audioState
 */
void MusicPlayer::do_exit(AudioState *is) {
    if (is) {
        stream_close(is);
    }
    avformat_network_deinit();
    // TODO 退出时这里不能用exit，但去掉的话，需要完全释放，如果不完全释放可能会产生驱动崩溃的情况。
    // exit(0);
}

/**
 * 获取主同步类型
 * @param audioState
 * @return
 */
int MusicPlayer::get_master_sync_type(AudioState *is) {
   if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audioStream) {
            return AV_SYNC_AUDIO_MASTER;
        } else {
            return AV_SYNC_EXTERNAL_CLOCK;
        }
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/**
 * 获取主时钟
 * @param audioState
 * @return
 */
double MusicPlayer::get_master_clock(AudioState *is) {
    double val;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&is->audioClock);
            break;
        default:
            val = get_clock(&is->extClock);
            break;
    }
    return val;
}


/**
 * 定位
 * @param audioState
 * @param pos
 * @param rel
 * @param seek_by_bytes
 */
void MusicPlayer::stream_seek(AudioState *is, int64_t pos, int64_t rel, int seek_by_bytes) {
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes) {
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        }
        is->seek_req = 1;
        CondSignal(is->readCondition);
    }
}


int MusicPlayer::read_thread(void *arg) {
    MusicPlayer * player = (MusicPlayer *) arg;
    return player->demux();
}

int MusicPlayer::audio_thread(void *arg) {
    MusicPlayer * player = (MusicPlayer *) arg;
    return player->audioDecode();
}

/**
 * 解复用
 * @return
 */
int MusicPlayer::demux() {
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int audioIdx = -1;
    AVPacket pkt1, *pkt = &pkt1;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    Mutex *wait_mutex = MutexCreate();
    int64_t pkt_ts;

    // 创建等待互斥锁
    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "MutexCreate(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    audioState->audioStreamIdx = -1;
    audioState->eof = 0;

    // 创建解复用上下文
    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    // 设置解复用中断回调
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = audioState;


    // 打开视频文件
    err = avformat_open_input(&ic, audioState->filename, NULL, NULL);
    if (err < 0) {
        ret = -1;
        goto fail;
    }

    audioState->ic = ic;

    if (genpts) {
        ic->flags |= AVFMT_FLAG_GENPTS;
    }
    av_format_inject_global_side_data(ic);
    // 查找媒体流信息
    err = avformat_find_stream_info(ic, NULL);

    if (err < 0) {
        av_log(NULL, AV_LOG_WARNING,
               "%s: could not find codec parameters\n", audioState->filename);
        ret = -1;
        goto fail;
    }

    if (ic->pb) {
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end
    }

    // 判断是否以字节方式定位
    if (seek_by_bytes < 0) {
        seek_by_bytes = ((ic->iformat->flags & AVFMT_TS_DISCONT) != 0)
                        && strcmp("ogg", ic->iformat->name);
    }

    // 如果不是从头开始播放，则跳转播放位置
    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        // 定位seekTo
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                   audioState->filename, (double)timestamp / AV_TIME_BASE);
        }
    }

    // 判断是否属于实时流
    audioState->realtime = is_realtime(ic);
    // 查找媒体流
    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type == AVMEDIA_TYPE_AUDIO && audioIdx == -1) {
            audioIdx = i;
            break;
        }
    }

    if (audioIdx == -1) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
               audioState->filename);
        ret = -1;
        goto fail;
    }

    // 打开音频流
    stream_component_open(audioState, audioIdx);

    // 如果是实时流，设置无线缓冲区
    if (infinite_buffer < 0 && audioState->realtime) {
        infinite_buffer = 1;
    }

    // 进入解复用阶段
    for (;;) {
        // 停止播放
        if (audioState->abort_request) {
            break;
        }
        // 暂停状态
        if (audioState->paused != audioState->last_paused) {
            audioState->last_paused = audioState->paused;
            if (audioState->paused) { // 如果此时处于暂停状态，则停止读文件
                av_read_pause(ic);
            } else {
                av_read_play(ic);
            }
        }
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        // 如果此时处于暂停状态，并且不是rtsp、mmsh实时流，则延时10毫秒在继续下一轮读操作
        if (audioState->paused &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
#endif
        // 如果处于定位操作状态，则进入定位操作
        if (audioState->seek_req) {
            int64_t seek_target = audioState->seek_pos;
            int64_t seek_min    = audioState->seek_rel > 0 ? seek_target - audioState->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = audioState->seek_rel < 0 ? seek_target - audioState->seek_rel - 2: INT64_MAX;
            // 定位
            ret = avformat_seek_file(audioState->ic, -1, seek_min, seek_target, seek_max, audioState->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", audioState->ic->filename);
            } else {
                if (audioState->audioStreamIdx >= 0) {
                    packet_queue_flush(&audioState->audioQueue);
                    packet_queue_put(&audioState->audioQueue, &flush_pkt);
                }
                if (audioState->seek_flags & AVSEEK_FLAG_BYTE) {
                    set_clock(&audioState->extClock, NAN, 0);
                } else {
                    set_clock(&audioState->extClock, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            audioState->seek_req = 0;
            audioState->eof = 0;
        }

        // 如果队列已满，不需要再继续读了
        if (infinite_buffer < 1 &&
            (audioState->audioQueue.size > MAX_QUEUE_SIZE
             || stream_has_enough_packets(audioState->audioStream, audioState->audioStreamIdx, &audioState->audioQueue))) {
            /* wait 10 ms */
            MutexLock(wait_mutex);
            CondWaitTimeout(audioState->readCondition, wait_mutex, 10);
            MutexUnlock(wait_mutex);
            continue;
        }
        // 如果此时不能处于暂停状态，并且为了播放到结尾了，判断是否需要循环播放。
        if (!audioState->paused &&
            (!audioState->audioStream || (audioState->audioDecoder.finished == audioState->audioQueue.serial && frame_queue_nb_remaining(&audioState->audioFrameQueue) == 0))) {
            if (loop != 1 && (!loop || --loop)) {
                stream_seek(audioState, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
            } else if (autoexit) {
                ret = AVERROR_EOF;
                goto fail;
            }
        }
        // 读出裸数据包
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            // 如果没能读出裸数据包，判断是否是结尾
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !audioState->eof) {
                if (audioState->audioStreamIdx >= 0) {
                    packet_queue_put_nullpacket(&audioState->audioQueue, audioState->audioStreamIdx);
                }
                audioState->eof = 1;
            }
            if (ic->pb && ic->pb->error) {
                break;
            }
            MutexLock(wait_mutex);
            CondWaitTimeout(audioState->readCondition, wait_mutex, 10);
            MutexUnlock(wait_mutex);
            continue;
        } else {
            audioState->eof = 0;
        }
        // 计算pkt的pts是否处于播放范围内
        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        // 播放范围内
        pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                            av_q2d(ic->streams[pkt->stream_index]->time_base) -
                            (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
                            <= ((double)duration / 1000000);
        if (pkt->stream_index == audioState->audioStreamIdx && pkt_in_play_range) {
            packet_queue_put(&audioState->audioQueue, pkt);
        } else {
            av_packet_unref(pkt);
        }
    }

    ret = 0;
    fail:
    if (ic && !audioState->ic) {
        avformat_close_input(&ic);
    }
    MutexDestroy(wait_mutex);
    return 0;
}


/**
 * 音频解码
 * @return
 */
int MusicPlayer::audioDecode() {
    AVFrame *frame = av_frame_alloc();
    Frame *af;
    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    if (!frame) {
        return AVERROR(ENOMEM);
    }

    do {
        if ((got_frame = decoder_decode_frame(&audioState->audioDecoder, frame, NULL)) < 0) {
            goto the_end;
        }

        if (got_frame) {
            tb = (AVRational){1, frame->sample_rate};
            if (!(af = frame_queue_peek_writable(&audioState->audioFrameQueue))){
                goto the_end;
            }

            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            af->pos = av_frame_get_pkt_pos(frame);
            af->serial = audioState->audioDecoder.pkt_serial;
            af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});

            av_frame_move_ref(af->frame, frame);
            frame_queue_push(&audioState->audioFrameQueue);
        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
the_end:
    av_frame_free(&frame);
    return ret;
}

/**
 * 同步音频
 * @param audioState
 * @param nb_samples
 * @return
 */
int MusicPlayer::synchronize_audio(AudioState *is, int nb_samples) {
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&is->audioClock) - get_master_clock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

/**
 * 音频解码
 * @param audioState
 * @return
 */
int MusicPlayer::audio_decode_frame(AudioState *is) {
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;

#if defined(__ANDROID__)
    int translate_time = 1;
#endif

    // 处于暂停状态
    if (is->paused) {
        return -1;
    }
reload:
    do {
        if (!(af = frame_queue_peek_readable(&is->audioFrameQueue))) {
            return -1;
        }
        frame_queue_next(&is->audioFrameQueue);
    } while (af->serial != is->audioQueue.serial);

    data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
                                           af->frame->nb_samples,
                                           (AVSampleFormat)af->frame->format, 1);

    dec_channel_layout =
            (af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout))
            ? af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);

    if (af->frame->format        != is->audio_src.fmt            ||
        dec_channel_layout       != is->audio_src.channel_layout ||
        af->frame->sample_rate   != is->audio_src.freq           ||
        (wanted_nb_samples       != af->frame->nb_samples && !is->swr_ctx)) {
        swr_free(&is->swr_ctx);
        is->swr_ctx = swr_alloc_set_opts(NULL, is->audio_tgt.channel_layout, is->audio_tgt.fmt,
                                         is->audio_tgt.freq, dec_channel_layout,
                                         (AVSampleFormat)af->frame->format, af->frame->sample_rate,
                                         0, NULL);

        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                   af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), av_frame_get_channels(af->frame),
                   is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels       = av_frame_get_channels(af->frame);
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (AVSampleFormat)af->frame->format;
    }

    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                     wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer audioState probably too small\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        int bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt.fmt);
        resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);

        // 变速变调处理
#if defined(__ANDROID__)
        if ((playbackRate != 1.0f || playbackPitch != 1.0f) && !is->abort_request) {
            av_fast_malloc(&is->audio_new_buf, &is->audio_new_buf_size, out_size * translate_time);
            for (int i = 0; i < (resampled_data_size / 2); i++)
            {
                is->audio_new_buf[i] = (is->audio_buf1[i * 2] | (is->audio_buf1[i * 2 + 1] << 8));
            }
            if (!mSoundTouchWrapper) {
                mSoundTouchWrapper = new SoundTouchWrapper();
            }
            int ret_len = mSoundTouchWrapper->translate(is->audio_new_buf, (float)(playbackRate),
                                                        (float)( playbackPitch != 1.0f ? playbackPitch : 1.0f / playbackRate),
                                                        resampled_data_size / 2, bytes_per_sample,
                                                        is->audio_tgt.channels, af->frame->sample_rate);
            if (ret_len > 0) {
                is->audio_buf = (uint8_t*)is->audio_new_buf;
                resampled_data_size = ret_len;
            } else {
                translate_time++;
                goto reload;
            }
        }
#endif
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    /* update the audio clock with the pts */
    if (!isnan(af->pts)) {
        is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    } else {
        is->audio_clock = NAN;
    }
    is->audio_clock_serial = af->serial;
    return resampled_data_size;
}

/**
 * 音频设备回调
 * @param opaque
 * @param stream
 * @param len
 */
void MusicPlayer::sdl_audio_callback(void *opaque, uint8_t *stream, int len) {
    MusicPlayer *player = (MusicPlayer *) opaque;
    player->audio_callback(stream, len);
}

/**
 * 音频设备回调
 * @param stream
 * @param len
 */
void MusicPlayer::audio_callback(uint8_t *stream, int len) {
    int audio_size, len1;

    audio_callback_time = av_gettime_relative();

    while (len > 0) {
        if (audioState->audio_buf_index >= audioState->audio_buf_size) {
            audio_size = audio_decode_frame(audioState);
            if (audio_size < 0) {
                /* if error, just output silence */
                audioState->audio_buf = NULL;
                audioState->audio_buf_size = (unsigned int) (SDL_AUDIO_MIN_BUFFER_SIZE / audioState->audio_tgt.frame_size
                                                     * audioState->audio_tgt.frame_size);
            } else {
                audioState->audio_buf_size = audio_size;
            }
            audioState->audio_buf_index = 0;
        }
        len1 = audioState->audio_buf_size - audioState->audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }
        if (!audioState->muted && audioState->audio_buf && audioState->audio_volume == MIX_MAXVOLUME) {
            memcpy(stream, (uint8_t *) audioState->audio_buf + audioState->audio_buf_index, len1);
        } else {
            memset(stream, 0, len1);
            if (!audioState->muted && audioState->audio_buf) {
//                SDL_MixAudio(stream, (uint8_t *) audioState->audio_buf + audioState->audio_buf_index, len1,
//                             audioState->audio_volume);
            }
        }
        len -= len1;
        stream += len1;
        audioState->audio_buf_index += len1;
    }
    audioState->audio_write_buf_size = audioState->audio_buf_size - audioState->audio_buf_index;
    /* Let's assume the audio driver that audioState used by SDL has two periods. */
    if (!isnan(audioState->audio_clock)) {
        set_clock_at(&audioState->audioClock,
                     audioState->audio_clock - (double)(2 * audioState->audio_hw_buf_size + audioState->audio_write_buf_size) / audioState->audio_tgt.bytes_per_sec,
                     audioState->audio_clock_serial,
                     audio_callback_time / 1000000.0);
        sync_clock_to_slave(&audioState->extClock, &audioState->audioClock);
    }
}

/**
 * 打开音频设备
 * @param opaque
 * @param wanted_channel_layout
 * @param wanted_nb_channels
 * @param wanted_sample_rate
 * @param audio_hw_params
 * @return
 */
int MusicPlayer::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels,
                           int wanted_sample_rate, struct AudioParams *audio_hw_params) {
    AudioSpec wanted_spec, spec;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }

    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq) {
        next_sample_rate_idx--;
    }

    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = this;
    while (mAudioOutput &&  mAudioOutput->openAudio(&wanted_spec, &spec) < 0) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d audioState not supported!\n", spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            av_log(NULL, AV_LOG_ERROR,
                   "SDL advised channel count %d audioState not supported!\n", spec.channels);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    return spec.size;
}


/**
 * 解码中断回调
 * @param ctx
 * @return
 */
int MusicPlayer::decode_interrupt_cb(void *ctx) {
    AudioState *is = (AudioState *)ctx;
    return is->abort_request;
}

/**
 * 判断媒体流中是否存在足够的裸数据包
 * @param st
 * @param stream_id
 * @param queue
 * @return
 */
int MusicPlayer::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return (stream_id < 0) || (queue->abort_request)
           || (st->disposition & AV_DISPOSITION_ATTACHED_PIC)
           || (queue->nb_packets > MIN_FRAMES)
              && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

/**
 * 是否实时流
 * @param s
 * @return
 */
int MusicPlayer::is_realtime(AVFormatContext *s) {
    if (!strcmp(s->iformat->name, "rtp") || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")) {
        return 1;
    }

    if (s->pb && (!strncmp(s->filename, "rtp:", 4) || !strncmp(s->filename, "udp:", 4))) {
        return 1;
    }
    return 0;
}

/**
 * 播放
 * @return
 */
void MusicPlayer::play() {
    if (audioState) {
        audioState->abort_request = 0;
        audioState->paused = 0;
    }
}




