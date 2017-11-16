const WebSocket = require('ws');

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

  // Proxy functions to work on behalf of another PeerConnection
  onRemoteAnswer: null,
  onRemoteCandidate: null,

  forwardLocalDescription: function(desc) {
    // send a desc to gridspace
    this._send({
      type: "desc",
      body: desc
    });
  },

  forwardLocalCandidate: function(candidate) {
    // send a candidate to gridspace
    this._send({
      type: "candidate",
      body: candidate
    });
  },

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

    console.log('gsproxy: authenticate');
    console.log('open ws connection to ' + this.GRIDSPACE_VOIP_URI);

    this._token = token;

    this._sock = new WebSocket(this.GRIDSPACE_VOIP_URI);
    this._sock.on('open', () => {
      console.log('gsproxy: wsopen');
      this._onSocketConnected();
    });
    this._sock.on('headers', (h) => {
      console.log('gsproxy: wsheaders', h);
    })
    this._sock.on('unexpected-response', (req, res) => {
      console.log(`gsproxy: unexpected-response ${res.statusCode} ${res.statusMessage}`);
    })
    this._sock.on('ping', (p) => {
      this._sock.pong(p);
    });
    this._sock.on('pong', (p) => {
      console.log('gsproxy: pong', p);
    });
    this._sock.on('message', (msg) => {
      console.log('gsproxy: incoming', msg);
      this._onSocketMessage(JSON.parse(msg));
    });
    this._sock.on('error', (err) => {
      console.log('gsproxy: wserr', err);
      this._onSocketError(err);
    });
    this._sock.on('close', (c) => {
      console.log('gsproxy: wsclose', c);
      this._onSocketClose();
    });
  },

  connect: function(data) {
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
    this._onVoipConnected();
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
    console.log('gsproxy: outgoing msg', message);
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
        this.onRemoteAnswer(desc);
      } else {
        log("Unsupported SDP type. Your code may differ here.");
      }
    } else if (message.type === 'candidate'){
      this.onRemoteCandidate(message.body);
    } else if (message.type === 'hangup') {
      if (this._state === this.State.CONNECTED) {
        this._state = this.State.AUTHENTICATED;
        this._disconnect();
        if (this.onRemotePartyHungUp) {
          this.onRemotePartyHungUp();
        }
      }
    } else if (message.type === 'transfer') {
      console.log("unhandled message: transfer");
    } else if (message.type === 'reset') {
      this.reset();
    } else if (message.type === 'error') {
      console.error('RTC error message: ', message);
    } else {
      console.error('Unknown message type:', message.type);
    }
  },

  _disconnect: function() {

  },

  _state: 'uninitialized',
  _sock: null,
};

  // Export for node so we can use as a module.
  if (typeof exports !== 'undefined') {
    if (typeof module !== 'undefined' && module.exports) {
      exports = module.exports = Sift;
    }
  }
