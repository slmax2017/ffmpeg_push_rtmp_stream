#include <iostream>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/error.h"
#include "libavutil/time.h"
}
using namespace std;


//#define IN_FILE_NAME "rtmp://202.69.69.180:443/webcast/bshdlive-pc"
#define IN_FILE_NAME "cuc_ieschool.flv"
#define OUT_URL "cuc_ieschool.avi"


int err;
char buf[1024] = { 0 };

#define  ptr_check(x) \
            do {\
                if (!x){\
                    printf("operator fail"); \
                    return -1; \
                }\
            }while(0) 

#define void_handle(x) \
            if ((err = (x)) < 0) {\
                memset(buf, 0, 1024); \
                av_strerror(err, buf, 1024); \
                printf(buf); \
                return -1; \
            }

#define ret_handle(x, r) \
            if ((r = (x)) < 0) {\
                memset(buf, 0, 1024); \
                av_strerror(r, buf, 1024); \
                printf(buf); \
                return -1; \
            }       




int main()
{
    AVFormatContext *in_ctx = NULL, *out_ctx = NULL;

    void_handle(avformat_open_input(&in_ctx, IN_FILE_NAME, NULL, NULL));
    if (!in_ctx->nb_streams) void_handle(avformat_find_stream_info(in_ctx, NULL));
    
    void_handle(avformat_alloc_output_context2(&out_ctx, NULL, "flv", OUT_URL));

    //查找流索引
    int vedio_index = -1, audio_index = -1;
    ret_handle(av_find_best_stream(in_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0), vedio_index);
    ret_handle(av_find_best_stream(in_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0), audio_index);

    //创建输出流
    for (int i = 0; i < in_ctx->nb_streams; ++i) {
        AVStream *in_stream = in_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(out_ctx, in_stream->codec->codec);

        void_handle(avcodec_parameters_to_context(out_stream->codec, in_stream->codecpar));

        out_stream->codec->codec_tag = 0;
        if (out_stream->codec->flags & AVFMT_GLOBALHEADER) {
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    //是否需要打开输出文件
    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        void_handle(avio_open(&out_ctx->pb, OUT_URL, AVIO_FLAG_READ_WRITE));
    }

    //写文件头
    void_handle(avformat_write_header(out_ctx, NULL));

    int64_t start = av_gettime();
    AVPacket *packet = av_packet_alloc();
    int index = 0;
    //推流
    while (1) {

        //读取一帧原始数据
        if (av_read_frame(in_ctx, packet) < 0) break;

        //pkt时间转换成ffmpeg系统时间,如果还未到显示时间,则等待显示时间
        AVRational ffmpeg_time_base = { 1, AV_TIME_BASE };
        int64_t pkt_time = av_rescale_q(packet->pts, in_ctx->streams[vedio_index]->time_base, ffmpeg_time_base);
        if (packet->stream_index == vedio_index) {
            int64_t nowTime = av_gettime() - start;
            if (pkt_time > nowTime) {
              //  av_usleep(pkt_time - nowTime);
            }
        }

        //转换时间基
        av_packet_rescale_ts(packet,
            in_ctx->streams[packet->stream_index]->time_base,
            out_ctx->streams[packet->stream_index]->time_base);

        if (packet->stream_index == vedio_index) {
            printf("packet->pts = %lld, ", packet->pts);     //
            printf("index = %d, packet->dts = %lld \n", index++, packet->dts);
        }

        //音视频分开
        void_handle(av_interleaved_write_frame(out_ctx, packet));


        av_packet_unref(packet);
    }

    //写文件尾
    void_handle(av_write_trailer(out_ctx));

    avformat_close_input(&in_ctx);
    avformat_free_context(out_ctx);
    av_packet_free(&packet);

    cout << "over";
    getchar();      
    return 0;
}