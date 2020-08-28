//
// Created by arnold on 10/29/2018.
//

#ifndef EC_MAIN_APP_EC_RECORD_MP4_H
#define EC_MAIN_APP_EC_RECORD_MP4_H

#include "../common/ec_define.h"

//用于保存h264流数据，用于辅助媒体测试
//#define MP4MUXER_VIDEO_SAVE_FILE
//用于保存音频流数据，用于辅助媒体测试
//#define MP4MUXER_AUDIO_SAVE_FILE
//表示媒体文件的最小时长（单位：秒）。用于校验文件的合法性。 如果文件小于最小时长，则删除。
#define MP4MUXER_MIN_VIDEO_DURATION		1

typedef struct
{
	void		*mp4handle;		//表示mp4文件处理句柄
	EC_INT		isInit;			//表示mp4handle是否被初始化，为SPSPPS等信息的设置；EC_FALSE表示未初始化；EC_TRUE表示已经初始化；
	EC_UINT		frameCount;		//表示当前已经写入的帧数量计数

#ifdef MP4MUXER_VIDEO_SAVE_FILE
	FILE		*fvideo;			//测试用保存视频文件
#endif
#ifdef MP4MUXER_AUDIO_SAVE_FILE
	FILE		*faudio;			//测试用保存音频文件
#endif

    EC_CHAR    currentFileName[EC_FILE_NAME_MAX_LEN];
} ec_mp4_info;


EC_INT open_mp4_handle(ec_mp4_info *mp4_info);

EC_INT close_mp4_handle(ec_mp4_info *mp4_info);

EC_VOID close_mp4_handle_bg  (void* handle);

EC_BOOL mp4_have_bg (EC_VOID);

#endif //EC_MAIN_APP_EC_RECORD_MP4_H
