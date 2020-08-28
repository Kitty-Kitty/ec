//
// Created by arnold on 10/29/2018.
//
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "ec_record_mp4muxer.h"
#include "mp4muxer.h"
#include "../conf/ec_conf.h"
#include "../rtc/ec_rtc.h"
#include "../utils/ec_utils.h"
#include "../log/ec_log.h"


static EC_INT getAVCProfileIndication(EC_VOID)
{
    VIDEO_CONF_PTR;

    switch (videoConf->h264ProfileType)
    {
        case H264_BASE_PROFILE:
            return 66;
        case H264_MAIN_PROFILE:
            return 77;
        case H264_HIGH_PROFILE:
            return 100;
        default:
            return 77;
            break;
    }
}

static EC_INT get_profile_compat(EC_VOID)
{
    return 0x00;
}

static EC_INT getAVCLevlIndication(EC_VOID)
{
    return 0x28;
}

EC_INT delete_invalid_file(ec_mp4_info *mp4_info)
{
	VIDEO_CONF_PTR;
	EC_INT		tmp_valid_frames = MP4MUXER_MIN_VIDEO_DURATION * videoConf->framerate;


	if (mp4_info->frameCount >= tmp_valid_frames) {
		dzlog_info("valid mp4 file[%s : %d frame]."
			, mp4_info->currentFileName
			, mp4_info->frameCount);
		return 0;
	}
	else {
		//表示删除文件
		unlink(mp4_info->currentFileName);
		dzlog_info("delete invalid mp4 file[%s : %d frame] less than [%d frame]."
			, mp4_info->currentFileName
			, mp4_info->frameCount
			, tmp_valid_frames);
		return -1;
	}
}


EC_INT open_mp4_handle(ec_mp4_info *mp4_info)
{
    EC_CHAR		tsBuf[128] = {'\0'};
	EC_UINT		duration = 300000;		//表示该文件保存的视频长度，单位为毫秒。默认:300000毫秒 = 5分钟

    DEV_CONF_PTR;
    VIDEO_CONF_PTR;
	RECODER_CONF_PTR;


    //getStrTime(tsBuf, 127);
    ec_rtc_get_ts(tsBuf, 127);
    EC_MEMSET_2(mp4_info->currentFileName, EC_FILE_NAME_MAX_LEN);
    snprintf(mp4_info->currentFileName, EC_FILE_NAME_MAX_LEN - 1,
             "%s/%s-%s-%s.mp4", EC_DEFAULT_RECODE_PATH, devConf->devid, devConf->userid, tsBuf);
    dzlog_info("new file : %s", mp4_info->currentFileName);

#ifdef MP4MUXER_VIDEO_SAVE_FILE
	char    tmp_file_name[1024] = { 0, };
	snprintf(tmp_file_name, sizeof(tmp_file_name), "%s.h264", mp4_info->currentFileName);
	mp4_info->fvideo = fopen(tmp_file_name, "wb");
	if (!mp4_info->fvideo) {
		printf("error: can't open output file[%s]\r\n"
			, tmp_file_name);
		return EC_FAILURE;
	}
	else {
		printf("open output file[%s]\r\n"
			, tmp_file_name);
	}
#endif

#ifdef MP4MUXER_AUDIO_SAVE_FILE
	char    tmp_audio_file_name[1024] = { 0, };
	snprintf(tmp_audio_file_name, sizeof(tmp_audio_file_name), "%s.aac", mp4_info->currentFileName);
	mp4_info->faudio = fopen(tmp_audio_file_name, "wb");
	if (!mp4_info->faudio) {
		printf("error: can't open output file[%s]\r\n"
			, tmp_audio_file_name);
		return EC_FAILURE;
	}
	else {
		printf("open output file[%s]\r\n"
			, tmp_audio_file_name);
	}
#endif

	if (recoderConf->interval > 0) {
		duration = recoderConf->interval * 1000;
	}

	mp4_info->mp4handle = mp4muxer_init(mp4_info->currentFileName
			, duration
			, 1920
			, 1080
			, videoConf->framerate
			, videoConf->gop
			, 1
			, 44100
			, 16
			, 1024
			, NULL);

    //set video part
    if (mp4_info->mp4handle == NULL)
    {
        dzlog_error("mp4 Createex failed");
        return EC_FAILURE;
    }
	//清空计数，方便文件合法性的校验
	mp4_info->frameCount = 0;
	//设置为EC_FALSE表示mp4文件还需要设置SPS、PPS等信息，进行初始化处理
	mp4_info->isInit = EC_FALSE;

	//for 22050 is   00010 0111 0001 000
	//for 44100 is   00010 0100  0001 000  //https://blog.csdn.net/dxpqxb/article/details/42266873
	// uint8_t config[2] = {0x13, 0x88}; for 22050
	//uint8_t config[] = { 0x12, 0x08 }; //for 44100
	uint8_t config[] = { 0x12, 0x10 }; //for 44100
	mp4muxer_aacdecspecinfo(mp4_info->mp4handle, config);

    return EC_SUCCESS;

    failed_0:
    mp4_info->mp4handle = NULL;
    unlink(mp4_info->currentFileName);
    return EC_FAILURE;
}


EC_INT close_mp4_handle(ec_mp4_info *mp4_info)
{
    if (mp4_info->mp4handle)
    {
        //flush cache first
		mp4muxer_exit(mp4_info->mp4handle);
        mp4_info->mp4handle = EC_NULL;
    }

	//删除那些太小的无法正常显示的视频
	delete_invalid_file(mp4_info);

#ifdef MP4MUXER_VIDEO_SAVE_FILE
	if (mp4_info->fvideo) {
		fclose(mp4_info->fvideo);
		mp4_info->fvideo = EC_NULL;
	}
#endif

#ifdef MP4MUXER_AUDIO_SAVE_FILE
	if (mp4_info->faudio) {
		fclose(mp4_info->faudio);
		mp4_info->faudio = EC_NULL;
	}
#endif
	//清空计数，方便文件合法性的校验
	mp4_info->frameCount = 0;
	//设置为EC_FALSE表示mp4文件还需要设置SPS、PPS等信息，进行初始化处理
	mp4_info->isInit = EC_FALSE;

    // dzlog_debug("mv file %s to %s", TMP_FILE_NAME, recode_stat.currentFileName);
    //  rename(TMP_FILE_NAME, recode_stat.currentFileName);
    //resetVideoDuration();
    //  resetAudioDuration();

    dzlog_debug("close mp4 file success");

    return EC_SUCCESS;

}

static EC_INT current_thread_cnt = 0;


static EC_VOID *close_thread(EC_VOID *arg)
{
    ec_atomic_add(&current_thread_cnt);
	//mp4muxer_exit(arg);	
    ec_atomic_sub(&current_thread_cnt);
}

EC_VOID close_mp4_handle_bg(void* handle)
{
    pthread_t pid;
    pthread_create(&pid, NULL, close_thread, handle);
}

EC_BOOL mp4_have_bg (EC_VOID)
{
    return current_thread_cnt;
}