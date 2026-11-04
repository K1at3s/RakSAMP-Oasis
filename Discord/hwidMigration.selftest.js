const assert = require('node:assert/strict');
const { migrateHwid } = require('./migrateHwid');
const oldHwid = 'BURHAN-1234-5678-90AB-CDEF-BOT';
const newHwid = 'BURHAN2-' + 'A'.repeat(32) + '-' + 'B'.repeat(16);

function fixture() {
    const state = {
        licenses: [{ hwid: oldHwid, is_approved: 1, welcome_enabled: 0, welcome_message: 'hello' }],
        guilds: [{ guild_id: 'guild', linked_hwids: JSON.stringify([oldHwid, 'OTHER']) }],
        welcome: [], writes: [], committed: false, rolledBack: false, fail: false
    };
    const connection = {
        async beginTransaction() {},
        async commit() { state.committed = true; },
        async rollback() { state.rolledBack = true; },
        async execute(sql, params) {
            if (sql.startsWith('SELECT * FROM licenses')) return [state.licenses];
            if (sql.startsWith('SELECT guild_id')) return [state.guilds];
            if (sql.startsWith('SELECT hwid FROM welcome_per_user')) return [state.welcome];
            if (state.fail && sql.startsWith('UPDATE welcome_per_user')) throw new Error('write failed');
            state.writes.push({ sql, params });
            return [{ affectedRows: 1 }];
        }
    };
    return { state, connection };
}

(async () => {
    let f = fixture();
    const plan = await migrateHwid(f.connection, oldHwid, newHwid);
    assert.equal(plan.applied, false);
    assert.equal(f.state.writes.length, 0);
    assert.equal(f.state.rolledBack, true);
    f = fixture();
    f.state.licenses.push({ hwid: newHwid, is_approved: 0, welcome_enabled: 1, welcome_message: '' });
    await migrateHwid(f.connection, oldHwid, newHwid, { apply: true });
    assert.equal(f.state.committed, true);
    assert.equal(f.state.writes[0].sql, 'DELETE FROM licenses WHERE hwid=?');
    const guildWrite = f.state.writes.find(write => write.sql.startsWith('UPDATE guilds'));
    assert.deepEqual(JSON.parse(guildWrite.params[0]), [newHwid, 'OTHER']);
    assert(f.state.writes.some(write => write.sql.startsWith('UPDATE welcome_per_user')));
    assert(f.state.writes.some(write => write.sql.startsWith('UPDATE group_logs')));
    for (const change of [
        state => { state.licenses[0].is_approved = 0; },
        state => { state.licenses.push({ hwid: newHwid, is_approved: 1 }); },
        state => { state.guilds.push({ guild_id: 'ambiguous', linked_hwids: JSON.stringify([oldHwid]) }); },
        state => { state.guilds[0].linked_hwids = JSON.stringify([newHwid]); },
        state => { state.welcome.push({ hwid: newHwid }); },
    ]) {
        f = fixture(); change(f.state);
        await assert.rejects(() => migrateHwid(f.connection, oldHwid, newHwid, { apply: true }));
        assert.equal(f.state.writes.length, 0);
        assert.equal(f.state.rolledBack, true);
    }
    f = fixture(); f.state.fail = true;
    await assert.rejects(() => migrateHwid(f.connection, oldHwid, newHwid, { apply: true }), /write failed/);
    assert.equal(f.state.rolledBack, true);
    assert.equal(f.state.committed, false);
    await assert.rejects(() => migrateHwid(f.connection, 'invalid', newHwid));
    console.log('HWID migration: OK (preview, preservation, conflicts, rollback)');
})().catch(error => { console.error(error); process.exitCode = 1; });
