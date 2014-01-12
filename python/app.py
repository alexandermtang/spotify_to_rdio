import sys
import spotify_to_rdio
import keys  # put your key/secret in keys.py
import oauth2 as oauth
import urllib

from threading import Timer
from flask import Flask, render_template

data = []
app = Flask(__name__)

CONSUMER_KEY = keys.CONSUMER_KEY
CONSUMER_SECRET = keys.CONSUMER_SECRET

consumer = oauth.Consumer(CONSUMER_KEY, CONSUMER_SECRET)

@app.route("/")
def index():
    return render_template("index.html")


# probably not the right way to logout
def logout(session):
    global data
    data = session.get_data()
    #for playlist in data:
        #print "%s (%d)" % (playlist.name, len(playlist.tracks))
    session.disconnect()


def main(argv):
    if len(argv) < 3:
        print "Usage:"
        print "python spotify_to_rdio.py <username> <password>"
        sys.exit(1)

    username = argv[1]
    password = argv[2]

    session = spotify_to_rdio.SessionManager(username, password)
    t = Timer(4, logout, [session])
    t.start()
    session.connect()

    for playlist in data:
        print "%s (%d)" % (playlist.name, len(playlist.tracks))

    print CONSUMER_KEY, CONSUMER_SECRET

    #add songs to rdio


if __name__ == "__main__":
    main(sys.argv)

#if __name__ == "__main__":
    #app.run('0.0.0.0', debug=True)  # change to port 80 later
