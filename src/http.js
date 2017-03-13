'use strict';

var express = require('express');
var bodyParser = require('body-parser');
// var logging = require('snap-framework').Logging;
var stats = require('snap-framework').Stats;
var _ = require('underscore');
var http = require('snap-framework').HttpServer;
var https = require('https');
var fs = require('fs');

// var logger = logging.getLogger();

// Remember that a key which is an object will be stringified as "[Object object]"
// so this _MAY_ map different key-value to the same key, leading to loss of data
//
// TODO(guillermo): there will be a possible ES7 Map.toJSON(), but for now, this
// may work
let mapToJSON = function(json) {
  let rv = {};
  json.forEach(function(value, key) {
    rv[key] = value;
  });
  return rv;
};

let cleanConferences = function(channels) {
  return JSON.stringify(mapToJSON(channels), function(key, val) {
    if (key === 'pc' || key === 'call') {
      return null;
    }
    return val;
  });
};

module.exports = function(options) {
  var api = options.api;
  var state = options.state;
  var opentok = options.opentok;

  let _server;

  var app = express();
  app.use(bodyParser.json());
  app.use(http.filter({ name: 'http', blacklist: /\/server/i }));

  app.get('/server/ping', function(req, res) {
    api.ping().then(function(response) {
      res.status(200).send(response);
    }, function(err) {
      res.status(500).send(err);
    });
  });

  app.get('/server/config', function(req, res) {
    api.getConfig().then(function(config) {
      var clean = JSON.stringify(config, function(key, val) {
        if (key === 'password') {
          return undefined;
        }
        return val;
      });
      res.status(200).json(JSON.parse(clean));
    });
  });

  app.get('/server/health', function(req, res) {
    let httpStats = stats.stats();
    let stateStats = state.getStatsSummary();
    let opentokStats = opentok.getStatsSummary();
    let extended = _.extend(httpStats, { state: stateStats, opentok: opentokStats });
    res.send({
      stats: extended,
    });
  });

  app.get('/server/channels', function(req, res) {
    res.set('Content-Type', 'application/json');
    res.send(cleanConferences(state.conferences));
  });

  app.post('/v2/partner/:partnerId/dial', function(req, res) {
    const partnerId = req.params.partnerId;
    api.dial(req.body, partnerId).then(function(fields) {
      res.send(fields);
    }, function() {
      res.sendStatus(400);
    });
  });

  var server = {
    port: process.env.port || 4080,
    listen: function() {
      // SSL
      if (options.secure) {
        let httpsOpts = {
          key: fs.readFileSync(options.key),
          cert: fs.readFileSync(options.cert),
        };
        _server = https.createServer(httpsOpts, app).listen(server.port);
        return Promise.resolve();
      }
      // No SSL
      _server = app.listen(server.port);
      return Promise.resolve();
    },
    stop: function() {
      _server.close();
    },
  };
  return server;
};
