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

module.exports = { getGuild, getGuildByHwid };
