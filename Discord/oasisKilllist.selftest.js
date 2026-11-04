const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { validateKillListContext } = require('./utils/killListContext');

// Exercise the actual route without starting the service or connecting to Discord/MySQL.
const source = fs.readFileSync(path.join(__dirname, 'server.js'), 'utf8');
const start = source.indexOf("app.post('/api/killlist'");
const end = source.indexOf('// Oyun', start);
assert(start >= 0 && end > start);
let handler, fields, deliveryOK = true, delivered = 0;
class Embed {
    setTitle() { return this; }
    setDescription() { return this; }
    setColor() { return this; }
    setTimestamp() { return this; }
    addFields(value) { fields = value; return this; }
}
new Function('app', 'createWarResultDelivery', 'db', 'guildUtils',
    'validateKillListContext', 'EmbedBuilder', 'crypto', 'sendWarResult', 'console', source.slice(start, end))(
    { post: (_, callback) => { handler = callback; } },
    () => ({ deliver: async () => { delivered++; return deliveryOK ? { success: true, channelId: 'test' } : { success: false, pending: true }; } }),
    {}, { getGuildByHwid: async () => ({ guildId: 'test', groupName: 'TestGroup', lang: 'en' }) },
    validateKillListContext, Embed, crypto, () => {}, { log() {} });
const body = {
    hwid: 'test', playerName: 'ReplayBot', warId: 'test-war', wonKnown: true,
    ownGroup: 'TestGroup', redName: 'TestGroup', blueName: 'EnemyGroup', won: 1,
    redKills: 3, blueKills: 0, redBox: 'Name K D\nReplayBot 3 0', blueBox: ''
};
async function call(payload) {
    const response = { statusCode: 200, status(code) { this.statusCode = code; return this; }, json(value) { this.body = value; return this; } };
    await handler({ body: payload }, response);
    return response;
}
(async () => {
    let response = await call(body);
    assert.equal(response.body.success, true);
    assert.equal(fields.length, 1);
    response = await call({ ...body, ownGroup: 'Other' });
    assert.equal(response.statusCode, 409);
    assert.equal(delivered, 1);
    deliveryOK = false;
    response = await call(body);
    assert.equal(response.statusCode, 503);
    assert.equal(response.body.pending, true);
    console.log('Oasis killlist: OK (single box, wrong group, uncertain delivery)');
})().catch(error => { console.error(error); process.exitCode = 1; });
