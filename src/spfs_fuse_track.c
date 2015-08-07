#include "spfs_fuse_track.h"
#include "spfs_fuse_artist.h"
#include "spfs_spotify.h"

static int is_starred_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	char *str = spotify_track_is_starred(SPFS_SP_SESSION, e->parent->auxdata) ? "1\n" : "0\n";
	READ_OFFSET(str, buf, size, offset);
	return size;
}

static int is_local_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	char *str = spotify_track_is_local(SPFS_SP_SESSION, e->parent->auxdata) ? "1\n" : "0\n";
	READ_OFFSET(str, buf, size, offset);
	return size;
}

static int is_autolinked_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	char *str = spotify_track_is_autolinked(SPFS_SP_SESSION, e->parent->auxdata) ? "1\n" : "0\n";
	READ_OFFSET(str, buf, size, offset);
	return size;
}

static int offlinestatus_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	char *str = g_strdup_printf("%s\n", spotify_track_offline_status_str(
			spotify_track_offline_get_status(e->parent->auxdata)
			));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

static int name_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	gchar *str = g_strdup_printf("%s\n", (spotify_track_name(e->parent->auxdata)));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

static int disc_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	gchar *str = g_strdup_printf("%d\n",
			spotify_track_disc(e->parent->auxdata));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

static int index_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	gchar *str = g_strdup_printf("%d\n",
			spotify_track_index(e->parent->auxdata));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

static int popularity_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	gchar *str = g_strdup_printf("%d\n",
			spotify_track_popularity(e->parent->auxdata));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

static int duration_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	gchar *str = g_strdup_printf("%d\n",
			spotify_track_duration(e->parent->auxdata));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

struct wav_header {
	char riff[4];
	uint32_t size;
	char type[4];
	char fmt[4];
	uint32_t fmt_size;
	uint16_t fmt_type;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
	char data[4];
	uint32_t data_size;
};

static void fill_wav_header(struct wav_header *h, int channels, int rate, int duration) {
	// A highly inflexible wave implementation, it gets the job done though
	// See, for example,
	// http://people.ece.cornell.edu/land/courses/ece4760/FinalProjects/f2014/wz233/ECE%204760%20Final%20Report%20%28HTML%29/ECE%204760%20Stereo%20Player_files/RIFF%20WAVE%20file%20format.jpg
	memcpy(h->riff, "RIFF", 4);
	h->bits_per_sample = 16; //only supported sample type as of now
	h->size = (sizeof(*h) + ((duration / 1000) * channels * h->bits_per_sample * rate) / 8) - 8;
	memcpy(h->type, "WAVE", 4);
	memcpy(h->fmt, "fmt ", 4);
	h->fmt_size = 16;
	h->fmt_type = 1; //PCM
	h->channels = channels;
	h->sample_rate = rate;
	h->byte_rate = (rate * h->bits_per_sample * channels) / 8;
	h->block_align = (h->bits_per_sample * channels) / 8;
	memcpy(h->data, "data", 4);
	h->data_size = h->size - 44;
}

static int wav_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	static off_t expoff = 0;
	spfs_entity *e = (spfs_entity *)fi->fh;
	sp_session *session = SPFS_SP_SESSION;
	int channels, rate, duration;

	if (offset == 0) {
		if (!spotify_play_track(session, e->parent->auxdata)) {
			g_warning("Failed to play track!");
			return 0;
		}
	}

	memset(buf, 0, size);
	struct wav_header header;
	size_t read = 0;


	if ((size_t) offset < sizeof(header)) {
		memset(&header, 0, sizeof(header));
		duration = spotify_get_track_info(&channels, &rate);
		fill_wav_header(&header, channels, rate, duration);

		if ( offset + size > sizeof(header))
			read = sizeof(header) - offset;
		else
			read = size;
		memcpy(buf, (char*)&header + offset, read);
	}

	if (expoff != offset) {
		//seek
		duration = spotify_get_track_info(&channels, &rate);
		size_t bytes_p_s = (channels * 16 * rate) / 8;

		int ms_offset = (offset / bytes_p_s) * 1000;

		if (duration < ms_offset)
			g_warn_if_reached();

		spotify_seek_track(session, ms_offset);
		expoff = offset;
	}

	if (read < size && (offset + read >= sizeof(header))) {
		// get some audio, but only if we've read the entire header
		read += spotify_get_audio(buf+read, size - read);
	}
	expoff += read;
	return read;
}

int artists_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	sp_track *track = e->parent->auxdata;
	int num_artists = spotify_track_num_artists(track);
	for (int i = 0; i < num_artists; ++i) {
		sp_artist *artist = spotify_track_artist(track, i);
		const gchar *artistname = spotify_artist_name(artist);
		spfs_entity *artist_browse_dir = create_artist_browse_dir(artist);
		/*
		 * FIXME: Deal with duplicates (artists with the same name).
		 */
		if (!spfs_entity_dir_has_child(e->e.dir, artistname)) {
			spfs_entity *artist_link = spfs_entity_link_create(artistname, NULL);
			spfs_entity_dir_add_child(e, artist_link);
			gchar *rpath = relpath(e, artist_browse_dir);
			spfs_entity_link_set_target(artist_link, rpath);
			g_free(rpath);
		}
	}
	return 0;
}

spfs_entity *create_track_browse_dir(sp_track *track) {
	g_return_val_if_fail(track != NULL, NULL);
	spfs_entity *track_browse_dir = spfs_entity_find_path(SPFS_DATA->root, "/browse/tracks");
	sp_link *link = spotify_link_create_from_track(track);

	g_return_val_if_fail(link != NULL, NULL);
	gchar track_linkstring[1024] = {0, };
	spotify_link_as_string(link, track_linkstring, 1024);
	if (spfs_entity_dir_has_child(track_browse_dir->e.dir, track_linkstring)) {
		return spfs_entity_dir_get_child(track_browse_dir->e.dir, track_linkstring);
	}
	spfs_entity *track_dir = spfs_entity_dir_create(track_linkstring, NULL);
	track_dir->auxdata = track;

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("name", name_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("track.wav", wav_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("duration", duration_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("popularity", popularity_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("index", index_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("disc", disc_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("starred", is_starred_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("local", is_local_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("autolinked", is_autolinked_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_file_create("offlinestatus", offlinestatus_read));

	spfs_entity_dir_add_child(track_dir,
			spfs_entity_dir_create("artists", artists_readdir));
	spfs_entity_dir_add_child(track_browse_dir, track_dir);
	return track_dir;
}


