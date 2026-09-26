const crypto = require('crypto');

module.exports = {
    PORT: process.env.PORT || 3000,

    // Ortam degiskeni yoksa eski sabitler kullanilir (uretimde .env onerilir)
    DISCORD_BOT_TOKEN: process.env.DISCORD_BOT_TOKEN || "MTU1MzQ2NTI0NjMzMTU2ODI4Mg.GHcX9C.LrO_i82qoGCDdVRF8HzJddCkUzbnybGiQOx5gU",

    // Veri katmani: yalnizca MySQL (Mongo katmani kaldirildi)
    DB: 'mysql',
    MYSQL: {
        host: process.env.MYSQL_HOST || '127.0.0.1',
        port: parseInt(process.env.MYSQL_PORT || '3306', 10), // dogrudan MySQL; ProxySQL eklenirse 6033
        user: process.env.MYSQL_USER || 'raksamp',
        password: process.env.MYSQL_PASSWORD || 'raksamp',
        database: process.env.MYSQL_DB || 'raksamp',
        waitForConnections: true,
        connectionLimit: 5
    },

    AES_SECRET_KEY: crypto.createHash('sha256').update('BURHAN_RAKSAMP_SECRET_2026').digest(),

    // Google Sheets (?gmembers rosters): service account JSON Discord/ klasorunde.
    // Tablo ID bos = service account yeni tablo olusturur + ownerEmail ile paylasir
    // (paylasilan tablo Drive'da gorunur). GS_OWNER_EMAIL = sahibin Google adresi.
    GOOGLE_SHEETS: {
        serviceAccountFile: process.env.GS_SERVICE_ACCOUNT_FILE || 'avian-computer-500410-j8-c8fb699de46a.json',
        spreadsheetId: process.env.GS_SPREADSHEET_ID || '1tfS4CF0usW7EdxmmJnnntpzr_yqB-fvphvjKv-aJ_Jc',
        ownerEmail: process.env.GS_OWNER_EMAIL || 'burhanuzun488@gmail.com'
    }
};
