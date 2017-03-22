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

var pup = require("./pup.js");

var TBPeerConnection = require('@opentok/neutrino-util').TokboxPeerConnection;
var RTPUtil = require('@opentok/neutrino-util').RtpUtil;

var sessionId = "";
var token = "";

var pc_config = {
  certificate: fs.readFileSync('conf/certificate.pem'), 
  decrypt: true, 
  useBundle: false,
  useRtcpMux: true,
  useFec: false,
  useIce: true
};

var ot = OpenTok({
  apiKey: 13112571,
  sessionId: sessionId,
  token: token,
  log: function(action, args) {
    logger.debug(logging.format(action, args));
  },
  apiUrl: "https://anvil.opentok.com"
});

var subscribers = {};

ot.on('stream#created', function(stream) {
  logger.debug(`hello stream! ${JSON.stringify(stream, null, ' ')}`);
  var subscriber = ot.subscribe({ streamId: stream.id });
  var pc = new TBPeerConnection(pc_config);

  subscribers[stream.id] = {
    signaling: subscriber,
    pc: pc
  }

  subscriber.on('offer', function(sdp) {
    logger.debug(`subscriber stream ${stream.id} received offer ${sdp}`);
    pup.sendControlMessage(['offer', stream.id, sdp]);
    pc.setRemoteDescription({"type":"offer", "sdp":sdp}, function() {
      logger.debug('pc.setRemoteDescription');
      pc.createAnswer(function(answer) {
        logger.debug(`pc.answer: ${JSON.stringify(answer, null, ' ')}`);
        subscriber.answer(answer.sdp);
        pc.setLocalDescription(answer);
      });
    });
  });
  
  pc.on('open', function() {
    logger.debug(`pc opened! pc: `+
      `${JSON.stringify(pc.id, null, ' ')}`);
    pc.getRemoteStream().on("packet", function(packet) {
      var rtp = RTPUtil.decode(packet);
      //logger.debug(`a real packet! ${JSON.stringify(rtp)}`);
      //logger.debug(`payload length: ${packet.length}`);
      pup.sendControlMessage(['data', stream.id, packet]);
    });
    pc.getRemoteStream().on("message", function(message) {
      logger.debug(`pc.stream.message: ${JSON.stringify(message)}`);
    });
  });
  
  pc.on('close', function() {
    // clean up
  });
})

var promise = ot.connect(sessionId, token);

promise.then(function() {
  console.log("connected??");
}).catch(function(err) {
  console.log('Error', err);
});

console.log(`sup`);
