/* Energy-threshold voice activity detector. Header-only, no dependencies.
 * Feed fixed 20 ms frames; it returns an event when an utterance starts or ends. */
#ifndef VAD_H
#define VAD_H
#include <math.h>
#include <stdint.h>
#define VAD_FRAME 320 /* 20 ms at 16 kHz */
typedef struct {
    double floor_db;      /* running noise floor estimate */
    double margin_db;     /* speech must exceed floor by this much */
    double abs_min_db;    /* and be above this absolute level (dBFS) */
    int start_frames;     /* consecutive loud frames to open */
    int hang_frames;      /* consecutive quiet frames to close */
    int loud_run, quiet_run, active;
} vad_t;
enum { VAD_NONE = 0, VAD_START = 1, VAD_END = 2 };
static inline void vad_init(vad_t *v) {
    v->floor_db = -70; v->margin_db = 12; v->abs_min_db = -45;
    v->start_frames = 4; v->hang_frames = 30; /* 80 ms to open, 600 ms to close */
    v->loud_run = v->quiet_run = v->active = 0;
}
static inline double vad_frame_db(const int16_t *s, int n) {
    double acc = 0;
    for (int i = 0; i < n; i++) { double x = s[i] / 32768.0; acc += x * x; }
    double rms = sqrt(acc / n);
    return 20.0 * log10(rms + 1e-9);
}
static inline int vad_push(vad_t *v, const int16_t *frame) {
    double db = vad_frame_db(frame, VAD_FRAME);
    double thr = v->floor_db + v->margin_db; if (thr < v->abs_min_db) thr = v->abs_min_db;
    int loud = db > thr;
    if (!v->active) {
        if (!loud) v->floor_db += 0.05 * (db - v->floor_db); /* track noise only while idle */
        v->loud_run = loud ? v->loud_run + 1 : 0;
        if (v->loud_run >= v->start_frames) { v->active = 1; v->quiet_run = 0; return VAD_START; }
    } else {
        v->quiet_run = loud ? 0 : v->quiet_run + 1;
        if (v->quiet_run >= v->hang_frames) { v->active = 0; v->loud_run = 0; return VAD_END; }
    }
    return VAD_NONE;
}
#endif
