// Veri katman─▒ k├Âpr├╝s├╝: yaln─▒zca MySQL/ProxySQL (Mongo katman─▒ kald─▒r─▒ld─▒).
// mongoose-uyumlu minimal API: findOne / create / findOneAndUpdate( $set|$addToSet|$pull ) + instance.save()
// Kapsam: yaln─▒zca kodun 8 slash komut dosyas─▒ + guildUtils + server.js'te KULLANDI─SI ┼sekiller.
const config = require('./config');

let pool = null;
const registry = {};

function esc(s) { return String(s).replace(/[\\%']/g, '\\$&'); }

// def = { table, pkey, fields: { prop: {col, type:'text'|'int'|'json', pk?:bool, def?} } }
function makeModel(table, def) {
    def.table = table; // init() CREATE TABLE icin; eksikti -> "undefined" tablosu olusuyordu
    registry[table] = def;

    const setSql = def.fields;

    function decode(row) {
        const inst = {};
        for (const [prop, c] of Object.entries(setSql)) {
            let v = row[c.col];
            if (c.type === 'json') v = (v && v !== '') ? JSON.parse(v) : [];
            else if (c.type === 'int') v = !!v;
            inst[prop] = v;
        }
        inst.__exists = true;
        Object.defineProperty(inst, 'save', { value: () => saveInst(inst), enumerable: false });
        return inst;
    }

    function defaults(doc) {
        const inst = {};
        for (const [prop, c] of Object.entries(setSql)) {
            inst[prop] = typeof c.def === 'function' ? c.def() : (c.def !== undefined ? c.def : null);
        }
        Object.assign(inst, doc);
        inst.__exists = false;
        for (const [prop, c] of Object.entries(setSql)) {
            if (c.type === 'json' && inst[prop] === null) inst[prop] = [];
        }
        Object.defineProperty(inst, 'save', { value: () => saveInst(inst), enumerable: false });
        return inst;
    }

    async function saveInst(inst) {
        const p = ensurePool();
        const pk = setSql[def.pkey];
        const cols = Object.entries(setSql);
        if (inst.__exists) {
            const sets = [];
            const vals = [];
            for (const [prop, c] of cols) {
                if (c.pk) continue;
                let v = inst[prop];
                if (c.type === 'json') v = JSON.stringify(v || []);
                else if (c.type === 'int') v = v ? 1 : 0;
                sets.push(`${c.col}=?`); vals.push(v);
            }
            const where = Object.entries(setSql).filter(([, c]) => c.pk).map(([, c]) => `${c.col}=?`);
            vals.push(inst[def.pkey]);
            await p.execute(`UPDATE ${table} SET ${sets.join(', ')} WHERE ${where.join(' AND ')}`, vals);
        } else {
            const names = cols.map(([, c]) => c.col);
            const vals = cols.map(([prop, c]) => {
                let v = inst[prop];
                if (c.type === 'json') v = JSON.stringify(v || []);
                else if (c.type === 'int') v = v ? 1 : 0;
                return (v === undefined || v === null) ? (typeof c.def === 'function' ? c.def() : c.def ?? null) : v;
            });
            await p.execute(
                `INSERT INTO ${table} (${names.join(', ')}) VALUES (${names.map(() => '?').join(', ')})`,
                vals
            );
            inst.__exists = true;
        }
        return inst;
    }

    function whereOf(filter) {
        const parts = [];
        const vals = [];
        for (const [prop, val] of Object.entries(filter || {})) {
            const c = setSql[prop];
            if (!c) continue;
            if (c.type === 'json') { parts.push(`${c.col} LIKE ?`); vals.push(`%"${esc(val)}"%`); }
            else { parts.push(`${c.col}=?`); vals.push(c.type === 'int' ? (val ? 1 : 0) : val); }
        }
        return { sql: parts.length ? `WHERE ${parts.join(' AND ')}` : '', vals };
    }

    return {
        async findOne(filter) {
            const p = ensurePool();
            const { sql, vals } = whereOf(filter);
            const [rows] = await p.execute(`SELECT * FROM ${table} ${sql} LIMIT 1`, vals);
            return rows.length ? decode(rows[0]) : null;
        },
        async create(doc) {
            const inst = defaults(doc);
            await inst.save();
            return inst;
        },
        async findOneAndUpdate(filter, update, opts = {}) {
            let inst = await this.findOne(filter);
            if (!inst) {
                if (opts.upsert === false) return null;
                inst = defaults({ ...filter });
                if (opts.setDefaultsOnInsert !== false && opts.setDefaultsOnInsert !== true) {}
            }
            for (const [op, fields] of Object.entries(update || {})) {
                // mongoose uyumlulu─su: operat├Ârs├╝z bare field ({ isApproved: true }) = $set
                if (!op.startsWith('$')) {
                    for (const [k, v] of Object.entries(update)) inst[k] = v;
                    break;
                }
                if (op === '$set') for (const [k, v] of Object.entries(fields)) inst[k] = v;
                else if (op === '$addToSet') for (const [k, v] of Object.entries(fields)) {
                    inst[k] = Array.isArray(inst[k]) && inst[k].includes(v) ? inst[k] : [...(inst[k] || []), v];
                }
                else if (op === '$pull') for (const [k, v] of Object.entries(fields)) {
                    inst[k] = (inst[k] || []).filter(x => x !== v);
                }
                else throw new Error(`db.js: desteklenmeyen operator ${op}`);
            }
            await inst.save();
            return inst;
        }
    };
}

function ensurePool() {
    if (pool) return pool;
    const mysql = require('mysql2/promise');
    pool = mysql.createPool(config.MYSQL);
    return pool;
}

async function init() {
    const p = ensurePool();
    // ┼sema bootstrap'─▒: bo┼s DB'de ilk a├g─▒l─▒┼sta tablolar─▒ kurar (schema.sql ile ayn─▒)
    for (const def of Object.values(registry)) {
        const cols = Object.entries(def.fields).map(([, c]) => {
            const t = c.type === 'json' ? 'TEXT' : c.type === 'int' ? 'TINYINT' : 'VARCHAR(255)';
            return `${c.col} ${t} ${c.pk ? 'PRIMARY KEY' : 'NULL'} ${c.type === 'int' && c.def !== undefined ? `DEFAULT ${c.def ? 1 : 0}` : ''}`;
        });
        await p.execute(`CREATE TABLE IF NOT EXISTS ${def.table} (${cols.join(', ')})`);
    }
    // client push tablolar─▒ (oyuncu cache + grup renkleri)
    await p.execute(`CREATE TABLE IF NOT EXISTS players (
        id INT PRIMARY KEY,
        name VARCHAR(64) NULL,
        packetloss DOUBLE NULL,
        fps INT NULL,
        ping INT NULL,
        updated_at DATETIME NULL
    )`);
    // Kalici zone->sahip haritasi gangs.tablosundaki zones JSON alanindadir
    // (gruba gore TEK satir). Ayri zones tablosu YOK ÔCo tek dogru kaynak gangs.
    // Ogrenilen grup adi + kisa tag haritasi (gruba gore TEK satir; zone kaydi yok).
    // 'groups' MySQL reserved word oldugu icin tablo adi 'gangs'.
    await p.execute(`CREATE TABLE IF NOT EXISTS gangs (
        group_name VARCHAR(96) PRIMARY KEY,
        tag VARCHAR(32) NULL,
        zones TEXT NULL,
        updated_at DATETIME NULL
    )`);
    await p.execute(`ALTER TABLE gangs ADD COLUMN zones TEXT NULL`).catch(() => {});
    // Her botun XML'deki ismiyle (name) eslestigi satir; botun verileri JSON data'da.
    // zscan_enabled + zgrp_enabled + group_name: ?zscan / ?zgrp on/off tercihi botun
    // oyun ici grubuna gore saklanir.
    await p.execute(`CREATE TABLE IF NOT EXISTS bots (
        name VARCHAR(64) PRIMARY KEY,
        group_name VARCHAR(255) NULL,
        zscan_enabled TINYINT NOT NULL DEFAULT 0,
        gzt_enabled TINYINT NOT NULL DEFAULT 1,
        zgrp_enabled TINYINT NOT NULL DEFAULT 1,
        data TEXT NULL,
        updated_at DATETIME NULL
    )`);
    await p.execute(`ALTER TABLE bots ADD COLUMN group_name VARCHAR(255) NULL`).catch(() => {});
    await p.execute(`ALTER TABLE bots ADD COLUMN zscan_enabled TINYINT NOT NULL DEFAULT 0`).catch(() => {});
    await p.execute(`ALTER TABLE bots ADD COLUMN gzt_enabled TINYINT NOT NULL DEFAULT 1`).catch(() => {});
    await p.execute(`ALTER TABLE bots ADD COLUMN zgrp_enabled TINYINT NOT NULL DEFAULT 1`).catch(() => {});
    await p.execute(`ALTER TABLE guilds ADD COLUMN allowed_role_id VARCHAR(255) NULL`).catch(() => {});
    // Grup log (?post) + bot komut kanallari (yeni) — mevcut tabloya kolon ekle
    await p.execute(`ALTER TABLE guilds ADD COLUMN group_log_channel_id VARCHAR(255) NULL`).catch(() => {});
    await p.execute(`ALTER TABLE guilds ADD COLUMN bot_commands_channel_id VARCHAR(255) NULL`).catch(() => {});
    // Welcome Discord kanali kaldirildi (welcome oyun ici kalir) — eski kolon DB'den duser.
    await p.execute(`ALTER TABLE guilds DROP COLUMN welcome_channel_id`).catch(() => {});
    // Index se├gici ekleme yok
    // Admin loglari: oyun ici "ADMIN: X banned/muted/kicked/disarmed Y(id) ..." yakalamasi
    await p.execute(`CREATE TABLE IF NOT EXISTS admin_logs (
        id INT AUTO_INCREMENT PRIMARY KEY,
        admin_name VARCHAR(64) NULL,
        target_name VARCHAR(64) NULL,
        target_id BIGINT NULL,
        action VARCHAR(16) NULL,
        mode VARCHAR(64) NULL,
        duration_min INT NULL,
        reason VARCHAR(255) NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    )`);
    // Kisi bazli hos geldin mesajlari (B secenegi): hwid + hedef oyuncu ismi -> ozel mesaj
    await p.execute(`CREATE TABLE IF NOT EXISTS welcome_per_user (
        id INT AUTO_INCREMENT PRIMARY KEY,
        hwid VARCHAR(255) NOT NULL,
        target_name VARCHAR(64) NOT NULL,
        custom_message VARCHAR(255) NOT NULL,
        updated_at DATETIME NULL,
        UNIQUE KEY uq_hwid_target (hwid, target_name)
    )`);
    // Grup rol degisikligi loglari: "(Ad)(accid) changed the rank of Hedef(accid) ..."
    await p.execute(`CREATE TABLE IF NOT EXISTS group_logs (
        id INT AUTO_INCREMENT PRIMARY KEY,
        actor_name VARCHAR(64) NULL,
        actor_acc BIGINT NULL,
        target_name VARCHAR(64) NULL,
        target_acc BIGINT NULL,
        old_rank VARCHAR(64) NULL,
        new_rank VARCHAR(64) NULL,
        reason VARCHAR(255) NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    )`);
    // Roster Discord mesaj kaydi (?gmembers bitiminde kanala atilan/duzenlenen link mesaji)
    await p.execute(`CREATE TABLE IF NOT EXISTS roster_messages (
        guild_id VARCHAR(255) PRIMARY KEY,
        channel_id VARCHAR(255) NULL,
        message_id VARCHAR(255) NULL,
        spreadsheet_id VARCHAR(255) NULL,
        member_count INT NULL,
        vip_count INT NULL,
        updated_at DATETIME NULL
    )`);
    return p;
}

// Ham SQL: server.js route'lar─▒ do─srudan kullan─▒r.
async function execute(sql, vals) {
    const [rows] = await ensurePool().execute(sql, vals);
    return rows;
}

// Batch INSERT ... VALUES ? i├gin (mysql2'de bu ┼sekil yaln─▒zca query()'de ├gal─▒┼s─▒r)
async function query(sql, vals) {
    const [rows] = await ensurePool().query(sql, vals);
    return rows;
}

module.exports = { init, model: makeModel, execute, query };
