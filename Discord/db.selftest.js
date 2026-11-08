// db.js koprusunun gercek SQL/operator mantigini MySQL'siz dogrular.
// mysql2/promise'i in-memory bir fake ile degistirir; uretilen SQL sekillerini isler.
// Gercek ProxySQL/MySQL baglantisi kurulunca: DB=mysql ile manual dogrula.
const path = require('node:path');
process.env.DISCORD_BOT_TOKEN ||= 'selftest-placeholder';
process.env.RAKSAMP_AES_SECRET ||= 'selftest-only-secret-material-at-least-32-characters';

// --- fake mysql2/promise (yalnizca db.js'in urettigi 4 SQL sekli) ---
const state = {}; // { table: [ {col: val}, ... ] }
const fakeMysql2 = {
    createPool: () => ({
        async execute(sql, vals = []) {
            if (/^CREATE TABLE/.test(sql)) return [{ warnings: 0 }]; // sema bootstrap'i no-op
            if (/^SELECT \* FROM (\w+)/.test(sql)) {
                const t = /^SELECT \* FROM (\w+)/.exec(sql)[1];
                const conds = /WHERE (.+) LIMIT 1/.exec(sql)[1].split(' AND ');
                outer: for (const row of (state[t] || [])) {
                    for (let i = 0; i < conds.length; i++) {
                        const m = /^([\w_]+)(=\?| LIKE \?)$/.exec(conds[i].trim());
                        const col = m[1], isLike = m[2].includes('LIKE');
                        const want = String(vals[i]);
                        const have = String(row[col]);
                        if (isLike ? !have.includes(want.slice(1, -1)) : have !== want) continue outer;
                    }
                    return [[row]];
                }
                return [[]];
            }
            if (/^UPDATE (\w+)/.test(sql)) {
                const t = /^UPDATE (\w+)/.exec(sql)[1];
                const sets = [], wheres = [];
                const splitSet = /SET (.+) WHERE ([\w_]+=\?)/.exec(sql);
                sets.push(...splitSet[1].split(', '));
                wheres.push(...splitSet[2].split(', '));
                const setPairs = sets.map(s => { const m = /^([\w_]+)=\?$/.exec(s.trim()); return [m[1], vals[sets.indexOf(s)]]; });
                const whereVals = vals.slice(sets.length);
                const wherePairs = wheres.map(w => { const m = /^([\w_]+)=\?$/.exec(w.trim()); return [m[1], whereVals[wheres.indexOf(w)]]; });
                let n = 0;
                for (const row of (state[t] || [])) {
                    if (wherePairs.every(([c, v]) => String(row[c]) === String(v))) {
                        for (const [c, v] of setPairs) row[c] = v;
                        n++;
                    }
                }
                return [{ affectedRows: n }];
            }
            if (/^INSERT INTO (\w+)/.test(sql)) {
                const m = /^INSERT INTO (\w+) \(([\w_, ]+)\) VALUES \(([\?, ]+)\)/.exec(sql);
                const t = m[1];
                const cols = m[2].split(', ').map(s => s.trim());
                const row = {};
                cols.forEach((c, i) => row[c] = vals[i]);
                (state[t] = state[t] || []).push(row);
                return [{ affectedRows: 1 }];
            }
            throw new Error('fake mysql: desteklenmeyen SQL: ' + sql);
        }
    })
};
const mod = require.resolve('mysql2/promise', { paths: [__dirname] });
require.cache[mod] = { id: mod, filename: mod, loaded: true, exports: fakeMysql2 };

process.env.DB = 'mysql';
const assert = require('node:assert');
const { model, init } = require('./db');

(async () => {
    await init(); // sema bootstrap'i da fake uzerinde calisir (CREATE TABLE IF NOT EXISTS)
    const Guild = model('guilds_test2', {
        pkey: 'guildId',
        fields: {
            guildId: { col: 'guild_id', type: 'text', pk: true },
            linkedHwids: { col: 'linked_hwids', type: 'json', def: [] },
            groupChatEnabled: { col: 'group_chat_enabled', type: 'int', def: 0 },
            lang: { col: 'lang', type: 'text', def: 'en' },
            createdAt: { col: 'created_at', type: 'text', def: () => 'NOW' }
        }
    });

    const doc = await Guild.create({ guildId: 'g1' });
    assert.strictEqual(doc.guildId, 'g1');
    assert.deepStrictEqual(doc.linkedHwids, []);
    assert.strictEqual(doc.lang, 'en');
    assert.strictEqual(doc.createdAt, 'NOW');
    assert.ok(!doc.groupChatEnabled); // defaults yolu: ham 0 (truthiness app'te kullanilir)

    // $addToSet (link.js) — dedup
    const a = await Guild.findOneAndUpdate({ guildId: 'g1' }, { $addToSet: { linkedHwids: 'HWID-A' } }, {});
    assert.deepStrictEqual(a.linkedHwids, ['HWID-A']);
    const b = await Guild.findOneAndUpdate({ guildId: 'g1' }, { $addToSet: { linkedHwids: 'HWID-A' } }, {});
    assert.deepStrictEqual(b.linkedHwids, ['HWID-A']);

    // $set (setchannel.js / ?tr / groupchat toggle)
    const c = await Guild.findOneAndUpdate({ guildId: 'g1' }, { $set: { lang: 'tr', groupChatEnabled: true } }, {});
    assert.strictEqual(c.lang, 'tr');
    assert.strictEqual(c.groupChatEnabled, true);

    // $pull (unlink.js)
    const d = await Guild.findOneAndUpdate({ guildId: 'g1' }, { $pull: { linkedHwids: 'HWID-A' } }, {});
    assert.deepStrictEqual(d.linkedHwids, []);

    // upsert + linkedHwids sorgusu (guildUtils.getGuildByHwid)
    await Guild.findOneAndUpdate({ guildId: 'g2' }, { $set: { linkedHwids: ['HWID-X'] } }, {});
    const byHwid = await Guild.findOne({ linkedHwids: 'HWID-X' });
    assert.strictEqual(byHwid.guildId, 'g2');
    assert.strictEqual(await Guild.findOne({ linkedHwids: 'YOK' }), null);

    // instance.save() (server.js: guild.lang=...; guild.save())
    const e = await Guild.findOne({ guildId: 'g2' });
    e.lang = 'tr';
    await e.save();
    assert.strictEqual((await Guild.findOne({ guildId: 'g2' })).lang, 'tr');

    console.log('db.js shim: OK — create/findOne/findOneAndUpdate($set,$addToSet,$pull)/save/upsert/JSON sorgu dogru.');
    process.exit(0);
})().catch(e => { console.error('FAIL:', e.stack); process.exit(1); });
