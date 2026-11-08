#ifndef AUDIO_STATE_MACHINE_H
#define AUDIO_STATE_MACHINE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Atomic sequential states of the audio pipeline:
// [STOPPING] -> [CLEANUP] -> [BUFFERING] -> [PLAYING]
typedef enum {
	AUDIO_SM_IDLE = 0,
	AUDIO_SM_STOPPING,
	AUDIO_SM_CLEANUP,
	AUDIO_SM_BUFFERING,
	AUDIO_SM_PLAYING
} audio_sm_state_t;

// Get the current audio state
audio_sm_state_t audio_sm_get_state(void);

// Get human-readable string representation of the state
const char *audio_sm_get_state_name(audio_sm_state_t state);

// Explicitly transition state
void audio_sm_set_state(audio_sm_state_t state);

// Sequential phases:
// a) Natychmiast zatrzymaj wyjście ALSA (pcm_drop / mute)
// b) Czysto zamknij socket sieciowy i wątek poprzedniego dekodera
// c) Wywołaj 'malloc_trim(0)', aby zresetować pamięć sterty po starym strumieniu
void audio_sm_stop_and_cleanup(void);

// Atomic switch between audio sources (local files, Tidal, YouTube Music, Radio, etc.)
// d) Dopiero po całkowitym zwolnieniu zasobów poprzedniego utworu zainicjalizuj połączenie i dekoder dla nowego strumienia.
bool audio_sm_switch_stream(const char *filepath, double position);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_STATE_MACHINE_H */
