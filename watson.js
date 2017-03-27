var watson = require('watson-developer-cloud');
var SpeechToTextV1 = require('watson-developer-cloud/speech-to-text/v1');
var fs = require('fs');
var GrowingFile = require('growing-file');
var signal = require('./signal.js');

var speech_to_text = new SpeechToTextV1({
  username: '',
  password: '',
});

// var request = {
//   config: {
//     encoding: 'opus',
//     sampleRate: 48000
//   },
//   singleUtterance: false,
//   interimResults: false
// };
//
//
// var processChunk = function(path) {
//   fs.createReadStream('/tmp/output.opus')
//   .pipe(speech_to_text.createRecognizeStream(request))
//   .on('data', function(data) {
//     console.log(data.toString());
//   });
// };
//
// module.exports.processChunk = processChunk;

var params = {
  model: 'en-US_BroadbandModel',
  content_type: 'audio/ogg;codecs=opus',
  continuous: true,
  'interim_results': true,
  'max_alternatives': 1,
  'word_confidence': false,
  timestamps: true,
  inactivity_timeout: -1,
  profanity_filter: false
  // keywords: ['colorado', 'tornado', 'tornadoes'],
  // 'keywords_threshold': 0.5
};

module.exports.recognizeStreamPath = function(streamId, path) {
  // Create the stream.
  var recognizeStream = speech_to_text.createRecognizeStream(params);

  // Pipe in the audio.
  GrowingFile.open(path).pipe(recognizeStream);

  // Pipe out the transcription to a file.
  // recognizeStream.pipe(fs.createWriteStream('transcription.txt'));

  // Get strings instead of buffers from 'data' events.
  recognizeStream.setEncoding('utf8');

  // Listen for events.
  //recognizeStream.on('results', function(event) { onEvent('Results:', event); });
  recognizeStream.on('data', function(utterance) { 
    console.log(`${streamId}: ${JSON.stringify(utterance, null, ' ')}`);
    signal.utterance(streamId.toString(), utterance.toString())
  });
  recognizeStream.on('error', function(event) { onEvent('Error:', event); });
  recognizeStream.on('close', function(event) { onEvent('Close:', event); });
  recognizeStream.on('speaker_labels', function(event) {
    onEvent('Speaker_Labels:', event); 
  });

  // Displays events on the console.
  function onEvent(name, event) {
    console.log(name, JSON.stringify(event, null, 2));
  };
};
