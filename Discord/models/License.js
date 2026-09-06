// License: MySQL/ProxySQL shim (db.js).
module.exports = require('../db').model('licenses', {
    pkey: 'hwid',
    fields: {
        hwid: { col: 'hwid', type: 'text', pk: true },
        isApproved: { col: 'is_approved', type: 'int', def: 0 },
        welcomeEnabled: { col: 'welcome_enabled', type: 'int', def: 1 },
        welcomeMessage: { col: 'welcome_message', type: 'text', def: '' },
        createdAt: { col: 'created_at', type: 'text', def: () => new Date().toISOString() }
    }
});