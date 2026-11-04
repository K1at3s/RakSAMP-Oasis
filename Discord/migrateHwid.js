// Offline administrator tool. Stop the backend and affected clients before --apply.
// No legacy HWID is trusted or automatically granted access by the new client.
async function migrateHwid(connection, oldHwid, newHwid, { apply = false } = {}) {
    if (!/^BURHAN-[A-Z0-9_-]{1,56}$/.test(oldHwid || '') ||
        !/^BURHAN2-[A-F0-9]{32}-[A-F0-9]{16}$/.test(newHwid || '')) {
        throw new Error('Expected a legacy BURHAN- HWID and a new BURHAN2- HWID.');
    }
    await connection.beginTransaction();
    try {
        const [licenses] = await connection.execute('SELECT * FROM licenses WHERE hwid IN (?,?) FOR UPDATE', [oldHwid, newHwid]);
        const source = licenses.find(row => row.hwid === oldHwid);
        const target = licenses.find(row => row.hwid === newHwid);
        if (!source || !source.is_approved) throw new Error('The old HWID must have an approved license.');
        if (target && (target.is_approved || target.welcome_message || !target.welcome_enabled)) {
            throw new Error('The new HWID already has license settings; refusing to overwrite them.');
        }
        const [guilds] = await connection.execute(
            'SELECT guild_id, linked_hwids FROM guilds WHERE JSON_CONTAINS(linked_hwids, JSON_QUOTE(?)) OR JSON_CONTAINS(linked_hwids, JSON_QUOTE(?)) FOR UPDATE',
            [oldHwid, newHwid]);
        if (guilds.length > 1) throw new Error('The HWID has ambiguous guild links; resolve them before migration.');
        const links = guilds.map(row => ({ ...row, values: JSON.parse(row.linked_hwids || '[]') }));
        if (links.some(row => row.values.includes(newHwid))) throw new Error('The new HWID is already linked.');
        const [welcome] = await connection.execute('SELECT hwid FROM welcome_per_user WHERE hwid=? FOR UPDATE', [newHwid]);
        if (welcome.length) throw new Error('The new HWID already has per-user welcome settings.');
        const plan = { oldHwid, newHwid, guilds: guilds.map(row => row.guild_id), applied: apply };
        if (!apply) { await connection.rollback(); return plan; }
        // A first handshake may already have created an empty, unapproved target.
        if (target) await connection.execute('DELETE FROM licenses WHERE hwid=?', [newHwid]);
        await connection.execute('UPDATE licenses SET hwid=? WHERE hwid=?', [newHwid, oldHwid]);
        for (const row of links) {
            await connection.execute('UPDATE guilds SET linked_hwids=? WHERE guild_id=?',
                [JSON.stringify(row.values.map(value => value === oldHwid ? newHwid : value)), row.guild_id]);
        }
        await connection.execute('UPDATE welcome_per_user SET hwid=? WHERE hwid=?', [newHwid, oldHwid]);
        await connection.execute('UPDATE group_logs SET hwid=? WHERE hwid=?', [newHwid, oldHwid]);
        await connection.execute("UPDATE bots SET data=JSON_SET(data, '$.hwid', ?) WHERE CASE WHEN JSON_VALID(data) THEN JSON_UNQUOTE(JSON_EXTRACT(data, '$.hwid')) ELSE NULL END=?", [newHwid, oldHwid]);
        await connection.commit();
        return plan;
    } catch (error) {
        await connection.rollback();
        throw error;
    }
}

module.exports = { migrateHwid };
if (require.main === module) {
    (async () => {
        const args = process.argv.slice(2);
        if (args.length < 2 || args.length > 3 || (args[2] && args[2] !== '--apply')) {
            throw new Error('Usage: bun migrateHwid.js OLD_HWID NEW_HWID [--apply] (default: preview only)');
        }
        const mysql = require('mysql2/promise');
        const connection = await mysql.createConnection(require('./config').MYSQL);
        try { console.log(JSON.stringify(await migrateHwid(connection, args[0], args[1], { apply: args[2] === '--apply' }), null, 2)); }
        finally { await connection.end(); }
    })().catch(error => { console.error(error.message); process.exitCode = 1; });
}
