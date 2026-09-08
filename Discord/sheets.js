// Google Sheets yazici (?gmembers roster). Service account JSON config.GOOGLE_SHEETS'ten.
// Tablo verilmezse service account YENI tablo olusturur ve sahibinin Google
// hesabiyla (config GOOGLE_SHEETS.ownerEmail) paylasir. Grup -> tablo ID eslemesi
// spreadsheets.json dosyasinda tutulur (her yazimda ayni tabloya yazilir).
const { google } = require('googleapis');
const fs = require('fs');
const path = require('path');
const config = require('./config');

const STATE_FILE = path.join(__dirname, 'spreadsheets.json');

let authClient = null;

function getAuth() {
    if (authClient) return authClient;
    const file = config.GOOGLE_SHEETS.serviceAccountFile;
    if (!file || !fs.existsSync(path.join(__dirname, file))) {
        throw new Error(`Service account dosyasi yok: ${file}`);
    }
    const creds = JSON.parse(fs.readFileSync(path.join(__dirname, file), 'utf8'));
    authClient = new google.auth.JWT({
        email: creds.client_email,
        key: creds.private_key,
        scopes: [
            'https://www.googleapis.com/auth/spreadsheets',
            'https://www.googleapis.com/auth/drive.file' // create icin gerekli
        ]
    });
    return authClient;
}

function loadState() {
    try { return JSON.parse(fs.readFileSync(STATE_FILE, 'utf8')); }
    catch { return {}; }
}

function saveState(state) {
    fs.writeFileSync(STATE_FILE, JSON.stringify(state, null, 2));
}

// Verilen ID gecerli mi? (okuma testi) Degilse veya yoksa yeni tablo olustur.
async function resolveSpreadsheetId(sheets, spreadsheetId, group) {
    if (spreadsheetId) {
        try {
            await sheets.spreadsheets.values.get({ spreadsheetId, range: 'A1:A1' });
            return spreadsheetId;
        } catch { /* yok veya izinsiz -> olustur */ }
    }
    const state = loadState();
    if (state[group]) {
        try {
            await sheets.spreadsheets.values.get({ spreadsheetId: state[group], range: 'A1:A1' });
            return state[group];
        } catch { /* silinmis olabilir -> yeniden olustur */ }
    }
    const created = await sheets.spreadsheets.create({
        requestBody: {
            properties: { title: `${group} Roster` },
            sheets: [{ properties: { title: 'Members', gridProperties: { rowCount: 500, columnCount: 7 } } }]
        }
    });
    const newId = created.data.spreadsheetId;
    const owner = config.GOOGLE_SHEETS.ownerEmail;
    if (owner) {
        try {
            await sheets.spreadsheets.permissions.create({
                spreadsheetId: newId,
                requestBody: { role: 'writer', type: 'user', emailAddress: owner }
            });
        } catch (e) { console.error(`📊 Sahip paylasimi basarisiz (${owner}): ${e.message}`); }
    }
    state[group] = newId;
    saveState(state);
    console.log(`📊 Yeni tablo olusturuldu: https://docs.google.com/spreadsheets/d/${newId}`);
    return newId;
}

// Ornek tablo formatini birebir uygular (Eagle Industries Roster):
//   A1: "<Grup> Roster"          (A1:G1 birlesik, koyu mavi, kalin beyaz, ortali)
//   A2: "N members -- Updated ..." (A2:G2 birlesik, acik gri, ortali)
//   A3: # Name Account ID Rank Role VIP Last Active (koyu, kalin beyaz, ortali)
//   A4+: uyeler, satirlar alternatif renkli (acik mor / beyaz)
//   Ilk 3 satir donmus.
const COLORS = {
    titleBg:  { red: 0.12941177, green: 0.14901961, blue: 0.21960784 },
    metaBg:   { red: 0.8980392,  green: 0.8980392,  blue: 0.91764706 },
    metaText: { red: 0.34901962, green: 0.34901962, blue: 0.36862746 },
    headerBg: { red: 0.1764706,  green: 0.1882353,  blue: 0.23921569 },
    altBg:    { red: 0.8980392,  green: 0.8666667,  blue: 0.9764706 },
    white:    { red: 1, green: 1, blue: 1 }
};

async function applyRosterFormatting(sheets, spreadsheetId, dataRowCount) {
    const requests = [
        // Kolon genislikleri (ornek tablodan birebir: # 50, Name 170, Account ID 110,
        // Rank 150, Role 110, VIP 90, Last Active 120)
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 0, endIndex: 7 },
                properties: { pixelSize: 50 }, fields: 'pixelSize'
            }
        },
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 1, endIndex: 2 },
                properties: { pixelSize: 170 }, fields: 'pixelSize'
            }
        },
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 2, endIndex: 3 },
                properties: { pixelSize: 110 }, fields: 'pixelSize'
            }
        },
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 3, endIndex: 4 },
                properties: { pixelSize: 150 }, fields: 'pixelSize'
            }
        },
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 4, endIndex: 5 },
                properties: { pixelSize: 110 }, fields: 'pixelSize'
            }
        },
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 5, endIndex: 6 },
                properties: { pixelSize: 90 }, fields: 'pixelSize'
            }
        },
        {
            updateDimensionProperties: {
                range: { dimension: 'COLUMNS', startIndex: 6, endIndex: 7 },
                properties: { pixelSize: 120 }, fields: 'pixelSize'
            }
        },
        // Ilk 3 satiri dondur (basliklar hep gorunur)
        {
            updateSheetProperties: {
                properties: { gridProperties: { frozenRowCount: 3 } },
                fields: 'gridProperties.frozenRowCount'
            }
        },
        // A1:G1 ve A2:G2 birlesik
        { mergeCells: { range: { startRowIndex: 0, endRowIndex: 1, startColumnIndex: 0, endColumnIndex: 7 }, mergeType: 'MERGE_ALL' } },
        { mergeCells: { range: { startRowIndex: 1, endRowIndex: 2, startColumnIndex: 0, endColumnIndex: 7 }, mergeType: 'MERGE_ALL' } },
        // Baslik satiri: koyu mavi, 20pt kalin beyaz, ortali
        {
            repeatCell: {
                range: { startRowIndex: 0, endRowIndex: 1, startColumnIndex: 0, endColumnIndex: 7 },
                cell: { userEnteredFormat: { backgroundColor: COLORS.titleBg, textFormat: { bold: true, fontSize: 20, foregroundColor: COLORS.white }, horizontalAlignment: 'CENTER' } },
                fields: 'userEnteredFormat.backgroundColor,userEnteredFormat.textFormat.bold,userEnteredFormat.textFormat.fontSize,userEnteredFormat.textFormat.foregroundColor,userEnteredFormat.horizontalAlignment'
            }
        },
        // Meta satiri: acik gri, 10pt italik gri yazi, ortali
        {
            repeatCell: {
                range: { startRowIndex: 1, endRowIndex: 2, startColumnIndex: 0, endColumnIndex: 7 },
                cell: { userEnteredFormat: { backgroundColor: COLORS.metaBg, textFormat: { fontSize: 10, italic: true, foregroundColor: COLORS.metaText }, horizontalAlignment: 'CENTER' } },
                fields: 'userEnteredFormat.backgroundColor,userEnteredFormat.textFormat.fontSize,userEnteredFormat.textFormat.italic,userEnteredFormat.textFormat.foregroundColor,userEnteredFormat.horizontalAlignment'
            }
        },
        // Kolon basliklari: koyu, 12pt kalin beyaz, ortali
        {
            repeatCell: {
                range: { startRowIndex: 2, endRowIndex: 3, startColumnIndex: 0, endColumnIndex: 7 },
                cell: { userEnteredFormat: { backgroundColor: COLORS.headerBg, textFormat: { bold: true, fontSize: 12, foregroundColor: COLORS.white }, horizontalAlignment: 'CENTER' } },
                fields: 'userEnteredFormat.backgroundColor,userEnteredFormat.textFormat.bold,userEnteredFormat.textFormat.fontSize,userEnteredFormat.textFormat.foregroundColor,userEnteredFormat.horizontalAlignment'
            }
        }
    ];
    // Veri satirlari: cift satirlar acik mor, tek satirlar beyaz; koyu metin;
    // Role (4) ve VIP (5) kolonlari kalin + ortali (ornek tablodaki gibi)
    const dataText = { red: 0.11764706, green: 0.12941177, blue: 0.14901961 };
    for (let i = 0; i < dataRowCount; i++) {
        requests.push({
            repeatCell: {
                range: { startRowIndex: 3 + i, endRowIndex: 4 + i, startColumnIndex: 0, endColumnIndex: 7 },
                cell: { userEnteredFormat: { backgroundColor: (i % 2 === 0) ? COLORS.altBg : COLORS.white, textFormat: { foregroundColor: dataText } } },
                fields: 'userEnteredFormat.backgroundColor,userEnteredFormat.textFormat.foregroundColor'
            }
        });
        requests.push({
            repeatCell: {
                range: { startRowIndex: 3 + i, endRowIndex: 4 + i, startColumnIndex: 4, endColumnIndex: 6 },
                cell: { userEnteredFormat: { textFormat: { bold: true }, horizontalAlignment: 'CENTER' } },
                fields: 'userEnteredFormat.textFormat.bold,userEnteredFormat.horizontalAlignment'
            }
        });
    }
    await sheets.spreadsheets.batchUpdate({ spreadsheetId, requestBody: { requests } });
}

async function writeRoster(spreadsheetId, group, rows) {
    const sheets = google.sheets({ version: 'v4', auth: getAuth() });
    const id = await resolveSpreadsheetId(sheets, spreadsheetId, group);

    // Yapiyi ornek tabloyla ayni yap: baslik "<Grup> - Members List", tab "Members"
    // (1000x26), ek "Summary" sekmesi (10x2). (Ornek: 'Eagle Industries - Members List')
    {
        const props = await sheets.spreadsheets.get({ spreadsheetId: id, fields: 'properties.title,sheets.properties(sheetId,title,gridProperties)' });
        const requests = [];
        if ((props.data.properties.title || '') !== `${group} - Members List`) {
            requests.push({ updateSpreadsheetProperties: { properties: { title: `${group} - Members List` }, fields: 'title' } });
        }
        const first = props.data.sheets[0].properties;
        if (first.title !== 'Members' || first.gridProperties.rowCount !== 1000 || first.gridProperties.columnCount !== 26) {
            requests.push({
                updateSheetProperties: {
                    properties: { sheetId: first.sheetId, title: 'Members', gridProperties: { rowCount: 1000, columnCount: 26 } },
                    fields: 'title,gridProperties.rowCount,gridProperties.columnCount'
                }
            });
        }
        const hasSummary = props.data.sheets.some(s => s.properties.title === 'Summary');
        if (!hasSummary) {
            requests.push({ addSheet: { properties: { title: 'Summary', gridProperties: { rowCount: 10, columnCount: 2 } } } });
        }
        if (requests.length) await sheets.spreadsheets.batchUpdate({ spreadsheetId: id, requestBody: { requests } });
    }

    const d = new Date();
    const m = /^(\w{3}), (\d{2}) (\w{3}) (\d{4}) (\d{2}):(\d{2}):\d{2} (?:UTC|GMT)$/.exec(d.toUTCString());
    const stamp = m ? `${m[2]} ${m[3]} ${m[4]}, ${m[5]}:${m[6]} UTC` : d.toISOString();

    const header = ['#', 'Name', 'Account ID', 'Rank', 'Role', 'VIP', 'Last Active'];
    // For /gmembers updates: preserve existing accountIds, delete missing users
    let existingMap = new Map();
    let isUpdate = false;
    try {
        const existing = await sheets.spreadsheets.values.get({ spreadsheetId: id, range: 'A4:G500' });
        const existingValues = existing.data.values || [];
        if (existingValues.length > 0) {
            isUpdate = true;
            for (const row of existingValues) {
                const name = (row[1] || '').trim();
                if (name) existingMap.set(name.toLowerCase(), { accountId: row[2] || '' });
            }
        }
    } catch (e) { /* new sheet */ }
    let finalRows = rows;
    if (isUpdate) {
        // Preserve accountIds for existing users, delete users not in new list
        const newNames = new Set(rows.map(r => (r.name || '').toLowerCase()));
        finalRows = [];
        for (const r of rows) {
            const key = (r.name || '').toLowerCase();
            const existing = existingMap.get(key);
            let accountId = r.accountId || '';
            if (existing && existing.accountId && !accountId) accountId = existing.accountId;
            finalRows.push({ ...r, accountId });
        }
        // Users in existing but not in new are automatically deleted (not included)
    }
    const values = [[`${group} Roster`], [`${finalRows.length} members -- Updated ${stamp}`], header];
    finalRows.forEach((r, i) => {
        values.push([
            String(i + 1), r.name || '', r.accountId || '', r.rank || '', r.role || '',
            r.vip || '', r.lastActive || ''
        ]);
    });

    // Eski veriyi temizle (satir sayisi azaldiysa artik kalmasin) sonra yaz.
    await sheets.spreadsheets.values.clear({ spreadsheetId: id, range: 'A1:Z500' });
    await sheets.spreadsheets.values.update({
        spreadsheetId: id,
        range: 'A1',
        valueInputOption: 'USER_ENTERED',
        requestBody: { values }
    });
    await applyRosterFormatting(sheets, id, finalRows.length);
    return { rows: finalRows.length, spreadsheetId: id };
}

module.exports = { writeRoster };