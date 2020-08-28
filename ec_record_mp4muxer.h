//
// Created by arnold on 10/29/2018.
//

#ifndef EC_MAIN_APP_EC_RECORD_MP4_H
#define EC_MAIN_APP_EC_RECORD_MP4_H

#include "../common/ec_define.h"

//���ڱ���h264�����ݣ����ڸ���ý�����
//#define MP4MUXER_VIDEO_SAVE_FILE
//���ڱ�����Ƶ�����ݣ����ڸ���ý�����
//#define MP4MUXER_AUDIO_SAVE_FILE
//��ʾý���ļ�����Сʱ������λ���룩������У���ļ��ĺϷ��ԡ� ����ļ�С����Сʱ������ɾ����
#define MP4MUXER_MIN_VIDEO_DURATION		1

typedef struct
{
	void		*mp4handle;		//��ʾmp4�ļ�������
	EC_INT		isInit;			//��ʾmp4handle�Ƿ񱻳�ʼ����ΪSPSPPS����Ϣ�����ã�EC_FALSE��ʾδ��ʼ����EC_TRUE��ʾ�Ѿ���ʼ����
	EC_UINT		frameCount;		//��ʾ��ǰ�Ѿ�д���֡��������

#ifdef MP4MUXER_VIDEO_SAVE_FILE
	FILE		*fvideo;			//�����ñ�����Ƶ�ļ�
#endif
#ifdef MP4MUXER_AUDIO_SAVE_FILE
	FILE		*faudio;			//�����ñ�����Ƶ�ļ�
#endif

    EC_CHAR    currentFileName[EC_FILE_NAME_MAX_LEN];
} ec_mp4_info;


EC_INT open_mp4_handle(ec_mp4_info *mp4_info);

EC_INT close_mp4_handle(ec_mp4_info *mp4_info);

EC_VOID close_mp4_handle_bg  (void* handle);

EC_BOOL mp4_have_bg (EC_VOID);

#endif //EC_MAIN_APP_EC_RECORD_MP4_H
