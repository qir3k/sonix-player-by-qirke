#include "metaloader.h"

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "src/system/core/utils.h"

typedef struct {
	char filepath[512];
	uint32_t generation;
	bool pending;
	bool running;
	bool done;
	bool found;
	song_metadata_t metadata;
} metaloader_job_t;

static pthread_mutex_t meta_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t meta_cond = PTHREAD_COND_INITIALIZER;
static pthread_t worker_thread;
static bool started = false;

static metaloader_job_t current_job;
static uint32_t current_generation = 0;

static void *metaloader_worker(void *arg) {
	(void)arg;
	thread_be_low_priority("metaloader");

	for (;;) {
		pthread_mutex_lock(&meta_lock);
		while (!current_job.pending) {
			pthread_cond_wait(&meta_cond, &meta_lock);
		}

		char path[512];
		strncpy(path, current_job.filepath, sizeof(path) - 1);
		path[sizeof(path) - 1] = '\0';
		uint32_t gen = current_job.generation;

		current_job.pending = false;
		current_job.running = true;
		pthread_mutex_unlock(&meta_lock);

		// Perform SD Card / disk metadata extraction off the main UI thread
		song_metadata_t result;
		memset(&result, 0, sizeof(result));
		metadata_read(path, &result);

		pthread_mutex_lock(&meta_lock);
		current_job.running = false;
		if (current_job.generation == gen) {
			current_job.metadata = result;
			current_job.done = true;
			current_job.found = true;
		}
		pthread_mutex_unlock(&meta_lock);
	}
	return NULL;
}

void metaloader_init(void) {
	pthread_mutex_lock(&meta_lock);
	if (!started) {
		started = true;
		pthread_create(&worker_thread, NULL, metaloader_worker, NULL);
	}
	pthread_mutex_unlock(&meta_lock);
}

void metaloader_request(const char *filepath) {
	if (!filepath || !filepath[0]) {
		return;
	}
	metaloader_init();

	pthread_mutex_lock(&meta_lock);
	if (strcmp(current_job.filepath, filepath) == 0 &&
		(current_job.done || current_job.running || current_job.pending)) {
		pthread_mutex_unlock(&meta_lock);
		return;
	}
	current_generation++;
	current_job.generation = current_generation;
	strncpy(current_job.filepath, filepath, sizeof(current_job.filepath) - 1);
	current_job.filepath[sizeof(current_job.filepath) - 1] = '\0';
	current_job.pending = true;
	current_job.running = false;
	current_job.done = false;
	current_job.found = false;
	pthread_cond_signal(&meta_cond);
	pthread_mutex_unlock(&meta_lock);
}

bool metaloader_take(const char *filepath, song_metadata_t *out) {
	if (!filepath || !out) {
		return false;
	}
	bool ready = false;
	pthread_mutex_lock(&meta_lock);
	if (current_job.done && strcmp(current_job.filepath, filepath) == 0) {
		*out = current_job.metadata;
		ready = true;
	}
	pthread_mutex_unlock(&meta_lock);
	return ready;
}

void metaloader_get_placeholder(const char *filepath, song_metadata_t *out) {
	if (!out) {
		return;
	}
	memset(out, 0, sizeof(*out));
	if (!filepath || !filepath[0]) {
		return;
	}

	const char *slash = strrchr(filepath, '/');
	const char *filename = slash ? slash + 1 : filepath;
	snprintf(out->title, sizeof(out->title), "%s", filename);
	char *dot = strrchr(out->title, '.');
	if (dot) {
		*dot = '\0';
	}
	snprintf(out->artist, sizeof(out->artist), "Loading...");
}
