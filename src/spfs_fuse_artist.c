#include "spfs_fuse_artist.h"
#include "spfs_fuse_album.h"
#include "spfs_spotify.h"

static int name_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	char *str = g_strdup(spotify_artist_name(
				spotify_artistbrowse_artist(e->parent->auxdata)
				));
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

static int albums_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	GArray *albums = spotify_get_artistbrowse_albums(e->parent->auxdata);
	if (!albums)
		return 0;

	for (guint i = 0; i < albums->len; ++i) {
		sp_album *album = g_array_index(albums, sp_album *, i);

		spfs_entity *album_browse_dir = create_album_browse_dir(album);
		const gchar *album_name = spotify_album_name(album);
		/* FIXME: Deal with duplicates (re-releases, region availability.) properly
		 * Probably by presenting the album that has the most tracks available
		 * to the user as suggested here: http://stackoverflow.com/a/11994581
		 *
		 * XXX: Perhaps, it'd be a good idea to also add the release year to the album title
		 * like "Ride The Lightning (1984)" to further differentiate between re-releases
		 */
		if (!spfs_entity_dir_has_child(e->e.dir, album_name)) {
			spfs_entity *album_link = spfs_entity_link_create(album_name, NULL);
			spfs_entity_dir_add_child(e, album_link);
			gchar *rpath = relpath(e, album_browse_dir);
			spfs_entity_link_set_target(album_link, rpath);
			g_free(rpath);
		}
	}
	g_array_free(albums, false);
	return 0;
}

static int biography_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	spfs_entity *e = (spfs_entity *)fi->fh;
	char *str = spotify_artistbrowse_biography(e->parent->auxdata);
	READ_OFFSET(str, buf, size, offset);
	g_free(str);
	return size;
}

spfs_entity *create_artist_browse_dir(sp_artist *artist) {
	g_return_val_if_fail(artist != NULL, NULL);
	spfs_entity *artist_browse_dir = spfs_entity_find_path(SPFS_DATA->root, "/browse/artists");
	sp_link *link = spotify_link_create_from_artist(artist);

	g_return_val_if_fail(link != NULL, NULL);
	gchar artist_linkstring[1024] = {0, };
	spotify_link_as_string(link, artist_linkstring, 1024);
	if (spfs_entity_dir_has_child(artist_browse_dir->e.dir, artist_linkstring)) {
		return spfs_entity_dir_get_child(artist_browse_dir->e.dir, artist_linkstring);
	}
	spfs_entity *artist_dir = spfs_entity_dir_create(artist_linkstring, NULL);
	artist_dir->auxdata = spotify_artistbrowse_create(SPFS_SP_SESSION, artist);

	spfs_entity_dir_add_child(artist_dir,
			spfs_entity_file_create("name", name_read));

	spfs_entity_dir_add_child(artist_dir,
			spfs_entity_file_create("biography", biography_read));

	spfs_entity_dir_add_child(artist_dir,
			spfs_entity_dir_create("albums", albums_readdir));

	spfs_entity_dir_add_child(artist_browse_dir, artist_dir);
	return artist_dir;
}


