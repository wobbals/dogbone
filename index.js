var fs = require('fs');
const OpenTok = require("./src/opentok/opentok.js");
var logging = require('snap-framework').Logging;
var logger = logging.getLogger('opentok');
var log4js = require('log4js');
log4js.configure({
  "appenders": [
    { "type": 'console' },
  ],
  "levels": {
    "[all]": "DEBUG",
  },
  replaceConsole: true
});
log4js.getLogger('webrtcconnection').setLevel('INFO');

var zmq = require('zeromq')
  , sock = zmq.socket('push');
sock.bindSync('ipc:///tmp/dogbone');

var TBPeerConnection = require('@opentok/neutrino-util').TokboxPeerConnection;
var RTPUtil = require('@opentok/neutrino-util').RtpUtil;

var sessionId =  "2_MX4xMzExMjU3MX5-MTQ4OTI3MjkwNTQzOX5YblYxa04xMnMzTTNtaTNJYkNTbWJNemd-fg";
var token = "T1==cGFydG5lcl9pZD0xMzExMjU3MSZzZGtfdmVyc2lvbj10YnBocC12MC45MS4yMDExLTA3LTA1JnNpZz00Mzg4YTY2ODZjZGE5NWQwM2YyNjFiZDJiMDEyMzAxNWQ3MmY5YWMyOnNlc3Npb25faWQ9Ml9NWDR4TXpFeE1qVTNNWDUtTVRRNE9USTNNamt3TlRRek9YNVlibFl4YTA0eE1uTXpUVE50YVROSllrTlRiV0pOZW1kLWZnJmNyZWF0ZV90aW1lPTE0ODkzNTA3Nzgmcm9sZT1tb2RlcmF0b3Imbm9uY2U9MTQ4OTM1MDc3OC43NjQxNTgzOTM0MDMyJmV4cGlyZV90aW1lPTE0OTE5NDI3Nzg=";

var pc_config = {
  certificate: fs.readFileSync('conf/certificate.pem'), 
  decrypt: true, 
  useBundle: false,
  useRtcpMux: true,
  useFec: false,
  useIce: true
};

var pc = new TBPeerConnection(pc_config);

var ot = OpenTok({
  apiKey: 13112571,
  sessionId: sessionId,
  token: token,
  log: function(action, args) {
    logger.debug(logging.format(action, args));
  },
  apiUrl: "https://anvil.opentok.com"
});

ot.on('stream#created', function(stream) {
  logger.debug(`hello stream! ${JSON.stringify(stream, null, ' ')}`);
  var subscriber = ot.subscribe({ streamId: stream.id });
  subscriber.on('offer', function(sdp) {
    logger.debug(`received offer ${sdp}`);
    // send offer to dogbone socket
    //sock.send(offer);
    pc.setRemoteDescription({"type":"offer", "sdp":sdp}, function() {
      logger.debug('pc.setRemoteDescription');
      pc.createAnswer(function(answer) {
        logger.debug(`pc.answer: ${JSON.stringify(answer, null, ' ')}`);
        subscriber.answer(answer.sdp);
        pc.setLocalDescription(answer);
      });
    });
  });
})

pc.on('open', function() {
  logger.debug(`pc opened! remoteStream: ${pc.getRemoteStream()}`);
  pc.getRemoteStream().on("packet", function(packet) {
    var rtp = RTPUtil.decode(packet);
    logger.debug(`a real packet! ${JSON.stringify(rtp)}`);
    logger.debug(`payload length: ${packet.length}`);
    sock.send(packet);
  });
  pc.getRemoteStream().on("message", function(message) {
    logger.debug(`pc.stream.message: ${JSON.stringify(message)}`);
  });
});

var promise = ot.connect(sessionId, token);

promise.then(function() {
  console.log("connected??");
}).catch(function(err) {
  console.log('Error', err);
});

console.log(`sup`);
