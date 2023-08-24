
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

struct Header {
	uint16_t magic;
	uint16_t headerLength;
	uint32_t dataLength;
	uint16_t sampleRate;
	uint16_t bitRate;
	uint32_t nop;
};

void doPlay(struct comment_data *ctx, void *data)
{
	struct Header *header = data;
	if (header->magic != 0x2525 || header->bitRate != 16) {
		blog(LOG_INFO, "comment_audio wrong header.");
		return;
	}

	uint32_t bpf = 2; // bytes per frame .. fix = 2(16bit)
	uint32_t frames = header->dataLength / bpf;
	uint32_t rate = header->sampleRate;
	uint8_t *rawData = (uint8_t *)data + header->headerLength;

	struct obs_source_audio osa;
	osa.speakers = SPEAKERS_MONO;
	osa.format = AUDIO_FORMAT_16BIT;
	osa.samples_per_sec = rate;

	uint64_t ts = os_gettime_ns();
	uint64_t shift = 10 * 1000000; // shift 10ms
	uint32_t size = rate / 10;     // 100ms

	for (uint32_t p = 0; p < frames; p += size) {
		uint32_t f = min(frames - p, size);
		osa.data[0] = rawData + p * bpf;
		osa.frames = f;
		osa.timestamp = ts;
		obs_source_output_audio(ctx->source, &osa); // limit 1 second
		ts += 1000000000 / rate * f;
		// blog(LOG_INFO,
		//      "comment_audio rate:%d frames:%d position:%d ts:%llu",
		//		     rate, f, p, ts);
		os_sleepto_ns(ts - shift); //  need to wait
	}
}

static void *comment_thread(void *pdata)
{
	struct comment_data *ctx = pdata;
	HANDLE hPipe = NULL;
	DWORD bufSize = 1024 * 1024;
	DWORD dwRead;

	while (os_event_try(ctx->event) == EAGAIN) {
		os_sleep_ms(10);

		if (!hPipe) {
			hPipe = CreateNamedPipe(L"\\\\.\\pipe\\comment_audio",
						PIPE_ACCESS_DUPLEX,
						PIPE_TYPE_MESSAGE |
							PIPE_READMODE_MESSAGE |
							PIPE_NOWAIT,
						PIPE_UNLIMITED_INSTANCES, 1024,
						bufSize, 0, NULL);

			if (hPipe == INVALID_HANDLE_VALUE)
				continue;
		}

		ConnectNamedPipe(hPipe, NULL);
		DWORD ret = GetLastError();
		if (ret == ERROR_PIPE_LISTENING)
			continue;
		if (ret != ERROR_PIPE_CONNECTED) {
			CloseHandle(hPipe);
			hPipe = NULL;
			continue;
		}

		blog(LOG_INFO, "comment_audio open pipe");

		struct Header header;
		uint8_t *buf = NULL;
		uint32_t pos = 0;
		uint32_t size = 0;

		while (os_event_try(ctx->event) == EAGAIN) {
			os_sleep_ms(10);
			DWORD len = pos ? size - pos : sizeof(header);
			uint8_t *p = pos ? buf + pos : (uint8_t *)&header;
			dwRead = 0;
			ReadFile(hPipe, p, len, &dwRead, NULL);
			DWORD ret = GetLastError();
			if (ret == ERROR_NO_DATA || ret == ERROR_IO_PENDING)
				continue;

			blog(LOG_INFO,
			     "comment_audio read read:%d len:%d status:%d",
			     dwRead, len, ret);

			if (!dwRead) {
				CloseHandle(hPipe);
				hPipe = NULL;
				break;
			}

			if (!pos) {
				if (header.magic != 0x2525 ||
				    header.bitRate != 16) {
					blog(LOG_INFO,
					     "comment_audio wrong header");
					continue;
				}
				size = header.dataLength + header.headerLength;
				buf = bzalloc(size);
				memcpy(buf, &header, dwRead);
				pos = dwRead;
				continue;
			}

			pos += dwRead;
			if (pos < size)
				continue;

			doPlay(ctx, buf);
			bfree(buf);
			buf = NULL;
			pos = size = 0;
			header.magic = 0;
		}

		if (buf)
			bfree(buf);
	}

	if (hPipe)
		CloseHandle(hPipe);

	return NULL;
}

/* ------------------------------------------------------------------------- */

static const char *comment_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Comment audio1";
}

static void comment_destroy(void *data)
{
	struct comment_data *ctx = data;

	if (ctx) {
		if (ctx->initialized_thread) {
			void *ret;
			os_event_signal(ctx->event);
			pthread_join(ctx->thread, &ret);
		}

		os_event_destroy(ctx->event);
		bfree(ctx);
	}
}

static void *comment_create(obs_data_t *settings, obs_source_t *source)
{
	struct comment_data *ctx = bzalloc(sizeof(struct comment_data));
	ctx->source = source;

	if (os_event_init(&ctx->event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&ctx->thread, NULL, comment_thread, ctx) != 0)
		goto fail;

	ctx->initialized_thread = true;

	UNUSED_PARAMETER(settings);
	return ctx;

fail:
	comment_destroy(ctx);
	return NULL;
}

struct obs_source_info comment_audio = {
	.id = "comment_audio",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = comment_getname,
	.create = comment_create,
	.destroy = comment_destroy,
};
