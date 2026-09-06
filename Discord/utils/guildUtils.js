const Guild = require('../models').Guild;

// Sunucu kaydini getir, yoksa varsayilan degerlerle olustur
async function getGuild(guildId) {
    if (!guildId) return null;
    let guild = await Guild.findOne({ guildId });
    if (!guild) {
        guild = await Guild.create({ guildId });
    }
    return guild;
}

// HWID ile bagli oldugu sunucuyu bul
async function getGuildByHwid(hwid) {
    if (!hwid) return null;
    return await Guild.findOne({ linkedHwids: hwid });
}

// Kanal ID'sini veritabanindan cek, ayarlanmamissa null
async function getChannelId(guildId, type) {
    const guild = await getGuild(guildId);
    if (!guild) return null;
    return guild[`${type}ChannelId`] || null;
}

module.exports = { getGuild, getGuildByHwid, getChannelId };
