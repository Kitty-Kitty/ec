//
// Created by arnold on 10/29/2018.
//

#ifndef EC_MAIN_APP_EC_RECORD_MP4_H
#define EC_MAIN_APP_EC_RECORD_MP4_H

#include "../common/ec_define.h"

#include "minimp4.h"

//表示媒体文件的最小时长（单位：秒）。用于校验文件的合法性。 如果文件小于最小时长，则删除。
#define MP4MUXER_MIN_VIDEO_DURATION		1

typedef struct
{
	MP4E_mux_t			*mp4handle;					//表示mp4文件处理句柄
	mp4_h26x_writer_t	mp4Writer;					//表示mp4文件中记录视频写入信息
	EC_INT				videoTimeStamp90kHzNext;	//表示视频下一帧间隔
	EC_INT				audioStampDuration;			//表示音频采样时长
	FILE				*fileOutput;				//mp4文件操作句柄
	EC_UCHAR			*videoBuffer;				//组合frame的buffer
	int					audioTrackId;				//mp4中音频track编号
	EC_UINT				audioPts;					//音频时间
	EC_UINT				videoPts;					//音频时间
	EC_UINT				frameCount;					//表示当前已经写入的帧数量计数
    EC_CHAR				currentFileName[EC_FILE_NAME_MAX_LEN];
} ec_mp4_info;


EC_INT open_mp4_handle(ec_mp4_info *mp4_info);

EC_INT close_mp4_handle(ec_mp4_info *mp4_info);

EC_VOID close_mp4_handle_bg  (void* handle);

EC_BOOL mp4_have_bg (EC_VOID);

#endif //EC_MAIN_APP_EC_RECORD_MP4_H
