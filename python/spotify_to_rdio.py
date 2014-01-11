import sys
import spotify.manager

from threading import Timer

class SessionManager(spotify.manager.SpotifySessionManager,
                     spotify.manager.SpotifyContainerManager):
    appkey_file = "spotify_appkey.key"

    def logged_in(self, session, error):
        print "User %s logged in" % (session.display_name())
        self.watch(session.playlist_container())

    #def container_loaded(self, playlist_container, userdata):
        #for item in playlist_container:
            #if item.type() == "playlist" and item.is_loaded():
                #item.add_playlist_state_changed_callback(
                    #self.playlist_state_changed)

    #def playlist_state_changed(self, playlist, userdata):
        #if playlist.is_loaded():
            #print "%s (%d tracks)" % (playlist.name(), len(playlist))
            #for track in playlist:
                #print "    %s - %s - %s" % (track.name(),
                                            #track.artists()[0].name(),
                                            #track.album().name())

    def logged_out(self, session):
        print "User %s logged out" % (session.display_name())


def logout(session):
    session.disconnect()

def main(argv):
    if len(argv) < 3:
        print "Usage:"
        print "python spotify_to_rdio.py <username> <password>"
        sys.exit(1)

    username = argv[1]
    password = argv[2]

    session = SessionManager(username, password)
    t = Timer(2, logout, [session])
    t.start()
    session.connect()


if __name__ == "__main__":
    main(sys.argv)
