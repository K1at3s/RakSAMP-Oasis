const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

// Load local settings without overriding process-level environment values.
const localEnv = path.join(__dirname, '.env');
if (fs.existsSync(localEnv)) {
    for (const line of fs.readFileSync(localEnv, 'utf8').replace(/^\uFEFF/, '').split(/\r?\n/)) {
        const match = line.match(/^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$/);
        if (!match || process.env[match[1]] !== undefined) continue;
        let value = match[2];
        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) value = value.slice(1, -1);
        else value = value.replace(/\s+#.*$/, '').trim();
        process.env[match[1]] = value;
    }
}

function requiredSecret(name, minimumLength = 1) {
    const value = process.env[name];
    if (typeof value !== 'string' || value.length < minimumLength) {
        throw new Error(`Missing or invalid required environment variable: ${name}`);
    }
    return value;
}

const botToken = requiredSecret('DISCORD_BOT_TOKEN');
const aesSecret = requiredSecret('RAKSAMP_AES_SECRET', 32);
const corsOrigins = (process.env.RAKSAMP_CORS_ORIGINS || '').split(',').map(x => x.trim()).filter(Boolean);
if (corsOrigins.some(origin => !/^https?:\/\/[^\s/]+(?::\d+)?$/i.test(origin))) {
    throw new Error('RAKSAMP_CORS_ORIGINS must be a comma-separated list of origins');
}

module.exports = {
    PORT: Number.parseInt(process.env.PORT || '3000', 10),
    DISCORD_BOT_TOKEN: botToken,
    DB: 'mysql',
    MYSQL: {
        host: process.env.MYSQL_HOST || '127.0.0.1',
        port: Number.parseInt(process.env.MYSQL_PORT || '3306', 10),
        user: process.env.MYSQL_USER || 'raksamp',
        password: process.env.MYSQL_PASSWORD || '',
        database: process.env.MYSQL_DB || 'raksamp_oasis',
        waitForConnections: true,
        connectionLimit: 5
    },
    AES_SECRET_KEY: crypto.createHash('sha256').update(aesSecret, 'utf8').digest(),
    CORS_ORIGINS: corsOrigins,
    GOOGLE_SHEETS: {
        serviceAccountFile: process.env.GS_SERVICE_ACCOUNT_FILE || '',
        spreadsheetId: process.env.GS_SPREADSHEET_ID || '',
        ownerEmail: process.env.GS_OWNER_EMAIL || ''
    }
};
