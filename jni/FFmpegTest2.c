#include "com_doom119_ffmpegtest_MainActivity.h"
#include <android/log.h>
#include <jni.h>
#include "libavformat/avformat.h"

#define LOG_TAG "doom119"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

typedef struct
{
	// �ֿ�ı�ţ�����¼�ƣ����ɿ�����һ���飩
	int pBlockIndex;
	// �ڼ�֡������豸�ȽϿ���ͬһ��pBlockIndex�е�pFrameIndex���ܲ��������ģ�
	int pFrameIndex;
	// codec��ʽ
	int pCodec;

	// ֡�Ŀ��
	int vWidth;
	// ֡�ĸ߶�
	int vHeight;
	// ����ͷ����һ���ֻ�������ͷ���Ǻ���ģ���������Ҫת90��
	int vOrientation;
	// ֡�ʣ���ÿ�벶���֡��
	int vFPS;
	// ɫ�ʿռ�
	int vColorFormat;
	// ѹ�����������������λkb
	int vBitRate;

	// �����ʣ�ÿ���Ӳ����Ĵ���
	int aSampleRate;
	// ͨ����˫ͨ������ͨ��)
	int aChannel;
	// ������С�����ö���bit��ʾһ��������(PCM����)<br>
	int aPCMFormat;
	// ѹ�����������������λkb
	int aBitRate;
}AVInfo;

AVFormatContext* pFormatContext;
AVOutputFormat* pOutputFormat;
AVCodec* pCodec;
AVStream *pStream;
AVCodecContext* pCodecContext;
AVFrame* pFrame;
int pts = 0;
unsigned char* frameDataBuffer;

JNIEXPORT void JNICALL Java_com_doom119_ffmpegtest_MainActivity_prepareFFmpeg
  (JNIEnv *env, jclass clazz, jstring path)
{
	const char* filename = (*env)->GetStringUTFChars(env, path, 0);
	const char* videoname = "/mnt/sdcard/ffmpeg_build_video.mp4";
	LOGD("filename=%s, videoname=%s", filename, videoname);

	if(initContext(videoname))
		return;

	if(beginWrite(videoname))
		return;

	if(toWrite(filename))
		return;

	endWrite();
}

int endWrite()
{
	av_write_trailer(pFormatContext);

	if(frameDataBuffer != NULL)
		free(frameDataBuffer);

	avcodec_close(pCodecContext);
	av_frame_free(pFrame);
	avio_close(pFormatContext->pb);
	avformat_free_context(pFormatContext);

	return 0;
}

int toWrite(const char* videopath)
{
	unsigned char* srcData;
	int ret = -999;
	AVInfo frameHead;
	AVPacket packet;
	int got_output = 0;

	FILE* videoFile = fopen(videopath, "rb");
	if(!videoFile)
	{
		LOGD("toWrite, open video file error");
		return -3;
	}

	do
	{
		ret = readFrame(videoFile, &frameHead, srcData);
		if(ret < 0)
		{
			LOGD("readFrame error, ret=%d", ret);
			return -1;
		}

		pFrame->width = frameHead.vWidth;
		pFrame->height = frameHead.vHeight;
		pFrame->data[0] = srcData;
		pFrame->data[1] = srcData + frameHead.vWidth * frameHead.vHeight;
		pFrame->data[2] = srcData + frameHead.vWidth * frameHead.vHeight * 5 / 4;
		pFrame->linesize[0] = frameHead.vWidth;
		pFrame->linesize[1] = pFrame->linesize[2] = frameHead.vWidth / 2;
		pFrame->pts = pts++;

		av_init_packet(&packet);
		packet.data = NULL;
		packet.priv = NULL;
		packet.size = 0;

		ret = avcodec_encode_video2(pCodecContext, &packet, pFrame, &got_output);
		if(ret < 0)
		{
			LOGD("avcodec_encode_video2 error, ret=%d, %s", ret, av_err2str(ret));
			av_free_packet(&packet);
			return -2;
		}
	}
	while(1);

	fclose(videoFile);

	return 0;
}

unsigned char* getFrameBuffer(int size)
{
	if(frameDataBuffer != NULL)
	{
		memset(frameDataBuffer, 0, sizeof(unsigned int)*size);
		return frameDataBuffer;
	}
	frameDataBuffer = (unsigned char*)malloc(sizeof(unsigned int)*size);
	return frameDataBuffer;
}

int readFrame(FILE *videoFile, AVInfo* frameHead, unsigned char** pdata)
{
	int ret = -999;

	ret = fread(frameHead, sizeof(AVInfo), 1, videoFile);
	LOGD("read frame head size=%d", ret);
	if(ret <= 0)
	{
		return -1;
	}

	int srcWidth = frameHead->vWidth;
	int srcHeight = frameHead->vHeight;
	int size = srcWidth * srcHeight * 3 / 2;

	unsigned char* data = getFrameBuffer(size);
	if(!data)
	{
		return -2;
	}

	ret = fread(data, size, 1, videoFile);
	LOGD("read frame data size=%d", ret);
	if(ret <= 0)
	{
		return -3;
	}

	*pdata = data;

	return 0;
}

int beginWrite(const char* videopath)
{
	int ret = -999;

	ret = avcodec_open2(pCodecContext, pCodec, NULL);
	if(ret < 0)
	{
		LOGD("avcodec_open2 error, ret=%d, %s", ret, av_err2str(ret));
		return -1;
	}

	pFrame = av_frame_alloc();
	if(!pFrame)
	{
		LOGD("pFrame is NULL");
		return -2;
	}

	if(!(pFormatContext->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&pFormatContext->pb, videopath, AVIO_FLAG_WRITE);
		if(ret < 0)
		{
			LOGD("avio_open error, ret=%d, %s", ret, av_err2str(ret));
			return -3;
		}
	}

	ret = avformat_write_header(pFormatContext, NULL);
	if(ret < 0)
	{
		LOGD("avformat_write_header error, ret=%d, %s", ret, av_err2str(ret));
		return -4;
	}

	pts = 0;

	return 0;
}

int initContext(const char* videopath)
{
	int ret = -999;

	av_register_all();

	ret = avformat_alloc_output_context2(&pFormatContext, NULL, NULL, videopath);
	if (ret < 0)
	{
		LOGD("avformat_alloc_output_context2 error, ret=%d, %s", ret, av_err2str(ret));
		return -1;
	}
	pOutputFormat = pFormatContext->oformat;
	LOGD("video_codec=%d", pOutputFormat->video_codec);

	pCodec = avcodec_find_encoder(pOutputFormat->video_codec);
	if(!pCodec)
	{
		LOGD("avcodec_find_encoder error");
		return -2;
	}

	pStream = avformat_new_stream(pFormatContext, pCodec);
	if(!pStream)
	{
		LOGD("avformat_new_stream error");
		return -3;
	}
	LOGD("pStream->id=%d", pFormatContext->nb_streams);
	pStream->id = pFormatContext->nb_streams;

	pCodecContext = pStream->codec;
	if(!pCodecContext)
	{
		LOGD("pCodecContext is NULL");
		return -4;
	}
	pCodecContext->codec_id = pOutputFormat->video_codec;
	pCodecContext->width = 480;
	pCodecContext->height = 480;
	pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecContext->time_base.den = 15;
	pCodecContext->time_base.num = 1;
	pCodecContext->gop_size = 15*3;
	pCodecContext->bit_rate = 500*1000;
	pCodecContext->profile = FF_PROFILE_H264_BASELINE;//baseline profile and main profileֻ֧��420����Ƶ����
	av_opt_set(pCodecContext->priv_data, "preset", "superfast");
	av_opt_set(pCodecContext->priv_data, "tune", "zerolatency");
	if(pFormatContext->flags & AVFMT_GLOBALHEADER)
		pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return 0;
}
