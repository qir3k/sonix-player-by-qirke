#include "audio_state_machine.h"
#include "src/system/audio/audio.h"
#include "src/system/audio/alsa-controls.h"
#include "src/system/streaming/radio.h"
#include "src/system/streaming/subsonicstream.h"
#include <stdio.h>
#include <unistd.h>
#if !defined(HOST_BUILD) || defined(__GLIBC__)
#include <malloc.h>
#endif

static audio_sm_state_t current_sm_state = AUDIO_SM_IDLE;

audio_sm_state_t audio_sm_get_state(void) {
	return current_sm_state;
}

const char *audio_sm_get_state_name(audio_sm_state_t state) {
	switch (state) {
		case AUDIO_SM_IDLE: return "IDLE";
		case AUDIO_SM_STOPPING: return "STOPPING";
		case AUDIO_SM_CLEANUP: return "CLEANUP";
		case AUDIO_SM_BUFFERING: return "BUFFERING";
		case AUDIO_SM_PLAYING: return "PLAYING";
		default: return "UNKNOWN";
	}
}

void audio_sm_set_state(audio_sm_state_t state) {
	current_sm_state = state;
	fprintf(stderr, "audio_sm: state -> [%s]\n", audio_sm_get_state_name(state));
}

void audio_sm_stop_and_cleanup(void) {
	// a) Natychmiast zatrzymaj wyjście ALSA (pcm_drop / wyciszenie)
	audio_sm_set_state(AUDIO_SM_STOPPING);
	alsa_track_mute();
	audio_stop();
	radio_clear();
	subsonicstream_cancel();

	// b) Czysto zamknij socket sieciowy i wątek poprzedniego dekodera
	audio_sm_set_state(AUDIO_SM_CLEANUP);
	usleep(25000); // 25 ms settle time for decoder loop & ALSA pcm drain/drop

	// c) Wywołaj 'malloc_trim(0)', aby zresetować pamięć sterty po starym strumieniu
#if !defined(HOST_BUILD) || defined(__GLIBC__)
	malloc_trim(0);
#endif
}

bool audio_sm_switch_stream(const char *filepath, double position) {
	if (!filepath || filepath[0] == '\0') {
		audio_sm_stop_and_cleanup();
		audio_sm_set_state(AUDIO_SM_IDLE);
		return false;
	}

	// [STOPPING] -> [CLEANUP]
	audio_sm_stop_and_cleanup();

	// [BUFFERING]
	audio_sm_set_state(AUDIO_SM_BUFFERING);

	// [PLAYING] - Po całkowitym zwolnieniu zasobów starego strumienia, uruchom nowy dekoder
	audio_sm_set_state(AUDIO_SM_PLAYING);
	if (position > 0) {
		return (audio_play_at(filepath, position) == 0);
	} else {
		return (audio_play(filepath) == 0);
	}
}
