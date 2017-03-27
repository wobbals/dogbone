var request = require('request');

var nameSent = false;

var sendSignal = function(type, signal) {
  var sessionId = "";
  var projectId = "";
  var secret = "";

  var url = `https://anvil-tbdev.opentok.com/v2/project/${projectId}/session/${sessionId}/signal`;

  var options = {
    url: url,
    headers: {
      'X-TB-PARTNER-AUTH': `${projectId}:${secret}`
    },
    json: {
      "type": type, 
      "data": JSON.stringify(signal)
    }
  };

  function callback(error, response, body) {
    if (!error && response.statusCode == 200) {
      console.log("sent signal for " + utterance);
    }
    //console.log(JSON.stringify(response, null, ' '));
  }
  //console.log(JSON.stringify(options, null, ' '));
  request.post(options, callback);
};

module.exports.utterance = function(streamId, utterance) {
  if (!nameSent) {
    nameSent = true;
    var signal = "watson";
    var type = "name";
    sendSignal(type, signal);
  }
  var signal = { "body": utterance, "date": new Date().getTime()/1000 };
  var type = "message";
  sendSignal(type, signal);
};
