//
// Created by arnold on 10/29/2018.
//

#ifndef EC_MAIN_APP_EC_RECORD_MP4_H
#define EC_MAIN_APP_EC_RECORD_MP4_H

#include "../common/ec_define.h"

#include "minimp4.h"

//��ʾý���ļ�����Сʱ������λ���룩������У���ļ��ĺϷ��ԡ� ����ļ�С����Сʱ������ɾ����
#define MP4MUXER_MIN_VIDEO_DURATION		1

typedef struct
{
	MP4E_mux_t			*mp4handle;					//��ʾmp4�ļ�������
	mp4_h26x_writer_t	mp4Writer;					//��ʾmp4�ļ��м�¼��Ƶд����Ϣ
	EC_INT				videoTimeStamp90kHzNext;	//��ʾ��Ƶ��һ֡���
	EC_INT				audioStampDuration;			//��ʾ��Ƶ����ʱ��
	FILE				*fileOutput;				//mp4�ļ��������
	EC_UCHAR			*videoBuffer;				//���frame��buffer
	int					audioTrackId;				//mp4����Ƶtrack���
	EC_UINT				audioPts;					//��Ƶʱ��
	EC_UINT				videoPts;					//��Ƶʱ��
	EC_UINT				frameCount;					//��ʾ��ǰ�Ѿ�д���֡��������
    EC_CHAR				currentFileName[EC_FILE_NAME_MAX_LEN];
} ec_mp4_info;


EC_INT open_mp4_handle(ec_mp4_info *mp4_info);

EC_INT close_mp4_handle(ec_mp4_info *mp4_info);

EC_VOID close_mp4_handle_bg  (void* handle);

EC_BOOL mp4_have_bg (EC_VOID);

#endif //EC_MAIN_APP_EC_RECORD_MP4_H
