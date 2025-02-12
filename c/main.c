/**
 * Copyright (c) 2006-2010 Spotify Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#define USER_AGENT "spotify_to_rdio"

#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include "arraylist.h"
#include "main.h"
#include "uthash.h"

sp_session *g_session;

static int notify_events;
static pthread_mutex_t notify_mutex;
static pthread_cond_t notify_cond;

struct s2r_playlist {
  char *name;
  struct array_list *tracks;
  UT_hash_handle hh;
};

struct s2r_playlist *playlists = NULL;

struct s2r_track {
  char *title;
  char *artist;
  char *album;
};

void free_s2r_track(void *data)
{
  struct s2r_track *track = (struct s2r_track *)data;
  free(track->title);
  free(track->artist);
  free(track->album);
  free(track);
}

static void container_loaded(sp_playlistcontainer *pc, void *userdata);
static void playlist_state_changed(sp_playlist *pl, void *userdata);
static void logged_in(sp_session *session, sp_error error);
static void connection_error(sp_session *session, sp_error error);
static void logged_out(sp_session *session);
static void log_message(sp_session *session, const char *data);
void notify_main_thread(sp_session *session);

static sp_playlistcontainer_callbacks pc_callbacks = {
	.container_loaded = &container_loaded
};

static sp_playlist_callbacks pl_callbacks = {
  .playlist_state_changed = &playlist_state_changed
};

static sp_session_callbacks callbacks = {
	.logged_in = &logged_in,
	.logged_out = &logged_out,
	.connection_error = &connection_error,
	.notify_main_thread = &notify_main_thread,
	.log_message = &log_message
};

static void container_loaded(sp_playlistcontainer *pc, void *userdata)
{
  int num_playlists = sp_playlistcontainer_num_playlists(pc);
	printf("Rootlist synchronized (%d playlists)\n", num_playlists);

  int i;
  for (i = 0; i < num_playlists; i++) {
    sp_playlist *pl = sp_playlistcontainer_playlist(pc, i);
    sp_playlist_add_callbacks(pl, &pl_callbacks, NULL);
  }
}

static void playlist_state_changed(sp_playlist *pl, void *userdata)
{
  if (sp_playlist_is_loaded(pl)) {
    const char *pl_name = sp_playlist_name(pl);

    // create pl_name in uthash

    int num_tracks = sp_playlist_num_tracks(pl);

    /*printf("Playlist (%d): %s\n", num_tracks, pl_name);*/

    int i;
    for (i = 0; i < num_tracks; i++) {
      const char *title = NULL, *artist = NULL, *album = NULL;

      sp_track *track = sp_playlist_track(pl, i);
      title = sp_track_name(track);

      sp_album *track_album = sp_track_album(track);
      album = sp_album_name(track_album);

      sp_artist *track_artist = sp_album_artist(track_album);
      artist = sp_artist_name(track_artist);

      struct s2r_track *trk = malloc(sizeof(struct s2r_track));
      trk->title = malloc(strlen(title) + 1);
      strcpy(trk->title, title);
      trk->artist = malloc(strlen(artist) + 1);
      strcpy(trk->artist, artist);
      trk->album = malloc(strlen(album) + 1);
      strcpy(trk->album, album);

       // add to tracks in playlists

      printf("Playlist (%d): %s, Track: %s - %s - %s\n",
          num_tracks, pl_name, title, artist, album);
    }

  }
}

static void logged_in(sp_session *session, sp_error error)
{
  sp_playlistcontainer *pc = sp_session_playlistcontainer(session);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "spotify_to_rdio: Login failed: %s\n",
			sp_error_message(error));
		exit(2);
	}

	sp_playlistcontainer_add_callbacks(pc, &pc_callbacks, NULL);

  sp_user *user = sp_session_user(session);
  const char *name = sp_user_canonical_name(user);
  printf("Logging in user: %s\n", name);
}

static void connection_error(sp_session *session, sp_error error)
{

}

static void logged_out(sp_session *session)
{
  sp_user *user = sp_session_user(session);
  const char *name = sp_user_canonical_name(user);
  printf("Logging out user: %s\n", name);
	exit(0);
}


/**
 * This callback is called for log messages.
 *
 * @sa sp_session_callbacks#log_message
 */
static void log_message(sp_session *session, const char *data)
{
  /*fprintf(stderr,"%s",data);*/
}

void notify_main_thread(sp_session *session)
{
	pthread_mutex_lock(&notify_mutex);
	notify_events = 1;
	pthread_cond_signal(&notify_cond);
	pthread_mutex_unlock(&notify_mutex);
}


int spotify_init(const char *username,const char *password)
{
	sp_session_config config;
	sp_error error;
	sp_session *session;

	/// The application key is specific to each project, and allows Spotify
	/// to produce statistics on how our service is used.
	extern const char g_appkey[];
	/// The size of the application key.
	extern const size_t g_appkey_size;

	// Always do this. It allows libspotify to check for
	// header/library inconsistencies.
	config.api_version = SPOTIFY_API_VERSION;

	// The path of the directory to store the cache. This must be specified.
	// Please read the documentation on preferred values.
	config.cache_location = "tmp";

	// The path of the directory to store the settings.
	// This must be specified.
	// Please read the documentation on preferred values.
	config.settings_location = "tmp";

	// The key of the application. They are generated by Spotify,
	// and are specific to each application using libspotify.
	config.application_key = g_appkey;
	config.application_key_size = g_appkey_size;

	// This identifies the application using some
	// free-text string [1, 255] characters.
	config.user_agent = USER_AGENT;

	// Register the callbacks.
	config.callbacks = &callbacks;

	error = sp_session_create(&config, &session);
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to create session: %s\n",
		                sp_error_message(error));
		return 2;
	}

	// Login using the credentials given on the command line.
	error = sp_session_login(session, username, password, 0, NULL);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to login: %s\n",
		                sp_error_message(error));
		return 3;
	}

	g_session = session;
	return 0;
}

int main(int argc, char **argv)
{
	int next_timeout = 0;
	if(argc < 3) {
		fprintf(stderr,"Usage: %s <username> <password>\n",argv[0]);
		exit(-1);
	}
	pthread_mutex_init(&notify_mutex, NULL);
	pthread_cond_init(&notify_cond, NULL);

	if(spotify_init(argv[1],argv[2]) != 0) {
		fprintf(stderr,"Spotify failed to initialize\n");
		exit(-1);
	}
	pthread_mutex_lock(&notify_mutex);
	for (;;) {
		if (next_timeout == 0) {
			while(!notify_events)
				pthread_cond_wait(&notify_cond, &notify_mutex);
		} else {
			struct timespec ts;

	#if _POSIX_TIMERS > 0
			clock_gettime(CLOCK_REALTIME, &ts);
	#else
			struct timeval tv;
			gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &ts);
	#endif

			ts.tv_sec += next_timeout / 1000;
			ts.tv_nsec += (next_timeout % 1000) * 1000000;

			while(!notify_events) {
				if(pthread_cond_timedwait(&notify_cond, &notify_mutex, &ts))
					break;
			}
		}

		// Process libspotify events
		notify_events = 0;
		pthread_mutex_unlock(&notify_mutex);

		do {
			sp_session_process_events(g_session, &next_timeout);
		} while (next_timeout == 0);

		pthread_mutex_lock(&notify_mutex);
	}

  sp_session_logout(g_session);
	return 0;
}
