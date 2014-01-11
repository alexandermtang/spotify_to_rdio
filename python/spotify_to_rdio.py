import spotify.manager

class Playlist:
    def __init__(self, name, tracks):
        self.name = name
        self.tracks = tracks

    def __eq__(self, other):
        return self.name == other.name


class Track:
    def __init__(self, title, artist, album):
        self.title = title
        self.artist = artist
        self.album = album


class SessionManager(spotify.manager.SpotifySessionManager,
                     spotify.manager.SpotifyContainerManager):
    appkey_file = "spotify_appkey.key"
    data = []

    def logged_in(self, session, error):
        print "User %s logged in" % (session.display_name())
        self.watch(session.playlist_container())

    def container_loaded(self, playlist_container, userdata):
        for item in playlist_container:
            if item.type() == "playlist" and item.is_loaded():
                item.add_playlist_state_changed_callback(
                    self.playlist_state_changed)

    def playlist_state_changed(self, playlist, userdata):
        if playlist.is_loaded():
            #print "%s (%d tracks)" % (playlist.name(), len(playlist))
            name = playlist.name()
            tracks = []

            for track in playlist:
                title = track.name()
                artist = track.artists()[0].name()
                album = track.album().name()
                #print "    %s - %s - %s" % (title, artist, album)
                track_obj = Track(title, artist, album)
                tracks.append(track_obj)

            playlist = Playlist(name, tracks)
            if playlist not in self.data:
                self.data.append(playlist)

    def logged_out(self, session):
        print "User %s logged out" % (session.display_name())

    def log_message(self, session, message):
        print message

    def get_data(self):
        return self.data
