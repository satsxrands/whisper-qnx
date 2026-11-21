/* Audio input abstraction for the QNX voice-command daemon.
 * All backends deliver 16 kHz, mono, signed 16-bit samples.
 *   file:<path.wav>     16 kHz mono 16-bit PCM WAV (works everywhere)
 *   tcp:<host>:<port>   raw s16le 16 kHz mono PCM stream from another machine
 *   snd:<pcm-name>      QNX io-snd capture via libasound (built only with -DHAVE_QNX_SND)
 */
#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AUDIO_RATE 16000
typedef struct audio_source audio_source;
/* Returns NULL on failure and writes a message into err. */
audio_source *audio_open(const char *spec, char *err, size_t errlen);
/* Blocks until n samples are available. Returns samples read (< n only at EOF), or -1 on error. */
long audio_read(audio_source *a, int16_t *buf, long n);
void audio_close(audio_source *a);
/* Name of the backend, for logging. */
const char *audio_backend(const audio_source *a);
#ifdef __cplusplus
}
#endif
#endif
