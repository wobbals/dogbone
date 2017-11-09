const JWT = require('jsonwebtoken');

const url_escape = function(obj) {
  let components = [];
  for (let key in obj) {
    let value = obj[key];
    components.push(`${key}=${escape(value)}`);
  }
  return components.join('&');
}

const scope_uri_for = function(service, privilege, params) {
  let result = `scope:${service}:${privilege}`;
  if (params) {
    result += `?${url_escape(params)}`;
  }
  return result;
}

const generateToken = function(credentials, expires = 3600) {
  let capabilities =
  scope_uri_for('client', 'outgoing', {
    app_id: credentials.appId
  });
  let payload = {
    scope: capabilities,
    iss: credentials.accountId,
    exp: (Date.now() / 1000.0) + expires
  }
  return JWT.sign(payload, credentials.appSecret);
}

module.exports = {
  generateToken
}
