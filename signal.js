var request = require('request');

module.exports.send = function(streamId, utterance) {
  var sessionId = "";
  var projectId = "";
  var secret = ""
  var jwt = ""

  var url = `https://api.opentok.com/v2/project/${projectId}/session/${sessionId}/signal`;

  var signal = { streamId: streamId, phrase: utterance }

  var options = {
    url: url,
    headers: {
      'X-TB-PARTNER-AUTH': `${projectId}:${secret}`
    },
    json: {
      "type": "stt", 
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
