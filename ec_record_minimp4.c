//
// Created by arnold on 10/29/2018.
//
#include <unistd.h>
#include <pthread.h>
#include "ec_record_minimp4.h"
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

static void write_callback(int64_t offset, const void *buffer, size_t size, void *token)
{
	FILE *f = (FILE*)token;
	fseek(f, offset, SEEK_SET);
	fwrite(buffer, size, 1, f);
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

    DEV_CONF_PTR;
    VIDEO_CONF_PTR;
	RECODER_CONF_PTR;


    //getStrTime(tsBuf, 127);
    ec_rtc_get_ts(tsBuf, 127);
    EC_MEMSET_2(mp4_info->currentFileName, EC_FILE_NAME_MAX_LEN);
    snprintf(mp4_info->currentFileName, EC_FILE_NAME_MAX_LEN - 1,
             "%s/%s-%s-%s.mp4", EC_DEFAULT_RECODE_PATH, devConf->devid, devConf->userid, tsBuf);
    
	//打开文件，用于保存mp4数据
	mp4_info->fileOutput = fopen(mp4_info->currentFileName, "wb");
	if (!mp4_info->fileOutput) {
		printf("error: can't open output file[%s]\r\n"
			, mp4_info->currentFileName);
		return EC_FAILURE;
	}
	else
	{
		dzlog_info("new file : %s", mp4_info->currentFileName);
	}

	mp4_info->mp4handle = MP4E__open(0, mp4_info->fileOutput, write_callback);
	//set video part
	if (mp4_info->mp4handle == NULL)
	{
		dzlog_error("mp4 Createex failed");
		close_mp4_handle(mp4_info);
		return EC_FAILURE;
	}
	else
	{
		dzlog_info("mp4 Createex succeed！");
	}

	//初始化视频信息
	mp4_h26x_write_init(&(mp4_info->mp4Writer), mp4_info->mp4handle, 1920, 1080, 0);

	//初始化音频信息
	//for 22050 is   00010 0111 0001 000
	//for 44100 is   00010 0100  0001 000  //https://blog.csdn.net/dxpqxb/article/details/42266873
	// uint8_t config[2] = {0x13, 0x88}; for 22050
	uint8_t config[] = { 0x12, 0x08 }; //for 44100

	MP4E_track_t tr;
	tr.track_media_kind = e_audio;
	tr.language[0] = 'u';
	tr.language[1] = 'n';
	tr.language[2] = 'd';
	tr.language[3] = 0;
	//tr.object_type_indication = MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3;
	tr.object_type_indication = MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_LC_PROFILE;
	tr.time_scale = 90000;
	tr.default_duration = 0;
	tr.u.a.channelcount = 1;
	mp4_info->audioTrackId = MP4E__add_track(mp4_info->mp4handle, &tr);
	MP4E__set_dsi(mp4_info->mp4handle, mp4_info->audioTrackId, config, sizeof(config));

	//设置音视频pts信息
	mp4_info->videoTimeStamp90kHzNext = 90000 / videoConf->framerate;
	mp4_info->audioStampDuration = 1024 * 90000 / 44100;
	mp4_info->audioPts = 0;
	mp4_info->videoPts = 0;

	//清空计数，方便文件合法性的校验
	mp4_info->frameCount = 0;

	if (!mp4_info->videoBuffer) {
		mp4_info->videoBuffer = malloc(1 * 1024 * 1024);
	}

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
		mp4_h26x_write_free(&(mp4_info->mp4Writer));
        //flush cache first
		MP4E__close(mp4_info->mp4handle);
        mp4_info->mp4handle = EC_NULL;
    }

	if (mp4_info->fileOutput)
	{
		fclose(mp4_info->fileOutput);
		mp4_info->fileOutput = EC_NULL;
	}

	if (mp4_info->videoBuffer)
	{
		free(mp4_info->videoBuffer);
		mp4_info->videoBuffer = EC_NULL;
	}

	//删除那些太小的无法正常显示的视频
	delete_invalid_file(mp4_info);

	//清空计数，方便文件合法性的校验
	mp4_info->frameCount = 0;

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