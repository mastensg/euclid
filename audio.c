#include <err.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <alsa/asoundlib.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <pthread.h>

#include "audio.h"

#define UNUSED(x) (void)(x)

#define TIME_SLICE 16384

static char *buf;
static size_t size;

static unsigned buffer_time = 8192;
static unsigned rate = 44100;

static snd_pcm_t* playback_handle = 0;
static snd_pcm_hw_params_t* hw_params = 0;
static snd_pcm_sframes_t delay = 0;
static snd_pcm_sframes_t written = 0;

void
alsa_init()
{
    char *device;
    int err;

    device = getenv("ALSA_DEVICE");

    if(!device)
        device = "default";

    if((err = snd_pcm_open(&playback_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        errx(1, "Cannot open audio device %s: %s", device, snd_strerror(err));

    if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
        errx(1, "Cannot allocate hardware parameter structure: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params_any(playback_handle, hw_params)) < 0)
        errx(1, "Cannot initialize hardware parameter structure: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        errx(1, "Cannot set access type: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
        errx(1, "Cannot set sample format: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0)) < 0)
        errx(1, "Cannot set sample rate: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2)) < 0)
        errx(1, "Cannot set channel count: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params_set_buffer_time_near(playback_handle, hw_params, &buffer_time, 0)) < 0)
        errx(1, "Cannot set buffer time: %s", snd_strerror(err));

    if((err = snd_pcm_hw_params(playback_handle, hw_params)) < 0)
        errx(1, "Cannot set parameters: %s", snd_strerror(err));

    snd_pcm_hw_params_free(hw_params);

    if((err = snd_pcm_prepare(playback_handle)) < 0)
        errx(1, "Cannot prepare audio interface for use: %s", snd_strerror(err));
}

size_t
alsa_offset()
{
    snd_pcm_delay(playback_handle, &delay);

    return written - delay;
}

static size_t
alsa_write(const void *samples, size_t numbytes)
{
    int ret;

    ret = snd_pcm_writei(playback_handle, samples, numbytes);

    if(ret == -EAGAIN)
        return 0;

    if(ret < 0) {
        if((ret = snd_pcm_recover(playback_handle, ret, 0)) < 0)
            errx(EXIT_FAILURE, "ALSA playback failed: %s", strerror(-ret));
    }

    written += ret;

    return ret;
}

#define MIN(a,b) ((a)>(b)?(b):(a))
static void *
play_thread(void *arg)
{
    char *p;
    size_t fill, n;

    UNUSED(arg);

    p = buf;
    n = size;

    for (;n;)
    {
        fill = alsa_write(p, n % TIME_SLICE);
        p += fill * 4;
        n -= fill * 4;

        if (alsa_offset() > 105 * 4 * 44100)
            break;
    }

    return NULL;
}

static char *
oggvorbis_decode(const char *path)
{
    int ret, section;
    FILE *f;
    char *p;
    OggVorbis_File vf;

    if (NULL == (f = fopen(path, "r")))
        err(EXIT_FAILURE, "fopen");

    if(0 > (ret = ov_open_callbacks(f, &vf, NULL, 0, OV_CALLBACKS_NOCLOSE)))
        errx(EXIT_FAILURE, "ov_open_callbacks: %d", ret);

    size = 4 * ov_pcm_total(&vf, -1);

    if (NULL == (buf = malloc(size + 16 * 4 * 44100)))
        err(EXIT_FAILURE, "malloc buf");

    memset(buf, '\0', size + 16 * 4 * 44100);

    for (p = buf;;p += ret)
    {
        ret = ov_read(&vf, p, 4096, 0, 2, 1, &section);

        if (ret == 0) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "decode error\n");
        }
    }

    ov_clear(&vf);

    return buf;
}

void
alsa_play(const char *path)
{
    pthread_attr_t attr;
    pthread_t audio;

    oggvorbis_decode(path);

    pthread_attr_init(&attr);

    if(pthread_create(&audio, &attr, play_thread, NULL))
        err(EXIT_FAILURE, "pthread_create");
}
