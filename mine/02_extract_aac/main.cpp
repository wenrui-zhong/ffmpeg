/*! @File        :
 *  @Brief       : Get AAC data and save it to file with ADTS container
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

const int sampling_frequencies[] = {
    96000,  // 0x0
    88200,  // 0x1
    64000,  // 0x2
    48000,  // 0x3
    44100,  // 0x4
    32000,  // 0x5
    24000,  // 0x6
    22050,  // 0x7
    16000,  // 0x8
    12000,  // 0x9
    11025,  // 0xa
    8000    // 0xb
};

/**
 * @brief build_asts_header
 * @param[out] header_buf
 * @param[in] profile
 * @param[in] samplerate
 * @param[in] channels
 * @param[in] data_length
 * @return
 * - success: 0
 * - failed: -1
 */
static int build_asts_header( char* header_buf,
                            const int profile,
                            const int samplerate,
                            const int channels,
                            const int data_length)
{

    int sampling_frequency_index = 3; //48000
    int i = 0;
    int samplerate_arry_size =  sizeof(sampling_frequencies)/sizeof(sampling_frequencies[0]);
    int frame_len = 7 + data_length;  // According the protocol, if there is not CRC, the header size if 7

    for( i = 0; i < samplerate_arry_size; i++)
    {
        if(samplerate == sampling_frequencies[i])
        {
            sampling_frequency_index = i;
            break;
        }
    }

    if(i == samplerate_arry_size)
    {
        printf("get index by samplerate %d failed. samplerate_arry_size: %d\n", samplerate, samplerate_arry_size);
        return -1;
    }

    //FIXME: Since we have memset the buffer, if the bit position is 0, we can don't set it

    /**
      * The syncword is always 0xFFF
      * ID=0: MPEG-4, ID=1: MPEG-2
      * layer: always 0
      * protection_absent, = 0: enable CRC check, = 1: without CRC check
     */

    ///<[0-7] syncword:8
    header_buf[0] = 0xFF;

    ///<[8-15]syncword:4 + ID:1 + layer:2 + protection_absent:1
    ///  0xF0              0x00   0x00      0x01
    ///////////////////////////////////////////////////////////

    header_buf[1] |= 0xF0;
    header_buf[1] |= 0x01;

    ///<[16-23] profile:2 + sampling_frequency_index:4 + private_bit:1 + chanel_configuration:1
    ///                                                  ox00
    ///////////////////////////////////////////////////////////////////////////////////////////

    header_buf[2] = (profile & 0x03)     << 6;
    header_buf[2] |= (sampling_frequency_index & 0xF)  << 2;
    header_buf[2] |= (channels & 0x04) >> 2;

    ///<[24-31] chanel_configuration:2 + original_copy:1 + home:1 + + copyright_identification_bit:1 + copyright_identification_start:1 + aac_frame_lenght:2
    ///                                  0x00              0x000      0x00                             0x00
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    header_buf[3] =  (channels & 0x03) << 6;
    header_buf[3] |= (data_length & 0x1800) >> 11;

    ///<[32-39] aac_frame_lenght:8
    header_buf[4] = (frame_len & 0x7F8) >> 3;

    ///<[40-47] aac_frame_lenght:3 + adts_buffer_fullness:5
    header_buf[5] = ((frame_len & 0x07) << 5);
    header_buf[5] |= 0x7F & 0x1F;

    /**
      * AAC frame size = number_of_raw_data_blocks_in_frame + 1
      */

    ///<[48-55] adts_buffer_fullness:6 + number_of_raw_data_blocks_in_frame:2
    header_buf[6] |= ((0x7F & 0x3F) << 2);
    header_buf[6] |= 0;

    return 0;
}

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        printf("Usgae: %s [input] [aac_results]\n", argv[0]);
        return -1;
    }

    ///< to get ffmpeg kernal error information
    char errors[1024] = {0};

    char adts_header_buf[7] = {0}; // If there is not CRC checking, the header size if 7 Bytes

    char *input = argv[1];
    char *output = argv[2];

    printf("input %s and output %s\n", input, output);

    int audio_index = 0;

    int ret = -1;
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkg;

    size_t w_szie = 0;

    FILE* fd = fopen(output, "wb");
    if(!fd)
    {
        printf("open file %s failed. reason: %s\n", output,  strerror(errno));
        goto failed;
    }

    //set loglevel
    av_log_set_level(AV_LOG_DEBUG);

    //ffmpeg open the video stream
    // just read header
    ret = avformat_open_input(&ifmt_ctx, input, NULL, NULL);
    if(ret)
    {
        av_strerror(ret, errors, sizeof(errors));
        printf("avformat open %s failed. reason: %s\n", input, errors);
        goto failed;
    }

    //read frame and get medta data to prase
    //and get the decoder information
    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if(ret < 0)
    {
        av_strerror(ret, errors, sizeof(errors));
        printf("find stream info failed, reason: %s\n", errors);
        goto failed;
    }

    //dump information
    av_dump_format(ifmt_ctx, 0, input, 0);

    // init package
    av_init_packet(&pkg);


    /** FIXME: I am not sure what this function do */
    audio_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if(AVERROR_STREAM_NOT_FOUND == audio_index)
    {
        printf("can not find audio stream index from stream\n");
        goto failed;
    }

    // AAC information
    printf("=========> audio profile:%d, FF_PROFILE_AAC_LOW:%d <=========\n",
           ifmt_ctx->streams[audio_index]->codecpar->profile,
           FF_PROFILE_AAC_LOW);

    //get audio pkg
    while(1)
    {
        ret = av_read_frame(ifmt_ctx, &pkg);
        if(ret < 0)
        {
            av_strerror(ret, errors, sizeof(errors));
            printf("read frame failed. reason: %s\n", errors);
            break;
        }

        if(pkg.stream_index != audio_index)
        {
            continue;
        }

        //Build ADTS header information
        memset(adts_header_buf, 0, sizeof(adts_header_buf));
        ret = build_asts_header(adts_header_buf,
                                ifmt_ctx->streams[audio_index]->codecpar->profile,
                                ifmt_ctx->streams[audio_index]->codecpar->sample_rate,
                                ifmt_ctx->streams[audio_index]->codecpar->channels,
                                pkg.size);
        if(ret < 0)
        {
            printf("build ADTS header failed\n");
            goto failed;
        }

        // write ADTS header
        w_szie  = fwrite(adts_header_buf, 1, sizeof(adts_header_buf), fd);
        assert(w_szie == sizeof(adts_header_buf));

        // write ES
        w_szie = fwrite(pkg.data, 1, pkg.size, fd);
        assert(w_szie == pkg.size);

    }

    ret = 0;
    goto finished;

failed:
    ret = -1;
finished:
    if(ifmt_ctx)
    {
        avformat_close_input(&ifmt_ctx);
    }
    av_packet_unref(&pkg);
    if(fd)
    {
        fclose(fd);
    }
    return ret;
}
