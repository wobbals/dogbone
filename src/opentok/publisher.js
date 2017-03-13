var RSVP = require('rsvp');
var _ = require('underscore');
var uuid = require('node-uuid');
var Resource = require('./resource');
var RaptorMessage = require('./raptor').Message;

module.exports = function(raptor, options) {
  // assert(options.pc);
  var streamId = options.streamId || uuid.v4();
  var uri = '/v2/partner/' + options.apiKey + '/session/' + options.sessionId + '/stream/' + streamId;
  var pc = options.pc;
  var channels = [];
  if (options.audio) {
    channels.push({ id: uuid.v4(), type: 'audio', active: 'true' });
  }
  // https://github.com/opentok/wormhole/issues/82
  options.name = options.name || '';
  var publisher = _.extend(Resource(streamId, uri),
  {
    partnerId: options.apiKey,
    sessionId: options.sessionId,
    close: function(err) {
      if (err) {
        options.log('PublisherClose: err: ' + err);
      }
      pc.close();
    },
    send: function(msg) {
      var promise = raptor.send(msg);
      promise.catch(function() {
        publisher.close(new Error());
      });
      return promise;
    },
    init: function() {
      raptor.on('resource#' + uri, function(msg) {
        if (msg.method == 'answer') {
          // options.log('AnswerRecv', { sdp: msg.content.sdp });
          publisher.emit('answer', msg.content.sdp);
        }
        if (msg.method == 'generateoffer') {
          publisher.emit('generateoffer');
        }
      });
    },
    create: function() {
      var create = RaptorMessage.streams.create(options.apiKey, options.sessionId, streamId, options.name, false, channels);
      publisher.send(create).then(function() {
      });
    },
    offer: function(sdp) {
      // options.log('OfferSend', { sdp: sdp });

      var offer = RaptorMessage.offer(uri, sdp);
      return publisher.send(offer);
    }
  });

  publisher.init();
  publisher.create();

  return publisher;
}
