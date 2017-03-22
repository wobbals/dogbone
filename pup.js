var zmq = require('zeromq');
var watson = require('./watson.js');

var controller = zmq.socket('push');
controller.bindSync('ipc:///tmp/dogbone');

var receiver = zmq.socket('pull');
receiver.connect('ipc:///tmp/pup');

module.exports.sendControlMessage = function(message) {
  controller.send(message);
};

receiver.on('message', function(streamId, path){
  console.log(`received ${streamId}, ${path} from pup`);
  watson.recognizeStreamPath(streamId, path);
});
