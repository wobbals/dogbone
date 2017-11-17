const gs = require('./gscapability');
let sift = require('./gsproxy');
const OpenTok = require("./src/opentok/opentok.js");
const log4js = require('log4js');
const config = require('config');
log4js.configure({
  "appenders": {
    'default': { "type": 'console' },
  },
  categories: { default: { appenders: ['default'], level: 'debug' } },
});
log4js.getLogger('webrtcconnection').level = 'INFO';
const logger = log4js.getLogger();

let gsToken = gs.generateToken({
  appId: config.get('GS_APP_ID'),
  accountId: config.get('GS_ACCOUNT_ID'),
  appSecret: config.get('GS_ACCOUNT_SECRET')
});

sift.onAuthenticated = function () {
  console.log('sift authenticated');
  openTokConnect();
}
sift.onIncomingCall = function() {
  console.log('sift incoming call');
}
sift.onConnected = function() {
  console.log('sift: connected');
}
sift.onRemotePartyHungUp = function() {
  console.log('sift: disconnected');
}
sift.onError = function(err) {
  console.log('sift: error ', err);
}

sift.authenticate(gsToken);

const sessionId = config.get('OT_SESSION_ID');
const token = config.get('OT_TOKEN');

let ot = OpenTok({
  apiKey: 100,
  sessionId: sessionId,
  token: token,
  log: function(action, args) {
    logger.debug(action, args);
  },
  apiUrl: "https://anvil-tbdev.opentok.com"
});

let subscribers = {};

ot.on('stream#created', function(stream) {
  logger.debug(`stream created! ${JSON.stringify(stream, null, ' ')}`);
  sift.connect(`ot-streamId:${stream.id}`);
  let subscriber = ot.subscribe({ streamId: stream.id });

  subscribers[stream.id] = {
    signaling: subscriber
  }

  subscriber.on('offer', function(sdp) {
    logger.debug(`subscriber stream ${stream.id} received offer ${sdp}`);
    sift.forwardOffer(sdp);
  });

  // this is only going to happen on p2p calls, which we have no good reason to
  // do, but hey - it's here for completeness
  subscriber.on('candidate', function(candidate) {
    logger.debug("subscriber candidate", candidate);
    sift.forwardLocalCandidate(candidate);
  });

  sift.onRemoteAnswer = function(answer) {
    console.log('sift: remote answer', answer);
    subscriber.answer(answer.sdp);
  };

  sift.onRemoteCandidate = function(candidate) {
    console.log('sift: remoteCandidate', candidate);
    subscriber.remoteCandidate(candidate);
  };
});

console.log(`sup`);

let openTokConnect = function() {
  ot.connect(sessionId, token)
  .then(function() {
    console.log("ot connected");
  }).catch(function(err) {
    console.log('Error', err);
  });
}