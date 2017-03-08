#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <assert.h>

#define log printf

long now() {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return 0;
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

int elapsed() {
    static long firstCall;
    if (!firstCall) {
        firstCall = now();
    }
    return now() - firstCall;
}

int main() {
    int errnum;
    unsigned int val;
    int dir;

    snd_pcm_t *handle;
    std::vector<unsigned char> buffer;
//    SoundDescription currentlyPlayed;
    unsigned char *nextToPlay;
    snd_pcm_hw_params_t *params;
    unsigned freq = 44100;

    /* Open PCM device for playback. */
    errnum = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (errnum < 0) {
        log("SOUND: unable to open pcm device: %s\n", snd_strerror(errnum));
        return 0;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);

    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
    snd_pcm_hw_params_set_channels(handle, params, 1);

    snd_pcm_hw_params_set_rate_near(handle, params, &freq, &dir);
    log("SOUND: setupWithFreq sampling rate: %d\n", freq);

    unsigned int nPeriods = 2;
    /* Set number of periods. Periods used to be called fragments. */
    if ((errnum = snd_pcm_hw_params_set_periods_near(handle, params, &nPeriods,
            0)) < 0) {
        log("SOUND: Error setting periods: %s\n", snd_strerror(errnum));
        return 0;
    }
    log("SOUND: number of periods: %d\n", nPeriods);

    snd_pcm_uframes_t nFrames = 1000;
    if (snd_pcm_hw_params_set_period_size_near(handle, params, &nFrames,
            &dir)) {
        log("SOUND: Error setting period size.\n");
        return 0;
    }
    log("SOUND: period size: %d frames\n", (int) nFrames);
    assert(nFrames > 950);

    /* Write the parameters to the driver */
    errnum = snd_pcm_hw_params(handle, params);
    if (errnum < 0) {
        log("SOUND: unable to set hw parameters: %s\n", snd_strerror(errnum));
        return 0;
    }
    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &nFrames, &dir);

    unsigned size = nFrames * 1; // 1 channel, 8 bit
    buffer.resize(size);

    // square of ~1kHz
    for (unsigned i = 0; i < nFrames; i++) {
        buffer[i] = (i % 40) > 20 ? 255 : 0;
    }

    while (true) {
        snd_pcm_prepare(handle);

        for (int i = 0; i < 3; i++) {
            log("%d {%d}\n", elapsed(), i);
            int ret;
            while ((ret = snd_pcm_writei(handle, &buffer[0], nFrames))
                    != nFrames) {
                if (ret < 0) {
                    log("ALSA error: %s\n", snd_strerror(ret));
                    if ((ret = snd_pcm_recover(handle, ret, 0)) < 0) {
                        log("ALSA error after recover: %s\n",
                                snd_strerror(ret));
                        if ((ret = snd_pcm_prepare(handle)) < 0) {
                            log("ALSA irrecoverable error: %s\n",
                                    snd_strerror(ret));
                            break;
                        }
                    }
                } else {
                    log("ALSA short write...?\n");
                    break;
                }
            }
        }
        log("%d before drain\n", elapsed());
        snd_pcm_drain(handle);
        log("%d after drain\n\n", elapsed());
        sleep(1);
    }
    return 0;
}
