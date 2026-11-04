// Discord tarafindan (slash komutlar) ve REST endpoint'lerden kullanilan
// ortak "kanala mesaj gonder" yardimcisi. client, server.js icinde set edilir.
let _client = null;

function setClient(client) {
    _client = client;
}

// guild + kanal cozumle, Discord'a embed/duz mesaj/dosya gonder.
// Kanal ayarlanmamissa null doner (istek basarili sayilir, sadece loglanir).
async function sendToGuildChannel(guild, field, { embed, content, files } = {}) {
    const channelId = guild ? guild[field] : null;
    if (!channelId) {
        console.log(`⚠️ ${field} kanali ayarlanmamis (guild=${guild ? guild.guildId : '?'}), mesaj atlaniyor.`);
        return null;
    }
    try {
        const channel = await _client.channels.fetch(channelId);
        if (!channel) {
            console.log(`⚠️ ${field} kanali bulunamadi (id=${channelId}).`);
            return null;
        }
        if (embed) await channel.send({ embeds: [embed], files });
        else if (files) await channel.send({ files });
        else if (content) await channel.send(content);
        return channelId;
    } catch (e) {
        // Silinmis kanal (Unknown Channel 10003/404): kaydi temizle ki her
        // gonderimde patlamasin; /setchannel veya /kurulum ile yeniden atanir.
        if (e && (e.code === 10003 || e.status === 404)) {
            try {
                const { Guild } = require('../models');
                const upd = {};
                upd[field] = null;
                if (guild && guild.guildId) await Guild.findOneAndUpdate({ guildId: guild.guildId }, upd);
                console.log(`🧹 ${field} temizlendi (kanal silinmis id=${channelId}, guild=${guild ? guild.guildId : '?'}). /setchannel ile yeniden ata.`);
            } catch (err2) {
                console.error(`Kanal temizleme hatasi (${field}):`, err2.message);
            }
            return null;
        }
        console.error(`Discord gonderim hatasi (${field}):`, e);
        return null;
    }
}


async function sendWarResult(guild, embed, marker, allowSend) {
    let channel;
    try {
        if (!guild?.zoneWarChannelId || !_client) throw new Error('War channel not configured');
        channel = await _client.channels.fetch(guild.zoneWarChannelId);
        if (!channel?.messages || !channel.send) throw new Error('War channel unavailable');
        const history = await channel.messages.fetch({ limit: 100 });
        if (history.some(message => message.author?.id === _client.user?.id && message.embeds.some(item => item.footer?.text === marker))) return channel.id;
    } catch (error) { error.definitelyNotSent = true; throw error; }
    if (!allowSend) return null;
    embed.setFooter({ text: marker });
    try { await channel.send({ embeds: [embed] }); return channel.id; }
    catch (error) {
        error.definitelyNotSent = error.status >= 400 && error.status < 500 && error.status !== 408;
        throw error;
    }
}
module.exports = { setClient, sendToGuildChannel, sendWarResult };
