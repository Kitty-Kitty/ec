//
// Created by arnold on 3/23/2018.
//
#include <unistd.h>
#include <sys/select.h>
#include "mp4muxer.h"
#include "ec_recode_mp4muxer_worker.h"
#include "../dev_stat/ec_dev_state_define.h"
#include "../dev_stat/ec_dev_stat.h"
#include "../sdk/ec_hisi_sdk_recorder.h"
#include "../conf/ec_conf.h"
#include "../rtc/ec_rtc.h"
#include "../event/ec_event.h"
#include "../utils/ec_utils.h"
#include "../module_manager/ec_module.h"
#include "ec_record_mp4muxer.h"

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

EC_BOOL ec_record_work_is_run (EC_VOID)
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


    while(mp4_have_bg())
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
	if (NULL == newFrame) {
		return EC_SUCCESS;
	}

	if (EC_FALSE == mp4_info->isInit)
	{
		if (!newFrame->keyFlag)
		{
			FREE_H264(newFrame);
			return EC_SUCCESS;
		}
		mp4_info->isInit = EC_TRUE;
	}
	
	if (EC_TRUE == mp4_info->isInit)
	{
		if (newFrame->keyFlag && newFrame->spsLen > 0 && newFrame->ppsLen > 0)
		{
			mp4muxer_spspps(mp4_info->mp4handle
				, newFrame->sps + 4
				, newFrame->spsLen - 4
				, newFrame->pps + 4
				, newFrame->ppsLen - 4
			);
		}

		if (newFrame->frameLen > 0) {
			mp4muxer_video(mp4_info->mp4handle
				, newFrame->frame
				, newFrame->frameLen
				, newFrame->pts
			);

			//写入图像帧后，修改计数
			mp4_info->frameCount++;
		}
	}

#ifdef MP4MUXER_VIDEO_SAVE_FILE
	printf("h264 info[frame_lenght: %d \t pts: %d]\r\n"
		, newFrame->frameLen
		, newFrame->pts);

	fwrite(newFrame->frame, newFrame->frameLen, 1, mp4_info->fvideo);
#endif

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
	if (NULL == newFrame) {
		return EC_SUCCESS;
	}

	if (EC_TRUE == mp4_info->isInit && newFrame->dataLen > 0)
	{
  		mp4muxer_audio(mp4_info->mp4handle
  			, newFrame->data + 7
  			, newFrame->dataLen - 7
  			, newFrame->pts
  		);

#ifdef MP4MUXER_AUDIO_SAVE_FILE
		printf("aac info[frame_lenght: %d \t pts: %d]\r\n"
			, newFrame->dataLen
			, newFrame->pts);
		fwrite(newFrame->data, newFrame->dataLen, 1, mp4_info->faudio);
#endif

	}

    FREE_AAC(newFrame);
	return EC_SUCCESS;

    failed_0:
    dzlog_error("wrte audio frame failed");
    FREE_AAC(newFrame);
    return EC_FAILURE;
}
#endif


