/*! @File        :
 *  @Brief       :
 *  @Details     :
 *  @Author      : wenrui.Zhong
 *  @Date        :
 *  @Version     :
 *  @Copyright   :
 **********************************************************/
extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
    #include <libavutil/log.h>
}

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define NOT_TS_FILE 1

static void dump_av_errstr(const int ret)
{
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    char errors[512] = {0};
    av_strerror(ret, errors, sizeof(errors)-1);
    printf("errors: %s\n", errors);
}

int main(int argc, char** argv)
{

    if(argc != 3)
    {
        printf("Usage: %s [input] [h264_out]\n", argv[0]);
        return -1;
    }

    int ret = -1;
    char *input = argv[1];
    char *output = argv[2];
    FILE* fp = NULL;

    int video_index = 0;
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkg;
    AVBSFContext *bsf_ctx = NULL;

    int input_size = 0;
    int out_size = 0;

    av_init_packet(&pkg);

    printf("input:%s, output:%s\n", input, output);

    //set loglevel
    av_log_set_level(AV_LOG_DEBUG);

    fp = fopen(output, "wb");
    if(!fp)
    {
        printf("open file %s failed. reason: %s\n", input, strerror(errno));
        return -1;
    }

    ret = avformat_open_input(&ifmt_ctx, input, NULL, NULL);
    if(ret < 0)
    {
        dump_av_errstr(ret);
        goto finished;
    }

    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if(ret < 0)
    {
        dump_av_errstr(ret);
        goto finished;
    }

    //dump information
    av_dump_format(ifmt_ctx, 0, input, 0);

    // init package
    av_init_packet(&pkg);

    video_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if(video_index < 0)
    {
        dump_av_errstr(video_index);
        goto finished;
    }

    // H264 has two packaging methods. 'MP4' and 'Annex B'
    // 'MP4': Generally, mp4 mkv is in mp4 mode, without startcode, SPS and PPS and other information
    //        are encapsulated in the container, the first 4 bytes of each frame is the length of the frame
    // 'Annex B': There is startcode, SPS and PPS are in ES, and h264 is displayed in the encoder information opened in vlc


    // 'Note': If you convert the output ts file, set a filter for it

#if NOT_TS_FILE
    ///<  'MP4' --> 'Annex B'
    {
        const AVBitStreamFilter *bsfilter = av_bsf_get_by_name("h264_mp4toannexb");
        if(!bsfilter)
        {
            printf("get filter h264_mp4toannexb bu name failed\n");
            goto finished;
        }

        ret = av_bsf_alloc(bsfilter, &bsf_ctx);
        if(ret)
        {
            dump_av_errstr(ret);
            goto finished;
        }

        ret = avcodec_parameters_copy(bsf_ctx->par_in, ifmt_ctx->streams[video_index]->codecpar);
        if(ret < 0)
        {
            dump_av_errstr(ret);
            goto finished;
        }

        ret = av_bsf_init(bsf_ctx);
        if(ret < 0)
        {
            dump_av_errstr(ret);
            goto finished;
        }
    }
#endif

    while(1)
    {
        // FIXME: One package can contain muitiple h264 units
        ret = av_read_frame(ifmt_ctx, &pkg);
        if(ret)
        {
            dump_av_errstr(ret);
            goto finished;
        }

        if(pkg.stream_index != video_index)
        {
            av_packet_unref(&pkg);
            continue;
        }

#if NOT_TS_FILE
        input_size = 0;
        out_size   = 0;

        input_size = pkg.size;

        if (av_bsf_send_packet(bsf_ctx, &pkg) != 0)
        {
            av_packet_unref(&pkg);
            continue;
        }
        else
        {
            av_packet_unref(&pkg);
        }

        while(av_bsf_receive_packet(bsf_ctx, &pkg) == 0)
        {
              out_size++;
              size_t size = fwrite(pkg.data, 1, pkg.size, fp);
              assert(size == pkg.size);
              av_packet_unref(&pkg);
        }

        if(out_size >= 2)
        {
            printf("cur pkt(size:%d) only get 1 out pkt, it get %d pkts\n",
                   input_size, out_size);
        }
#else   ///< for ts, it is Annex B, don't need to use filter to conver it
        size_t size = fwrite(pkg.data, 1, pkg.size, fp);
        assert(size == pkg.size);
        av_packet_unref(&pkg);
#endif

    }

finished:
    av_packet_unref(&pkg);

    if(fp){fclose(fp);}
    if(ifmt_ctx){avformat_close_input(&ifmt_ctx);}
    if(bsf_ctx) {av_bsf_free(&bsf_ctx);}

    return 0;
}
