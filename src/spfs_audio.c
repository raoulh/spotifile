#include "spfs_audio.h"

void spfs_audio_free(spfs_audio *audio) {
	g_free(audio);
}

bool spfs_audio_playback_is_playing(spfs_audio_playback *playback) {
	g_mutex_lock(&playback->mutex);
	bool ret = playback->playing != NULL;
	g_mutex_unlock(&playback->mutex);
	return ret;

}
void spfs_audio_playback_flush(spfs_audio_playback *playback) {
	g_mutex_lock(&playback->mutex);
	spfs_audio *audio = NULL;
	while ((audio = g_queue_pop_head(playback->queue)) != NULL) {
		playback->nsamples -= audio->nsamples;
		spfs_audio_free(audio);
	}
	g_warn_if_fail(playback->nsamples == 0);
	g_warn_if_fail(g_queue_is_empty(playback->queue));
	playback->stutter = 0;
	g_cond_signal(&playback->cond);
	g_mutex_unlock(&playback->mutex);
}

spfs_audio_playback *spfs_audio_playback_new(void) {
	spfs_audio_playback *pbk = g_new0(spfs_audio_playback, 1);
	pbk->queue = g_queue_new();
	pbk->playing = NULL;
	g_cond_init(&pbk->cond);
	g_mutex_init(&pbk->mutex);
	return pbk;
}

void spfs_audio_playback_free(spfs_audio_playback *playback) {
	spfs_audio_playback_flush(playback);
	g_queue_free(playback->queue);
	g_cond_clear(&playback->cond);
	g_mutex_clear(&playback->mutex);
	g_free(playback);
}


