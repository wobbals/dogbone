var RSVP = require('rsvp');
var _ = require('underscore');
var uuid = require('node-uuid');
var Resource = require('./resource');
var RaptorMessage = require('./raptor').Message;

module.exports = function(raptor, options) {
  // assert(options.pc);
  var subscriberId = uuid.v4();
  var uri = '/v2/partner/' + options.apiKey + '/session/' + options.sessionId + '/stream/' + options.streamId  + '/subscriber/' + subscriberId;
  var pc = options.pc;
  var subscriber = _.extend(Resource(subscriberId, uri),
  {
    partnerId: options.apiKey,
    sessionId: options.sessionId,
    streamId: options.streamId,
    close: function(err) {
      if (err) {
        options.log('SubscriberClose: err: ' + err);
      }
      pc.close();
    },
    send: function(msg) {
      var promise = raptor.send(msg);
      promise.catch(function() {
        subscriber.close(new Error());
      });
      return promise;
    },
    init: function() {
      raptor.on('resource#' + uri, function(msg) {
        if (msg.method == 'offer') {
          subscriber.emit('offer', msg.content.sdp);
        }
        if (msg.method == 'candidate') {
          subscriber.emit('candidate', msg.content);
        }
      });
    },
    create: function() {
      var channels = [];
      var create = RaptorMessage.subscribers.create(options.apiKey, options.sessionId, options.streamId, subscriberId, '', channels);
      subscriber.send(create);
    },
    answer: function(sdp) {
      var answer = RaptorMessage.subscribers.answer(options.apiKey, options.sessionId, options.streamId, subscriberId, sdp);
      return subscriber.send(answer);
    },
    remoteCandidate: function(candidate) {
      let msg = RaptorMessage.subscribers.candidate(
        options.apiKey, options.sessionId, options.streamId,
        subscriberId, candidate
      );
      return subscriber.send(msg);
    },
    close: function() {
      // TODO: destroy subscriber
    },
  });

  subscriber.init();
  subscriber.create();

  return subscriber;
}
