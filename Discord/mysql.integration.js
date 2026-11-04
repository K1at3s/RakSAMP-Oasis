const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const mysql = require('mysql2/promise');
const { migrateHwid } = require('./migrateHwid');
const { createWarResultDelivery } = require('./utils/warResultDelivery');

// Never read config.js: only an explicitly supplied test server is used.
async function main() {
    if (!process.env.TEST_MYSQL_URL) throw new Error('TEST_MYSQL_URL is required; use tests/Run-LocalMySql.ps1 for an isolated server.');
    const url = new URL(process.env.TEST_MYSQL_URL);
    if (!['127.0.0.1', 'localhost'].includes(url.hostname)) throw new Error('The integration server must be local.');
    const options = { host: url.hostname, port: Number(url.port || 3306), user: decodeURIComponent(url.username), password: decodeURIComponent(url.password), supportBigNumbers: true };
    const schema = 'raksamp_test_' + crypto.randomBytes(8).toString('hex');
    assert.match(schema, /^raksamp_test_[a-f0-9]+$/);
    const admin = await mysql.createConnection(options);
    let connection, pool;
    try {
        await admin.query(`CREATE DATABASE \`${schema}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci`);
        const configPath = require.resolve('./config');
        require.cache[configPath] = { id: configPath, filename: configPath, loaded: true, exports: { MYSQL: { ...options, database: schema, connectionLimit: 8 } } };
        const db = require('./db');
        const { Guild, License } = require('./models');
        // Upgrade an actual pre-HWID log table, not just a freshly created schema.
        await admin.query(`CREATE TABLE \`${schema}\`.group_logs (
            id INT AUTO_INCREMENT PRIMARY KEY, actor_name VARCHAR(64), actor_acc BIGINT,
            target_name VARCHAR(64), target_acc BIGINT, old_rank VARCHAR(64), new_rank VARCHAR(64),
            reason VARCHAR(255), created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)`);
        pool = await db.init();
        await db.init(); // Repeated startup must preserve data and tolerate existing columns.
        connection = await mysql.createConnection({ ...options, database: schema });
        const oldHwid = 'BURHAN-INTEGRATION';
        const newHwid = 'BURHAN2-' + 'A'.repeat(32) + '-' + 'B'.repeat(16);
        await License.create({ hwid: oldHwid, isApproved: true, welcomeEnabled: false, welcomeMessage: 'Merhaba' });
        await Guild.create({ guildId: 'test-guild', linkedHwids: [oldHwid, 'OTHER'], groupName: 'TestGroup' });
        await connection.execute('INSERT INTO welcome_per_user (hwid,target_name,custom_message) VALUES (?,?,?)', [oldHwid, 'Ali', 'Welcome']);
        await connection.execute('INSERT INTO group_logs (hwid,actor_name) VALUES (?,?)', [oldHwid, 'Ali']);
        await connection.execute('INSERT INTO bots (name,data) VALUES (?,?)', ['TestBot', JSON.stringify({ hwid: oldHwid })]);
        // Real JSON_CONTAINS, JSON_SET, transaction rollback and license settings.
        await migrateHwid(connection, oldHwid, newHwid);
        assert.equal((await License.findOne({ hwid: oldHwid })).isApproved, true);
        assert.equal(await License.findOne({ hwid: newHwid }), null);
        await migrateHwid(connection, oldHwid, newHwid, { apply: true });
        const license = await License.findOne({ hwid: newHwid });
        assert.equal(license.isApproved, true);
        assert.equal(license.welcomeEnabled, false);
        assert.equal(license.welcomeMessage, 'Merhaba');
        assert.deepEqual((await Guild.findOne({ guildId: 'test-guild' })).linkedHwids, [newHwid, 'OTHER']);
        assert.equal((await connection.execute('SELECT hwid FROM welcome_per_user'))[0][0].hwid, newHwid);
        assert.equal((await connection.execute('SELECT hwid FROM group_logs'))[0][0].hwid, newHwid);
        assert.equal(JSON.parse((await connection.execute('SELECT data FROM bots'))[0][0].data).hwid, newHwid);
        // A failure after updating the license must roll the entire transaction back.
        const rollbackOld = 'BURHAN-ROLLBACK';
        const rollbackNew = 'BURHAN2-' + 'C'.repeat(32) + '-' + 'D'.repeat(16);
        await License.create({ hwid: rollbackOld, isApproved: true });
        const failing = {
            beginTransaction: () => connection.beginTransaction(), commit: () => connection.commit(), rollback: () => connection.rollback(),
            execute(sql, values) { if (sql.startsWith('UPDATE welcome_per_user')) throw new Error('injected write failure'); return connection.execute(sql, values); }
        };
        await assert.rejects(() => migrateHwid(failing, rollbackOld, rollbackNew, { apply: true }), /injected/);
        assert.equal((await License.findOne({ hwid: rollbackOld })).isApproved, true);
        assert.equal(await License.findOne({ hwid: rollbackNew }), null);
        // Concurrent receipt reservations must grant sending permission only once.
        let sends = 0;
        const send = async allow => { if (allow) { sends++; await new Promise(resolve => setTimeout(resolve, 10)); } return 'test-channel'; };
        const delivery = createWarResultDelivery(db.execute);
        const results = await Promise.all(Array.from({ length: 24 }, () => delivery.deliver(newHwid, 'WAR-1', send)));
        assert(results.every(result => result.success));
        assert.equal(sends, 1);
        const restarted = createWarResultDelivery(db.execute);
        assert.equal((await restarted.deliver(newHwid, 'WAR-1', send)).duplicate, true);
        assert.equal(sends, 1);
        await delivery.deliver('OTHER', 'WAR-1', send);
        assert.equal(sends, 2);
        console.log('Real MySQL: OK (schema, JSON, HWID preview/apply/rollback, concurrent receipts, restart and bot isolation)');
    } finally {
        if (connection) await connection.end();
        if (pool) await pool.end();
        await admin.query(`DROP DATABASE IF EXISTS \`${schema}\``);
        await admin.end();
    }
}
main().catch(error => { console.error(error.message); process.exitCode = 1; });
