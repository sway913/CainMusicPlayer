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

    playbackRate = 1.0;
    playbackPitch = 1.0;
    playbackTempo = 1.0;
    playbackRateChanged = 0;
    playbackTempoChanged = 0;
    playbackPitchOctaves = 0;
    playbackPitchSemiTones = 0;
}

MusicPlayer::~MusicPlayer() {
    if (mSoundTouchWrapper) {
        delete(mSoundTouchWrapper);
        mSoundTouchWrapper = NULL;
    }
    avformat_network_deinit();
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
    if (!audioState || !audioState->pFormatContext) {
        return 0;
    }

    int64_t start_time = audioState->pFormatContext->start_time;
    int64_t start_diff = 0;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE) {
        start_diff = av_rescale(start_time, 1000, AV_TIME_BASE);
    }

    int64_t pos = 0;
    // 获取音频时钟
    double pos_clock = get_clock(&audioState->audioClock);
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
    if (audioState && audioState->eof) {
        return 0;
    }
    return (long)adjust_pos;
}

/**
 * 获取时长(ms)
 * @return
 */
long MusicPlayer::getDuration() {
    if (!audioState || !audioState->pFormatContext) {
        return 0;
    }
    // 将ffmpeg中的duration 转成毫秒
    int64_t duration = av_rescale(audioState->pFormatContext->duration, 1000, AV_TIME_BASE);
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
    return playLoop == 1;
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
        int ret = openMusicFile(fileName);
        if (ret != 0) {
            av_log(NULL, AV_LOG_FATAL, "Failed to initialize AudioState!\n");
        }
        return ret;
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
    start_time = audioState->pFormatContext->start_time;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE) {
        seek_pos += start_time;
    }
    streamSeek(audioState, seek_pos, 0, 0);
    return 0;
}

/**
 * 设置是否循环播放
 * @param loop
 */
void MusicPlayer::setLooping(bool loop) {
    this->playLoop = loop ? 1 : 0;
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
    CondSignal(audioState->demuxCondition);
}

/**
 * 设置节拍
 * @param tempo
 */
void MusicPlayer::setTempo(double tempo) {
    this->playbackTempo = tempo;
    CondSignal(audioState->demuxCondition);
}

/**
 * 设置速度改变 -50 ~ 100 (%)
 * @param newRate
 */
void MusicPlayer::setRateChange(double newRate) {
    if (newRate < -50) {
        newRate = -50;
    } else if (newRate > 100) {
        newRate = 100;
    }
    this->playbackRateChanged = newRate;
    CondSignal(audioState->demuxCondition);
}

/**
 * 设置节拍改变
 * @param newTempo
 */
void MusicPlayer::setTempoChange(double newTempo) {
    if (newTempo < -50) {
        newTempo = -50;
    } else if (newTempo > 100) {
        newTempo = 100;
    }
    this->playbackTempoChanged = newTempo;
    CondSignal(audioState->demuxCondition);
}

/**
 * 原音调基础上调节八度音
 * @param newPitch
 */
void MusicPlayer::setPitchOctaves(double newPitch) {
    if (newPitch < -1.0) {
        newPitch = -1.0;
    } else if (newPitch > 1.0) {
        newPitch = 1.0;
    }
    this->playbackPitchOctaves = newPitch;
    CondSignal(audioState->demuxCondition);
}

/**
 * 设置半音程
 * @param newPitch
 */
void MusicPlayer::setPitchSemiTones(double newPitch) {
    if (newPitch < -12) {
        newPitch = -12;
    } else if (newPitch > 12) {
        newPitch = 12;
    }
    this->playbackPitchSemiTones = newPitch;
    CondSignal(audioState->demuxCondition);
}

/**
 * 打开媒体流
 * @param stream_index
 * @return
 */
int MusicPlayer::openStream(int stream_index) {
    AVFormatContext *ic = audioState->pFormatContext;
    AVCodecContext *avctx;
    AVCodec *codec;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    // 判断媒体流索引是否合法
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

#if FF_API_EMU_EDGE
    if(codec->capabilities & AV_CODEC_CAP_DR1) {
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
    }
#endif

    // 打开解码器
    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) {
        goto fail;
    }
    //
    audioState->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            // 计算采样率、声道等
            sample_rate    = avctx->sample_rate;
            nb_channels    = avctx->channels;
            channel_layout = avctx->channel_layout;
            // 打开音频设备
            if ((ret = openAudioDevice(channel_layout, nb_channels, sample_rate, &audioState->audio_tgt)) < 0) {
                goto fail;
            }
            audioState->audio_hw_buf_size = ret;
            audioState->audio_src = audioState->audio_tgt;
            audioState->audio_buf_size  = 0;
            audioState->audio_buf_index = 0;
            audioState->audioStreamIdx = stream_index;
            audioState->audioStream = ic->streams[stream_index];

            // 初始化解码器
            decoder_init(&audioState->audioDecoder, avctx, &audioState->audioQueue, audioState->demuxCondition);
            if ((audioState->pFormatContext->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK))
                && !audioState->pFormatContext->iformat->read_seek) {
                audioState->audioDecoder.start_pts = audioState->audioStream->start_time;
                audioState->audioDecoder.start_pts_tb = audioState->audioStream->time_base;
            }
            // 开启解码线程
            if ((ret = decoder_start(&audioState->audioDecoder, audioDecodeThreadHandle, this)) < 0) {
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
int MusicPlayer::openMusicFile(const char *filename) {
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
    if (!(audioState->demuxCondition = CondCreate())) {
        av_log(NULL, AV_LOG_FATAL, "CondCreate(): %s\n", SDL_GetError());
        return -1;
    }

    // 初始化时钟
    init_clock(&audioState->audioClock, &audioState->audioQueue.serial);
    init_clock(&audioState->extClock, &audioState->extClock.serial);
    audioState->audio_clock_serial = -1;

    // 解复用线程
    audioState->demuxThread = ThreadCreate(demuxThreadHandle, this, "demuxThread");
    if (!audioState->demuxThread) {
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
void MusicPlayer::closeStream(int stream_index) {
    AVFormatContext *ic = audioState->pFormatContext;
    AVCodecParameters *codecpar;
    // 判断媒体流索引是否合法
    if (stream_index < 0 || stream_index >= ic->nb_streams) {
        return;
    }
    codecpar = ic->streams[stream_index]->codecpar;
    switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            decoder_abort(&audioState->audioDecoder, &audioState->audioFrameQueue);
            if (mAudioOutput) {
                mAudioOutput->freeAudio();
            }
            decoder_destroy(&audioState->audioDecoder);
            swr_free(&audioState->swr_ctx);
            av_freep(&audioState->audio_buf1);
            audioState->audio_buf1_size = 0;
            audioState->audio_buf = NULL;
            break;

        default:
            break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            audioState->audioStream = NULL;
            audioState->audioStreamIdx = -1;
            break;

        default:
            break;
    }
}

/**
 * 关闭媒体流
 * @param audioState
 */
void MusicPlayer::closeMusicFile() {
    if (!audioState) {
        return;
    }

    // 等待读文件线程退出
    audioState->abort_request = 1;
    ThreadWait(audioState->demuxThread, NULL);

    /* close each stream */
    if (audioState->audioStreamIdx >= 0) {
        closeStream(audioState->audioStreamIdx);
    }

    // 关闭输入上下文
    avformat_close_input(&audioState->pFormatContext);

    // 销毁裸数据包队列
    packet_queue_destroy(&audioState->audioQueue);
    frame_queue_destory(&audioState->audioFrameQueue);

    // 销毁读文件条件锁
    CondDestroy(audioState->demuxCondition);
    // 释放文件名
    av_free(audioState->filename);
    // 释放结构体
    av_free(audioState);
    audioState = NULL;
}

/**
 * 关闭播放器
 */
void MusicPlayer::closePlayer() {
    closeMusicFile();
}

/**
 * 定位
 * @param audioState
 * @param pos
 * @param rel
 * @param seek_by_bytes
 */
void MusicPlayer::streamSeek(AudioState *is, int64_t pos, int64_t rel, int seek_by_bytes) {
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes) {
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        }
        is->seek_req = 1;
        CondSignal(is->demuxCondition);
        if (audioState->audioStreamIdx >= 0) {
            packet_queue_flush(&audioState->audioQueue);
            packet_queue_put(&audioState->audioQueue, &flush_pkt);
        }
    }
}

/**
 * 解复用线程句柄
 * @param arg
 * @return
 */
int MusicPlayer::demuxThreadHandle(void *arg) {
    MusicPlayer * player = (MusicPlayer *) arg;
    return player->demux();
}

/**
 * 音频解码线程句柄
 * @param arg
 * @return
 */
int MusicPlayer::audioDecodeThreadHandle(void *arg) {
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

    audioState->pFormatContext = ic;

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
    openStream(audioIdx);

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
        // 处于暂停状态
        if (audioState->paused) {
            continue;
        }

        // 如果处于定位操作状态，则进入定位操作
        if (audioState->seek_req) {
            int64_t seek_target = audioState->seek_pos;
            int64_t seek_min    = audioState->seek_rel > 0 ? seek_target - audioState->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = audioState->seek_rel < 0 ? seek_target - audioState->seek_rel - 2: INT64_MAX;
            // 定位
            ret = avformat_seek_file(audioState->pFormatContext, -1, seek_min, seek_target, seek_max, audioState->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", audioState->pFormatContext->filename);
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
        if ((audioState->audioQueue.size > MAX_QUEUE_SIZE || stream_has_enough_packets(audioState->audioStream, audioState->audioStreamIdx, &audioState->audioQueue))) {
            /* wait 10 ms */
            MutexLock(wait_mutex);
            CondWaitTimeout(audioState->demuxCondition, wait_mutex, 10);
            MutexUnlock(wait_mutex);
            continue;
        }
        // 如果此时不能处于暂停状态，并且播放到结尾了，判断是否需要循环播放。
        if (!audioState->paused &&
            (!audioState->audioStream || (audioState->audioDecoder.finished == audioState->audioQueue.serial && frame_queue_nb_remaining(&audioState->audioFrameQueue) == 0))) {
            if (playLoop != 1 && (!playLoop || --playLoop)) {
                streamSeek(audioState, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
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
            CondWaitTimeout(audioState->demuxCondition, wait_mutex, 10);
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
    if (ic && !audioState->pFormatContext) {
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
 * 音频解码
 * @param audioState
 * @return
 */
int MusicPlayer::decodeAudioFrame() {
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;
    int translate_time = 1;

    // 处于暂停状态
    if (audioState->paused) {
        return -1;
    }
reload:
    do {
        if (!(af = frame_queue_peek_readable(&audioState->audioFrameQueue))) {
            return -1;
        }
        frame_queue_next(&audioState->audioFrameQueue);
    } while (af->serial != audioState->audioQueue.serial);

    data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
                                           af->frame->nb_samples,
                                           (AVSampleFormat)af->frame->format, 1);
    dec_channel_layout =
            (af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout))
            ? af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
    wanted_nb_samples = af->frame->nb_samples;

    // 判断音频帧格式是否发生变化
    if (af->frame->format        != audioState->audio_src.fmt            ||
        dec_channel_layout       != audioState->audio_src.channel_layout ||
        af->frame->sample_rate   != audioState->audio_src.sampleRate           ||
        (wanted_nb_samples       != af->frame->nb_samples && !audioState->swr_ctx)) {
        // 释放旧的转码上下文
        swr_free(&audioState->swr_ctx);
        // 重新设置转码上下文
        audioState->swr_ctx = swr_alloc_set_opts(NULL, audioState->audio_tgt.channel_layout,
                                                 audioState->audio_tgt.fmt,
                                                 audioState->audio_tgt.sampleRate,
                                                 dec_channel_layout,
                                                 (AVSampleFormat)af->frame->format,
                                                 af->frame->sample_rate,
                                                 0, NULL);

        if (!audioState->swr_ctx || swr_init(audioState->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                   af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), av_frame_get_channels(af->frame),
                   audioState->audio_tgt.sampleRate, av_get_sample_fmt_name(audioState->audio_tgt.fmt), audioState->audio_tgt.channels);
            swr_free(&audioState->swr_ctx);
            return -1;
        }

        // 设置源音频格式
        audioState->audio_src.channel_layout = dec_channel_layout;
        audioState->audio_src.channels       = av_frame_get_channels(af->frame);
        audioState->audio_src.sampleRate = af->frame->sample_rate;
        audioState->audio_src.fmt = (AVSampleFormat)af->frame->format;
    }

    // 如果存在音频转码上下文，则获取数据进行转码
    if (audioState->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &audioState->audio_buf1;
        int out_count = (int)(wanted_nb_samples * audioState->audio_tgt.sampleRate / af->frame->sample_rate + 256);
        int out_size  = av_samples_get_buffer_size(NULL, audioState->audio_tgt.channels, out_count, audioState->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(audioState->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * audioState->audio_tgt.sampleRate / af->frame->sample_rate,
                                     wanted_nb_samples * audioState->audio_tgt.sampleRate / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        // 分配缓冲区内存
        av_fast_malloc(&audioState->audio_buf1, &audioState->audio_buf1_size, out_size);
        if (!audioState->audio_buf1) {
            return AVERROR(ENOMEM);
        }
        // 音频转码
        len2 = swr_convert(audioState->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer audioState probably too small\n");
            if (swr_init(audioState->swr_ctx) < 0)
                swr_free(&audioState->swr_ctx);
        }
        audioState->audio_buf = audioState->audio_buf1;
        int bytes_per_sample = av_get_bytes_per_sample(audioState->audio_tgt.fmt);
        resampled_data_size = len2 * audioState->audio_tgt.channels * av_get_bytes_per_sample(audioState->audio_tgt.fmt);

        // 变速变调变节拍处理
        if (isNeedToAdjustAudio()) {
            av_fast_malloc(&audioState->audio_new_buf, &audioState->audio_new_buf_size, out_size * translate_time);
            for (int i = 0; i < (resampled_data_size / 2); i++) {
                audioState->audio_new_buf[i] = (audioState->audio_buf1[i * 2] | (audioState->audio_buf1[i * 2 + 1] << 8));
            }
            if (!mSoundTouchWrapper) {
                mSoundTouchWrapper = new SoundTouchWrapper();
            }
            // 调整声音
            int ret_len = adjustAudio(audioState->audio_new_buf,
                                      resampled_data_size / 2, bytes_per_sample,
                                      audioState->audio_tgt.channels, af->frame->sample_rate);
            if (ret_len > 0) {
                audioState->audio_buf = (uint8_t*)audioState->audio_new_buf;
                resampled_data_size = ret_len;
            } else {
                translate_time++;
                goto reload;
            }
        }
    } else {
        audioState->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    /* update the audio clock with the pts */
    if (!isnan(af->pts)) {
        audioState->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    } else {
        audioState->audio_clock = NAN;
    }
    audioState->audio_clock_serial = af->serial;
    return resampled_data_size;
}

/**
 * 音频设备回调
 * @param opaque
 * @param stream
 * @param len
 */
void MusicPlayer::audioCallback(void *opaque, uint8_t *stream, int len) {
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
            audio_size = decodeAudioFrame();
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
        // 将缓冲数据复制到stream中播放
        if (audioState->audio_buf) {
            memcpy(stream, (uint8_t *) audioState->audio_buf + audioState->audio_buf_index, len1);
        } else {
            memset(stream, 0, len1);
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
 * @param wanted_channel_layout 期望音频声道格式
 * @param wanted_nb_channels   期望声道数
 * @param wanted_sample_rate   采样率
 * @param audio_hw_params      最终的硬件参数
 * @return
 */
int MusicPlayer::openAudioDevice(int64_t wanted_channel_layout,
                                 int wanted_nb_channels, int wanted_sample_rate,
                                 struct AudioParams *audio_hw_params) {
    AudioSpec wanted_spec, spec;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    if (!wanted_channel_layout
        || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = (uint8_t)wanted_nb_channels;
    wanted_spec.sampleRate = wanted_sample_rate;
    if (wanted_spec.sampleRate <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }

    // 查找合适的采样率
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.sampleRate) {
        next_sample_rate_idx--;
    }

    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE,
                                2 << av_log2(wanted_spec.sampleRate / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = audioCallback;
    wanted_spec.userdata = this;
    // 打开音频设备
    while (mAudioOutput &&  mAudioOutput->openAudio(&wanted_spec, &spec) < 0) {

        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.sampleRate, SDL_GetError());


        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.sampleRate = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.sampleRate) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    // 判断音频输出格式是否正确
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d audioState not supported!\n", spec.format);
        return -1;
    }

    // 声道数不相等，则重新设置声道格式
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            av_log(NULL, AV_LOG_ERROR,
                   "SDL advised channel count %d audioState not supported!\n", spec.channels);
            return -1;
        }
    }

    // 最后得到的音频参数
    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;   // 输出格式是s16
    audio_hw_params->sampleRate = spec.sampleRate;  // 采样率
    audio_hw_params->channel_layout = wanted_channel_layout;    // 声道格式
    audio_hw_params->channels =  spec.channels;                 // 声道数
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1); // 缓冲大小
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->sampleRate, audio_hw_params->fmt, 1); // 每秒字节数

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
           || (queue->nb_packets > MIN_FRAMES) && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}


/**
 * 调整音频数据
 * @param data              原来的PCM数据
 * @param len               PCM数据的长度
 * @param bytes_per_sample  每次采样的字节数
 * @param n_channel         声道
 * @param n_sampleRate      采样率
 * @return                  经过转换后的PCM数据大小
 */
int MusicPlayer::adjustAudio(short *data, int len, int bytes_per_sample, uint channel,
                             uint sampleRate) {
    if (mSoundTouchWrapper == NULL) {
        return 0;
    }
    // 设置采样率
    mSoundTouchWrapper->setSampleRate(sampleRate);
    // 设置声道数
    mSoundTouchWrapper->setChannels(channel);
    // 设置播放速度
    mSoundTouchWrapper->setRate(playbackRate);
    // 设置播放音调
    mSoundTouchWrapper->setPitch(playbackPitch != 1.0f
                                                  ? playbackPitch
                                                  : 1.0f / playbackRate);
    // 设置播放节拍
    mSoundTouchWrapper->setTempo(playbackTempo);
    // 设置速度改变值
    mSoundTouchWrapper->setRateChange(playbackRateChanged);
    // 设置节拍改变值
    mSoundTouchWrapper->setTempoChange(playbackTempoChanged);
    // 设置八度音调节
    mSoundTouchWrapper->setPitchOctaves(playbackPitchOctaves);
    // 设置半音
    mSoundTouchWrapper->setPitchSemiTones(playbackPitchSemiTones);
    // 转换并返回转换后PCM的带下
    return mSoundTouchWrapper->translate(data, len, bytes_per_sample, channel, sampleRate);
}

