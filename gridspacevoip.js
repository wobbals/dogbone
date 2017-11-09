
var Sift = {

    State: Object.freeze({
        UNINITIALIZED: 'uninitialized',   // Initial state, have not received voip token.
        AUTHENTICATING: 'authenticating', // Sent authentication token, waiting for response.
        AUTHENTICATED: 'authenticated',   // Auth successful, ready to connect.
        RINGING: 'ringing',               // Incoming call is connecting.
        CONNECTING: 'connecting',         // Establishing a connection.
        CONNECTED: 'connected'            // Connected to a call.
    }),

    Error: Object.freeze({
        NONE: 'none',
        UNKNOWN: 'unknown',
        LOST_INTERNET: 'lost-internet',
        NO_MIC: 'no-mic',
        MIC_DISABLED: 'mic-disabled',
        MIC_PERMISSION_DENIED: 'mic-permission-denied',
        AUTHENTICATION_FAILED: 'authentication-failed',
        OUTGOING_NOT_PERMITTED: 'outgoing-not-permitted',
    }),

    GRIDSPACE_VOIP_URI: 'wss://api.gridspace.com/webrtc/',

    // Callback functions to be set by the client:
    onAuthenticated: null,
    onIncomingCall: null,
    onConnected: null,
    onRemotePartyHungUp: null,
    onError: null,

    authenticate: function(token) {
        try {
            this.GRIDSPACE_VOIP_URI = OVERRIDE_GRIDSPACE_VOIP_URI;
        } catch (err) {}

        if (this._state !== this.State.UNINITIALIZED) {
            throw 'Cannot call authenticate twice in a row';
        }

        if (!token) {
            throw 'Must provide a token';
        }

        this._state = this.State.AUTHENTICATING;

        // Create the audio element if needed.
        if (!document.getElementById('audio_remote')) {
            var remote_audio = document.createElement('audio');
            remote_audio.id = 'audio_remote';
            remote_audio.autoplay = true;
            document.body.appendChild(remote_audio);
        }

        console.log("voip: connect(" + token);

        this._token = token;

        this._sock = new WebSocket(this.GRIDSPACE_VOIP_URI);
        this._sock.onopen = this._onSocketConnected.bind(this);

        this._sock.onmessage = function(event) {
            var data = JSON.parse( event.data );
            this._onSocketMessage(data);
        }.bind(this);
        this._sock.onerror = this._onSocketError.bind(this);
        this._sock.onclose = this._onSocketClose.bind(this);
    },

    connect: function(data, mediaStream) {
        // Initiate an outbound call.
        // Call will be handled by the application bound to the Token passed to init. Data is an
        // optional opaque json-serializable object that will be passed in the POST parameters
        // to the initial connection url associated with the application.
        //
        // Mediastream is an optional mediaStream to use as the audio source for the connection.
        // Normally this is not provided and the mic is used by default.
        if (this._state !== this.State.AUTHENTICATED || !this._token) {
            throw 'Initialize before calling ready';
        }

        // Ready to begin the voip connection!
        this._state = this.State.CONNECTING;
        this._send({type: 'connect', body: data});
        this._connect(mediaStream);
    },

    mute: function() {
        this.setMuted(true);
    },
    unmute: function() {
        this.setMuted(false);
    },
    setMuted: function(muted) {
        if (muted != this.muted) {
            this.muted = muted;

            if (!this._localMediaStream || !this._localMediaStream.getAudioTracks) {
                return;
            }
            var tracks = this._localMediaStream.getAudioTracks();
            tracks.forEach(function(track) {
                track.enabled = !muted;
            });
        }
    },
    hangUp: function() {
        // Hang up an active connection.
        if (this._state === this.State.CONNECTED) {
            this._state = this.State.AUTHENTICATED;
            console.log('Voip: hangup');
            this._send({type: 'hangup'});
            this._disconnect();
        }
    },
    reset: function() {
        console.log("voip: reset");
        this.hangUp();
        if (this._sock) {
            this._sock.close();
            this._sock = null;
        }
        this._token = null;
        this._state = this.State.UNINITIALIZED;
    },
    state: function() {
        return this._state;
    },

    _onVoipConnected: function() {
        if (this._state == this.State.CONNECTING) {
            this._state = this.State.CONNECTED;
            if (this.onConnected) {
                this.onConnected();
            }
        } else {
            this._disconnect();
        }
    },

    _send: function(message) {
        this._sock.send(JSON.stringify(message));
    },

    _onSocketConnected: function() {
        if (this._state === this.State.AUTHENTICATING) {
            // Send the auth token.
            this._send({
                type: 'auth',
                body: this._token
            });
        }
    },

    _onSocketError: function() {
        if (this._state !== this.State.UNINITIALIZED) {
            this._disconnect();
            if (this.onError) {
                this.onError(this.Error.LOST_INTERNET);
            }
        }
    },

    _onSocketClose: function() {

    },

    answer: function() {
        if (this._state === this.State.RINGING) {
            this._send({
                type: 'ringanswer',
                body: true
            })
            this.muted = false;
            this._state = this.State.CONNECTING;
            this._connect();
        }
    },
    decline: function() {
        if (this._state === this.State.RINGING) {
            this._send({
                type: 'ringanswer',
                body: false
            })
            this._state = this.State.AUTHENTICATED;
        }
    },

    _onSocketMessage: function(message) {
        console.log('Rtc sock message:\n', message);
        if (message.type === 'auth') {
            if (this._state === this.State.AUTHENTICATING) {
                if (message.body === 'OK') {
                    this._state = this.State.AUTHENTICATED;
                    if (this.onAuthenticated) {
                        this.onAuthenticated();
                    }
                } else {
                    this._token = null;
                    this._state = this.State.UNINITIALIZED;
                    if (this.onError) {
                        this.onError(this.Error.AUTHENTICATION_FAILED);
                    }
                }
            }
        } else if (message.type === 'ring') {
            if (this._state === this.State.AUTHENTICATED) {
                this._state = this.State.RINGING;
                if (this.onIncomingCall) {
                    this.onIncomingCall();
                }
            }
        } else if (message.type === 'desc') {
            var desc = message.body;
            if (desc.type == "answer") {
                this._peerConnection.setRemoteDescription(new RTCSessionDescription( desc ));
            } else {
                log("Unsupported SDP type. Your code may differ here.");
            }
        } else if (message.type === 'candidate'){
            this._peerConnection.addIceCandidate(new RTCIceCandidate(message.body));
        } else if (message.type === 'hangup') {
            if (this._state === this.State.CONNECTED) {
                this._state = this.State.AUTHENTICATED;
                this._disconnect();
                if (this.onRemotePartyHungUp) {
                    this.onRemotePartyHungUp();
                }
            }
        } else if (message.type === 'transfer') {
            if (this._state === this.State.CONNECTED) {
                
                // Don't do a full disconnect because we want to keep the media stream.
                if (this._peerConnection) {
                    this._peerConnection.close();
                    this._peerConnection = null;
                }
                this._connect();
            }
        } else if (message.type === 'reset') {
            this.reset();
        } else if (message.type === 'error') {
            console.error('RTC error message: ', message);
        } else {
            console.error('Unknown message type:', message.type);
        }
    },

    _connect: function(mediaStream) {
        var configuration = {
            "iceServers": [
                {'urls': ['stun:stun.l.google.com:19302', 'stun:stun.sipgate.net:10000']}
            ]
        };
        this._peerConnection = new RTCPeerConnection(configuration);

        this._peerConnection.onicecandidate = function (evt) {
            console.log('onicecandidate', evt);
            if (evt.candidate) {
                this._send({
                    type: "candidate",
                    body: evt.candidate
                });
            }
        }.bind(this);

        // let the "negotiationneeded" event trigger offer generation
        this._peerConnection.onnegotiationneeded = function () {
            console.log("ON negotiationneeded");
            this._createAndSendOffer();
        }.bind(this);

        // once remote video track arrives, show it in the remote video element
        this._peerConnection.ontrack = function (evt) {
            console.log("ON REMOTE TRACK");
        };

        this._peerConnection.onaddstream = function(evt) {
            console.log("ON ADDSTREAAAAAAM!", evt);
            var remoteAudio = document.getElementById('audio_remote');
            remoteAudio.src = window.URL.createObjectURL(evt.stream);

            // This is where we'll say we're connected.
            this._onVoipConnected();
        }.bind(this);

        if (mediaStream) {
            this._localMediaStream = mediaStream;
        }

        // If we've already gotten the local media (for example, during a transfer)
        // don't bother requesting it again.
        if (this._localMediaStream) {
            this._peerConnection.addStream(this._localMediaStream);
        } else {
            // get a local stream, show it in a self-view and add it to be sent
            navigator.mediaDevices.getUserMedia({ "audio": true, "video": false })
            .then(function (stream) {
                // Make sure we haven't aborted in the meantime.
                if (this._state === this.State.CONNECTING
                        || this._state === this.State.CONNECTED) {
                    console.log("GOT USER MEDIA");
                    this._localMediaStream = stream;
                    this._peerConnection.addStream(stream);
                }
            }.bind(this))
            .catch(function(error) {
                console.error(error);
            });
        }
    },

    _createAndSendOffer: function() {
        console.log("Create and send offer");
        this._peerConnection.createOffer().then(function (offer) {
            return this._peerConnection.setLocalDescription(offer);
        }.bind(this)).then(function() {
            this._send({
                type: "desc",
                body: this._peerConnection.localDescription
            });
        }.bind(this));
    },

    _disconnect: function() {
        if (this._peerConnection) {
            this._peerConnection.close();
            this._peerConnection = null;
        }

        // NOTE: Firefox <44 has a bug where the icon still shows up after disconnecting. It's
        // a bug with them - not us. https://bugzilla.mozilla.org/show_bug.cgi?id=1192170
        if (this._localMediaStream) {
            var tracks = this._localMediaStream.getTracks();
            for (var i = 0; i < tracks.length; ++i) {
                tracks[i].stop();
            }
            this._localMediaStream = null;
        }
    },

    _state: 'uninitialized',
    _sock: null,
    _localMediaStream: null,
};

// Export for node so we can use as a module.
if (typeof exports !== 'undefined') {
    if (typeof module !== 'undefined' && module.exports) {
        exports = module.exports = Sift;
    }
}
