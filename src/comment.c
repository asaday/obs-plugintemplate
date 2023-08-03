
#include <math.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>


#include <windows.h>

struct comment_data {
	bool initialized_thread;
	pthread_t thread;
	os_event_t *event;
	obs_source_t *source;
};


static void *comment_thread(void *pdata)
{

	struct comment_data *swd = pdata;
	uint64_t last_time = os_gettime_ns();
	uint64_t ts = 0;


	int rate = 16000; // 44100
	struct obs_source_audio data;
	//data.data[0] = bytes;
	data.frames = 4800;
	data.speakers = SPEAKERS_MONO;
	data.samples_per_sec = rate;      //44100;
	data.timestamp = ts;
	data.format = AUDIO_FORMAT_16BIT;

	HANDLE hPipe = NULL;
	DWORD len = rate / 2; //44100;
	char *buffer = bzalloc(len);
	DWORD dwRead;

	while (os_event_try(swd->event) == EAGAIN) {
		os_sleep_ms(100);

		if (!hPipe) {
			hPipe = CreateNamedPipe(
				L"\\\\.\\pipe\\MyNamedPipe", PIPE_ACCESS_DUPLEX,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
					PIPE_NOWAIT,
				PIPE_UNLIMITED_INSTANCES, 1024, len * 8, 0,
				NULL);

			if (hPipe == INVALID_HANDLE_VALUE) {
				continue;
			}
		}

		ConnectNamedPipe(hPipe, NULL);
		DWORD ret = GetLastError();
		if (ret == ERROR_PIPE_LISTENING)
			continue;
		if (GetLastError() != ERROR_PIPE_CONNECTED) {
			CloseHandle(hPipe);
			hPipe = NULL;
			continue;
		}

		uint64_t last_time = os_gettime_ns();
		ts = 0;
		while (os_event_try(swd->event) == EAGAIN) {
			dwRead = 0;
			ReadFile(hPipe, buffer, len, &dwRead, NULL);
			DWORD ret = GetLastError();
			if (ret == ERROR_NO_DATA || ret == ERROR_IO_PENDING)
				continue;

			if (!dwRead) {
				CloseHandle(hPipe);
				hPipe = NULL;
				break;
			}

			data.data[0] = buffer;
			uint32_t frames = dwRead / 2; /// 2;
			data.frames = frames;
			data.timestamp = ts;
			obs_source_output_audio(swd->source, &data);
			uint64_t duration = 1000000000 / (rate / frames);
			ts += duration;
			os_sleepto_ns(last_time + ts);
		}
	}

	if (hPipe)
		CloseHandle(hPipe);

	bfree(buffer);

	return NULL;
}

/* ------------------------------------------------------------------------- */

static const char *comment_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Comment voice Source (Test)";
}

static void comment_destroy(void *data)
{
	struct comment_data *swd = data;

	if (swd) {
		if (swd->initialized_thread) {
			void *ret;
			os_event_signal(swd->event);
			pthread_join(swd->thread, &ret);
		}

		os_event_destroy(swd->event);
		bfree(swd);
	}
}

static void *comment_create(obs_data_t *settings, obs_source_t *source)
{
	struct comment_data *swd = bzalloc(sizeof(struct comment_data));
	swd->source = source;

	if (os_event_init(&swd->event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&swd->thread, NULL, comment_thread, swd) != 0)
		goto fail;

	swd->initialized_thread = true;

	UNUSED_PARAMETER(settings);
	return swd;

fail:
	comment_destroy(swd);
	return NULL;
}

struct obs_source_info test_comment = {
	.id = "test_comment",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = comment_getname,
	.create = comment_create,
	.destroy = comment_destroy,
};
