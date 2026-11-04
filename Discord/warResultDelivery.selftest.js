const assert = require('node:assert/strict');
const { createWarResultDelivery } = require('./utils/warResultDelivery');
const { setClient, sendWarResult } = require('./utils/discordNotifier');

function fixture() {
    const rows = new Map();
    let failCommit = false;
    const execute = async (sql, values) => {
        if (sql.startsWith('INSERT')) {
            const key = JSON.stringify(values);
            if (rows.has(key)) return { affectedRows: 0 };
            rows.set(key, { state: 'sending' }); return { affectedRows: 1 };
        }
        if (sql.startsWith('SELECT')) return rows.has(JSON.stringify(values)) ? [rows.get(JSON.stringify(values))] : [];
        if (sql.startsWith('DELETE')) { rows.delete(JSON.stringify(values)); return { affectedRows: 1 }; }
        if (sql.startsWith('UPDATE')) {
            if (failCommit) { failCommit = false; throw new Error('database offline after Discord send'); }
            rows.set(JSON.stringify(values.slice(1)), { state: 'delivered', channel_id: values[0] }); return { affectedRows: 1 };
        }
        throw new Error('Unexpected SQL');
    };
    return { rows, execute, failNextCommit: () => { failCommit = true; } };
}

(async () => {
    const db = fixture(); let sends = 0;
    const delivery = createWarResultDelivery(db.execute);
    const send = async allowSend => { if (allowSend) sends++; return 'channel'; };
    assert.equal((await delivery.deliver('BOT-A', 'WAR-1', send)).success, true);
    assert.equal((await createWarResultDelivery(db.execute).deliver('BOT-A', 'WAR-1', send)).duplicate, true);
    assert.equal(sends, 1); // backend restarted, receipt survives
    await delivery.deliver('BOT-B', 'WAR-1', send); assert.equal(sends, 2);
    db.failNextCommit();
    const unconfirmed = await delivery.deliver('BOT-A', 'WAR-2', send);
    assert.equal(unconfirmed.pending, true); assert.equal(sends, 3);
    const recovered = await createWarResultDelivery(db.execute).deliver('BOT-A', 'WAR-2', send);
    assert.equal(recovered.success, true); assert.equal(sends, 3); // history reconciliation only
    await delivery.deliver('BOT-A', 'WAR-3', async () => { throw Object.assign(new Error('permission'), { definitelyNotSent: true }); });
    assert.equal(db.rows.has(JSON.stringify(['BOT-A', 'WAR-3'])), false);
    await delivery.deliver('BOT-A', 'WAR-3', send); assert.equal(sends, 4);
    const uncertain = await delivery.deliver('BOT-A', 'WAR-4', async () => { throw new Error('socket closed during send'); });
    assert.equal(uncertain.pending, true);
    let permittedToSend = true;
    assert.equal((await delivery.deliver('BOT-A', 'WAR-4', async allow => { permittedToSend = allow; return null; })).pending, true);
    assert.equal(permittedToSend, false);

    let history = [], posts = 0;
    const channel = { id: 'channel', messages: { async fetch() { return history; } }, async send() { posts++; } };
    setClient({ user: { id: 'bot' }, channels: { async fetch() { return channel; } } });
    const guild = { zoneWarChannelId: 'channel' }, embed = { setFooter(value) { this.footer = value; return this; } };
    assert.equal(await sendWarResult(guild, embed, 'marker', true), 'channel'); assert.equal(posts, 1);
    history = [{ author: { id: 'bot' }, embeds: [{ footer: { text: 'marker' } }] }];
    assert.equal(await sendWarResult(guild, embed, 'marker', false), 'channel'); assert.equal(posts, 1);
    history = [];
    assert.equal(await sendWarResult(guild, embed, 'unknown', false), null); assert.equal(posts, 1);
    console.log('War delivery: OK (restart, isolation, uncertain send, reconciliation, safe retry)');
})().catch(error => { console.error(error); process.exitCode = 1; });
