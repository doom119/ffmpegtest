/*#include "com_doom119_ffmpegtest_MainActivity.h"
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

JNIEXPORT void JNICALL Java_com_doom119_ffmpegtest_MainActivity_prepareFFmpeg
  (JNIEnv *env, jclass clazz, jstring path)
{
	const char* videoPath = (*env)->GetStringUTFChars(env, path, 0);
	const char* desVideoPath = "/mnt/sdcard/ffmpeg_build_video.mp4";
	LOGD("videoPath=%s, desVideoPath=%s", videoPath, desVideoPath);

	FILE* videoFile = fopen(videoPath, "rb");
	fseek(videoFile, 0, SEEK_END);
	long size = ftell(videoFile);
	LOGD("videoSize=%d", size);
	rewind(videoFile);

	AVFormatContext *pFormatContext;
	AVOutputFormat *pOutputFormat;
	AVCodecContext *pCodecContext;

	int ret = -9999;
	av_register_all();
	//����FormatContext
	ret = avformat_alloc_output_context2(&pFormatContext, NULL, NULL, desVideoPath);
	LOGD("avformat_alloc_output_context2, pFormatContext=%d, nb_streams=%d, ret=%d",
			pFormatContext, ret, pFormatContext->nb_streams);

	pOutputFormat = pFormatContext->oformat;
	LOGD("pOutputFormat=%d", pOutputFormat);
	LOGD("codec id=%d", pOutputFormat->video_codec);
	//���ұ�����
	AVCodec* pCodec = avcodec_find_encoder(pOutputFormat->video_codec);
	LOGD("pCodec=%d, type=%d", pCodec, pCodec->type);

	//������
	AVStream* pStream = avformat_new_stream(pFormatContext, pCodec);
	LOGD("pStream=%d", pStream);
	pCodecContext = pStream->codec;
	LOGD("pCodecContext=%d", pCodecContext);

	//��CodecContext��ֵ����
	pCodecContext->codec_id = pOutputFormat->video_codec;
	pCodecContext->coded_width = 480;
	pCodecContext->coded_height = 480;
	pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecContext->time_base.den = 15;
	pCodecContext->time_base.num = 1;
	pCodecContext->gop_size = 15 * 3;
	pCodecContext->bit_rate = 500*1000;
	pCodecContext->profile = AV_CODEC_ID_H264;
	av_opt_set(pCodecContext->priv_data, "preset", "veryfast", 0);
	av_opt_set(pCodecContext->priv_data, "tune", "zerolatency", 0);
	int globalHeader = pOutputFormat->flags & AVFMT_GLOBALHEADER;
	if(globalHeader)
		pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

	//�򿪱�����
	ret = avcodec_open2(pCodecContext, pCodec, NULL);
	LOGD("avcodec_open2, ret=%d, %s", ret, av_err2str(ret));

	av_dump_format(pFormatContext, 0, desVideoPath, 1);

	if(!(pFormatContext->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&pFormatContext->pb, desVideoPath, AVIO_FLAG_WRITE);
		LOGD("avio_open, ret=%d, %s", ret, av_err2str(ret));
	}

	//дͷ
	ret = avformat_write_header(pFormatContext, NULL);
	LOGD("avformat_wirte_header, ret=%d, %s", ret, av_err2str(ret));

//	double videoTime = pStream->pts.val * pStream->time_base.num / pStream->time_base.den;
//	LOGD("videoTime=%lf", videoTime);

	int videoStreamPts = 0;

	//����Frame
	AVFrame* pFrame = av_frame_alloc(); //avcodec_alloc_frame();
	LOGD("pFrame=%d", pFrame);
	do
	{
		LOGD("****************begin frame****************");

		//��vf�ж�һ֡��Ƶ����
		AVInfo avInfo;
		int avSize = sizeof(AVInfo);
		ret = fread(&avInfo, 1, avSize, videoFile);
		LOGD("fread avinfo data, ret=%d", ret);
		if (!ret) break;
		LOGD("avSize=%d, AVInfo.pFrameIndex=%d, AVInfo.vWidth=%d, AVInfo.vHeight=%d",
				avSize, avInfo.pFrameIndex, avInfo.vWidth, avInfo.vHeight);
		int frameSize = avInfo.vWidth * avInfo.vHeight * 3 / 2;
		unsigned char* srcData = (unsigned char*) malloc(avInfo.vWidth * avInfo.vHeight * 3 / 2);
		ret = fread(srcData, frameSize, 1, videoFile);
		LOGD("fread frame data, ret=%d", ret);
		if (!ret) break;

		//��Frame��ֵ
		pFrame->width = avInfo.vWidth;
		pFrame->height = avInfo.vHeight;
		pFrame->data[0] = srcData;
		pFrame->data[1] = srcData + avInfo.vWidth * avInfo.vHeight;
		pFrame->data[2] = srcData + avInfo.vWidth * avInfo.vHeight * 5 / 4;
		pFrame->linesize[0] = avInfo.vWidth;
		pFrame->linesize[1] = pFrame->linesize[2] = avInfo.vWidth / 2;
		pFrame->pts = videoStreamPts++;

		//��ʼ��Pakcet
		AVPacket packet;
		av_init_packet(&packet);
		packet.data = NULL;
		packet.priv = NULL;
		packet.size = 0;

		int got_output;
		//��������
		ret = avcodec_encode_video2(pCodecContext, &packet, pFrame, &got_output);
		LOGD("avcodec_encode_video2, got_output=%d, ret=%d, %s", got_output, ret, av_err2str(ret));
		if (ret < 0)
		{
			av_free_packet(&packet);
			free(srcData);
			break;
		}
		if (got_output)
		{
			packet.stream_index = pStream->index;
			//�ѱ��������д�ļ�
			ret = av_interleaved_write_frame(pFormatContext, &packet);
			LOGD("av_interleaved_write_frame, ret=%d, %s", ret, av_err2str(ret));
		}
		av_free_packet(&packet);
		free(srcData);
		LOGD("****************end frame****************");
	}
	while (1);

	ret = av_write_trailer(pFormatContext);
	LOGD("av_write_trailer, ret=%d", ret);

	//�ͷ���Դ
	avcodec_close(pStream->codec);
	av_frame_free(&pFrame);//avcodec_free_frame(pFrame);
	avio_close(pFormatContext->pb);
	avformat_free_context(pFormatContext);
	fclose(videoFile);
}*/
