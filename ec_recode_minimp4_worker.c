//
// Created by arnold on 3/23/2018.
//

#define MINIMP4_IMPLEMENTATION
#include <unistd.h>
#include <sys/select.h>
#include "ec_recode_minimp4_worker.h"
#include "../dev_stat/ec_dev_state_define.h"
#include "../dev_stat/ec_dev_stat.h"
#include "../sdk/ec_hisi_sdk_recorder.h"
#include "../conf/ec_conf.h"
#include "../rtc/ec_rtc.h"
#include "../event/ec_event.h"
#include "../utils/ec_utils.h"
#include "../module_manager/ec_module.h"
#include "ec_record_minimp4.h"

static EC_INT writeVideoFrame(ec_mp4_info *mp4_info, ec_h264_frame *newFrame);

static EC_INT writeAacFrame(ec_mp4_info *mp4_info, ec_aac_frame *newFrame);




static EC_INT is_need_run = EC_TRUE;
static EC_BOOL is_run = EC_FALSE;


static EC_VOID record_work(EC_VOID *arg);


EC_VOID ec_record_work_run(EC_VOID)
{
	if (ec_stat_set(&is_run) == EC_FAILURE)
	{
		return;
	}
	is_need_run = EC_TRUE;
	uv_thread_t tid = 0;
	uv_thread_create(&tid, record_work, NULL);
}

EC_VOID ec_record_work_stop(EC_VOID)
{
	is_need_run = EC_FALSE;
	while (is_run)
	{
		cmh_wait_usec(1000);
	}

	return;
}

EC_BOOL ec_record_work_is_run(EC_VOID)
{
	return is_run;
}


static EC_VOID record_work(EC_VOID *arg)
{
	RECODER_CONF_PTR;
	EC_INT videoFd, audioFd;
	ec_mp4_info mp4_info = { 0, };
	//添加一个U盘引用
	dzlog_debug("recorde worker up");
	ec_disk_ref_add();
	fd_set readfds;

	CHECK_FAILED(ec_hisi_sdk_recorde_start(&videoFd, &audioFd), 0);
	CHECK_FAILED(open_mp4_handle(&mp4_info), 1);

	//create a timer for splice
	EC_INT splice_timer_fd = ec_timer_create(recoderConf->interval, 0);
	if (splice_timer_fd == EC_FAILURE)
	{
		goto failed_2;
	}


	EC_INT maxfd = videoFd > audioFd ? videoFd : audioFd;
	maxfd = maxfd > splice_timer_fd ? maxfd : splice_timer_fd;

	maxfd++;
	EC_INT ret;

	EC_INT video_frame_cnt = 0;
	EC_INT audio_frame_cnt = 0;

	BROAD_CAST(EC_EVENT_RECODER_START);

	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(videoFd, &readfds);
		FD_SET(audioFd, &readfds);
		FD_SET(splice_timer_fd, &readfds);

		ret = select(maxfd, &readfds, NULL, NULL, NULL);
		if (ret == -1)
		{
			continue;
		}
		if (!is_need_run)
		{
			//写入最后一帧
			writeVideoFrame(&mp4_info, NULL);
			writeAacFrame(&mp4_info, NULL);
			BROAD_CAST(EC_EVENT_WAIT_POWER_OFF);
			//关闭文件
			close_mp4_handle(&mp4_info);
			break;
		}
		if (FD_ISSET(videoFd, &readfds))
		{
			ec_h264_frame *frame;
			frame = ec_hisi_sdk_recorde_h264_get();
			if (!frame)
			{
				continue;
			}
			if (writeVideoFrame(&mp4_info, frame) == EC_FAILURE)
			{
				is_need_run = 0;
			}
			video_frame_cnt++;
		}

		if (FD_ISSET(audioFd, &readfds))
		{
			ec_aac_frame *frame;

			frame = ec_hisi_sdk_recorde_aac_get();
			if (!frame)
			{
				continue;
			}
			if (writeAacFrame(&mp4_info, frame) == EC_FAILURE)
			{
				is_need_run = 0;
			}
			audio_frame_cnt++;
		}

		if (FD_ISSET(splice_timer_fd, &readfds))
		{
			// ec_hisi_sdk_recorde_pause();
			 //写入最后一帧
			writeVideoFrame(&mp4_info, NULL);
			writeAacFrame(&mp4_info, NULL);
			//关闭文件
			close_mp4_handle(&mp4_info);

			//打开新的文件
			if (open_mp4_handle(&mp4_info) == EC_FAILURE)
			{
				dzlog_error("open mp4 handle failed");
				break;
			}
			// ec_hisi_sdk_recorde_resume();

			 //update timer
			ec_timer_update(splice_timer_fd, recoderConf->interval, 0);
		}


	}

	ec_hisi_sdk_recorde_stop(videoFd, audioFd);
	ec_timer_close(splice_timer_fd);



	BROAD_CAST(EC_EVENT_RECODER_STOP);


	while (mp4_have_bg())
	{
		dzlog_error("recorde  module have back ground thread run, waiting");
		cmh_wait_sec(1);
	}

	ec_stat_unset(&is_run);
	dzlog_debug("recorde worker down");
	ec_disk_ref_del();


	return;

failed_3:
	ec_timer_close(splice_timer_fd);
failed_2:
	close_mp4_handle(&mp4_info);
failed_1:
	ec_hisi_sdk_recorde_stop(videoFd, audioFd);
failed_0:
	dzlog_error("record worker error");
	ec_disk_ref_del();
	ec_stat_unset(&is_run);
	BROAD_CAST(EC_EVENT_RECODER_STOP);
	return;

}


static ec_h264_frame *lastVideoFrame = EC_NULL;
static ec_aac_frame *lastAudioFrame = EC_NULL;

#if 0
static EC_INT writeVideoFrame(ec_mp4_info *mp4_info, ec_h264_frame *newFrame)
{
	if (lastVideoFrame == NULL)
	{
		lastVideoFrame = newFrame;
		return EC_SUCCESS;
	}
	else
	{
		FREE_H264(lastVideoFrame);
		lastVideoFrame = newFrame;
	}

	return EC_SUCCESS;
}
static EC_INT writeAacFrame(ec_mp4_info *mp4_info, ec_aac_frame *newFrame)
{
	if (lastAudioFrame == EC_NULL)
	{

		lastAudioFrame = newFrame;
		return EC_SUCCESS;
	}
	FREE_AAC(lastAudioFrame);
	lastAudioFrame = newFrame;

}
#else
static EC_INT writeVideoFrame(ec_mp4_info *mp4_info, ec_h264_frame *newFrame)
{
	EC_UCHAR		*tmpPos = mp4_info->videoBuffer;
	EC_UINT			tmpPts = 0;
	EC_UINT			tmpFrameLength = 0;


	if (lastVideoFrame == NULL) //cache first frame
	{
		lastVideoFrame = newFrame;

		return EC_SUCCESS;
	}

	if (NULL == newFrame) 
	{
		return EC_SUCCESS;
	}
	else
	{
		duration = (newFrame->pts - lastVideoFrame->pts) * 90;
	}

	if (newFrame->spsLen > 0) 
	{
// 		memcpy(tmpPos, newFrame->sps, newFrame->spsLen);
// 		tmpPos += newFrame->spsLen;
		mp4_h26x_write_nal(&(mp4_info->mp4Writer)
			, newFrame->sps
			, newFrame->spsLen
			, 0
			, newFrame->pts
			, mp4_info->videoTimeStamp90kHzNext
		);
	}

	if (newFrame->ppsLen > 0) 
	{
		memcpy(tmpPos, newFrame->pps, newFrame->ppsLen);
		tmpPos += newFrame->ppsLen;
		//mp4_h26x_write_nal(&(mp4_info->mp4Writer)
		//	, newFrame->pps
		//	, newFrame->ppsLen
			//, 0
			//, newFrame->pts
		//	, mp4_info->videoTimeStamp90kHzNext
		//);
	}

	if (newFrame->seiLen > 0) 
	{
		memcpy(tmpPos, newFrame->sei, newFrame->seiLen);
		tmpPos += newFrame->seiLen;

		//mp4_h26x_write_nal(&(mp4_info->mp4Writer)
		//	, newFrame->sei
		//	, newFrame->seiLen
			//, 0
			//, newFrame->pts
		//	, mp4_info->videoTimeStamp90kHzNext
		//);
	}

	//dzlog_debug("write video last %d  ", lastVideoFrame->pts);
	if (newFrame->frameLen > 0) 
	{
		memcpy(tmpPos, newFrame->frame, newFrame->frameLen);
		tmpPos += newFrame->frameLen;

		//mp4_h26x_write_nal(&(mp4_info->mp4Writer)
		//	, newFrame->frame
		//	, newFrame->frameLen
			//, tmpPts
			//, newFrame->pts
		//	, mp4_info->videoTimeStamp90kHzNext
		//);
	}

	mp4_h26x_write_nal(&(mp4_info->mp4Writer)
		, mp4_info->videoBuffer
		, tmpFrameLength
		//, tmpPts
		//, newFrame->pts
		, mp4_info->videoTimeStamp90kHzNext
	);

	//写入图像帧后，修改计数
	mp4_info->frameCount++;

	printf("video pts : %d \r\n", tmpPts);

	FREE_H264(newFrame);
	return EC_SUCCESS;

failed_0:
	dzlog_error("write video frame failed");
	//free now
	FREE_H264(newFrame);
	return EC_FAILURE;
}


static EC_UINT audioTickCorrect = 0; //use for correct audio offset
static EC_INT writeAacFrame(ec_mp4_info *mp4_info, ec_aac_frame *newFrame)
{
	EC_UINT			tmpPts = 0;


	if (NULL == newFrame)
	{
		return EC_SUCCESS;
	}
	else
	{
		if (mp4_info->audioPts > 0)
		{
			tmpPts = (newFrame->pts - mp4_info->audioPts) * 1000;
		}

		mp4_info->audioPts = newFrame->pts;
	}
	
	if (newFrame->dataLen > 0)
	{
		MP4E__put_sample(mp4_info->mp4handle
			, mp4_info->audioTrackId
			, newFrame->data + 7
			, newFrame->dataLen - 7
			//, tmpPts
			//, newFrame->pts
			, mp4_info->audioStampDuration
			, MP4E_SAMPLE_RANDOM_ACCESS);

		printf("audio pts : %d \r\n", tmpPts);
	}

	FREE_AAC(newFrame);

	return EC_SUCCESS;

failed_0:
	dzlog_error("wrte audio frame failed");
	FREE_AAC(newFrame);
	return EC_FAILURE;
}
#endif


