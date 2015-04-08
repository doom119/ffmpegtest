#include "com_doom119_ffmpegtest_MainActivity.h"
#include <android/log.h>
#include <jni.h>
#include "libavformat/avformat.h"

#define LOG_TAG "doom119"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

typedef struct
{
	// 分块的编号（按下录制，再松开就是一个块）
	int pBlockIndex;
	// 第几帧（如果设备比较卡，同一个pBlockIndex中的pFrameIndex可能不是连续的）
	int pFrameIndex;
	// codec格式
	int pCodec;

	// 帧的宽度
	int vWidth;
	// 帧的高度
	int vHeight;
	// 摄像头方向，一般手机的摄像头都是横向的，故竖屏需要转90度
	int vOrientation;
	// 帧率，即每秒捕获的帧数
	int vFPS;
	// 色彩空间
	int vColorFormat;
	// 压缩后输出的码流，单位kb
	int vBitRate;

	// 采样率，每秒钟采样的次数
	int aSampleRate;
	// 通道（双通道，单通道)
	int aChannel;
	// 采样大小，即用多少bit表示一个采样点(PCM编码)<br>
	int aPCMFormat;
	// 压缩后输出的码流，单位kb
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
int frameIndex = 0;

JNIEXPORT void JNICALL Java_com_doom119_ffmpegtest_MainActivity_prepareFFmpeg
  (JNIEnv *env, jclass clazz, jstring path)
{
	const char* srcVideoPath = (*env)->GetStringUTFChars(env, path, 0);
	const char* dstVideoPath = "/mnt/sdcard/ffmpeg_build_video.mp4";
	LOGD("srcVideoPath=%s, dstVideoPath=%s", srcVideoPath, dstVideoPath);

	if(initContext(dstVideoPath))
		return;

	if(beginWrite(dstVideoPath))
		return;

//	if(toWrite(srcVideoPath))
//		return;
	int ret = toWrite(srcVideoPath);
	LOGD("toWrite ret=%d", ret);

	endWrite();
}

int endWrite()
{
	av_write_trailer(pFormatContext);

	if(frameDataBuffer != NULL)
		free(frameDataBuffer);

	avcodec_close(pCodecContext);
	av_frame_free(&pFrame);
	avio_close(pFormatContext->pb);
	avformat_free_context(pFormatContext);

	return 0;
}

int toWrite(const char* srcVideoPath)
{
	unsigned char* srcData;
	int ret = -999;
	AVInfo frameHead;
	AVPacket packet;
	int got_output = 0;

	FILE* videoFile = fopen(srcVideoPath, "rb");
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
			LOGD("readFrame error, ret=%d, pts=%d", ret, pts);
			return -1;
		}

		pFrame->width = frameHead.vWidth;
		pFrame->height = frameHead.vHeight;
		pFrame->data[0] = srcData;
		pFrame->data[1] = srcData + frameHead.vWidth * frameHead.vHeight;
		pFrame->data[2] = srcData + frameHead.vWidth * frameHead.vHeight * 5 / 4;
		pFrame->linesize[0] = frameHead.vWidth;
		pFrame->linesize[1] = pFrame->linesize[2] = frameHead.vWidth / 2;
		pFrame->pts = pts;

		int frameDiff = 1;
		if(frameIndex == 0)
		{
			frameDiff = 1;
		}
		else
		{
			frameDiff = frameHead.pFrameIndex - frameIndex;
		}
		LOGD("frameDiff=%d", frameDiff);
		if(frameDiff < 1)
			return -3;

		int i;
		for(i = 0; i < frameDiff; ++i)
		{

		av_init_packet(&packet);
		packet.data = NULL;
		packet.priv = NULL;
		packet.size = 0;

		ret = avcodec_encode_video2(pStream->codec, &packet, pFrame, &got_output);
		if(ret < 0)
		{
			LOGD("avcodec_encode_video2 error, ret=%d, %s", ret, av_err2str(ret));
			av_free_packet(&packet);
			return -2;
		}

		if(got_output)
		{
			packet.stream_index = pStream->index;
			ret = av_interleaved_write_frame(pFormatContext, &packet);
			if(ret < 0)
			{
				LOGD("av_interleaved_write_frame error, %s", av_err2str(ret));
			}
		}
		av_free_packet(&packet);

		pts += av_rescale_q(1, pStream->codec->time_base, pStream->time_base);
		pFrame->pts = pts;
		}

		frameIndex = frameHead.pFrameIndex;
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
	if(ret <= 0)
	{
		return -1;
	}
	LOGD("read frame head count=%d, size=%d, frameIndex=%d", ret, sizeof(AVInfo), frameHead->pFrameIndex);

	int srcWidth = frameHead->vWidth;
	int srcHeight = frameHead->vHeight;
	int size = srcWidth * srcHeight * 3 / 2;

	unsigned char* data = getFrameBuffer(size);
	if(!data)
	{
		return -2;
	}

	ret = fread(data, size, 1, videoFile);
	//LOGD("read frame data count=%d, size=%d", ret, size);
	if(ret <= 0)
	{
		return -3;
	}

	*pdata = data;

	return 0;
}

int beginWrite(const char* dstVideoPath)
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
		ret = avio_open(&pFormatContext->pb, dstVideoPath, AVIO_FLAG_WRITE);
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

int initContext(const char* dstVideoPath)
{
	int ret = -999;

	av_register_all();

	ret = avformat_alloc_output_context2(&pFormatContext, NULL, NULL, dstVideoPath);
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
	pStream->id = pFormatContext->nb_streams-1;

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
	pCodecContext->profile = FF_PROFILE_H264_BASELINE;//baseline profile and main profile只支持420的视频序列
	av_opt_set(pCodecContext->priv_data, "preset", "veryfast", 0);
	av_opt_set(pCodecContext->priv_data, "tune", "zerolatency", 0);
	if(pFormatContext->flags & AVFMT_GLOBALHEADER)
		pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return 0;
}
