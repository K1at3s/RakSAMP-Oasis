const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const Guild = require('../models').Guild;

// Komut listesinin TEK kaynagi (embed alanlari). TR + EN.
// /yardim paylas:true -> liste, /kurulum'un actigi bot-komut kanalina da birakilir.
function buildEmbed(isTr) {
    const field = (name, lines) => ({ name, value: lines.join('\n'), inline: false });
    return new EmbedBuilder()
        .setTitle(isTr ? '🤖 RakSAMP Bot Komutlari' : '🤖 RakSAMP Bot Commands')
        .setColor(0x00AEFF)
        .addFields(
            field(isTr ? '👥 Grup Yonetimi' : '👥 Group Management', [
                '`/grole [name] [role]` — ' + (isTr ? 'uye rol degistir (Member/Co-Leader)' : 'change a member role (Member/Co-Leader)'),
                '`/grank [name] [rank]` — ' + (isTr ? 'uye rank degistir (bos = menuden sec)' : 'change a member rank (blank = pick from menu)'),
                '`/gkick [name] [reason]` — ' + (isTr ? 'uye kickle (bos = menuden sec)' : 'kick a member (blank = pick from menu)'),
                '`/ginv <id>` — ' + (isTr ? 'oyuncu davet et (/ginvite)' : 'invite a player (/ginvite)'),
                '`/gmembers` — ' + (isTr ? 'uye listesi + VIP harvest → Google Sheets roster' : 'member list + VIP harvest → Google Sheets roster')
            ]),
            field(isTr ? '📊 Bilgi / Sorgu' : '📊 Info / Lookup', [
                '`/stats [id]` — ' + (isTr ? 'grup/oyuncu istatistigi (bos = kendi)' : 'group/player stats (blank = own)'),
                '`/astats <account_id>` — ' + (isTr ? 'hesap istatistigi' : 'account stats'),
                '`/gstats [group_id]` — ' + (isTr ? 'grup istatistigi (gstats kanalina da duser)' : 'group stats (also posted to the gstats channel)'),
                '`/admins` — ' + (isTr ? 'online admin listesi' : 'online admin list'),
                '`/oyuncu <isim|id>` — ' + (isTr ? 'oyuncu cache (packetloss/FPS/ping)' : 'player cache (packetloss/FPS/ping)'),
                '`/bolge <grup|tag>` — ' + (isTr ? 'grubun tag + sahip oldugu zone listesi' : 'group tag + owned zones'),
                '`/botdurum` — ' + (isTr ? 'bot kaydi: grup adi + zscan/gzt/zgrp durumu' : 'bot record: group name + zscan/gzt/zgrp state'),
                '`/alog <oyuncu|id>` — ' + (isTr ? 'admin ceza gecmisi (ban/mute/kick)' : 'admin punishment history'),
                '`/glog <oyuncu|id>` — ' + (isTr ? 'grup rol/rank degisiklik gecmisi' : 'group role/rank change history')
            ]),            field(isTr ? '✉️ Mesaj & Cagri' : '✉️ Message & Call', [
                '`/send <komut>` — ' + (isTr ? 'oyun icine serbest komut/mesaj' : 'free-form command/message to the game'),
                '`/pm <oyuncu> <mesaj>` — ' + (isTr ? 'oyuncuya ozel mesaj (yanit burada doner)' : 'private message to a player (reply shows here)'),
                '`/duyuru <mesaj>` — ' + (isTr ? 'grup chatine duyuru at (oyun ici)' : 'broadcast to the group chat (in-game)'),
                '`/backup [grup] [mesaj]` — ' + (isTr ? 'acil destek cagrisi (@here)' : 'urgent backup call (@here)'),
                '`/zonewar ...` — ' + (isTr ? 'manuel ZONE WAR bildirimi' : 'manual ZONE WAR notification')
            ]),
            field(isTr ? '⚙️ Ayarlar' : '⚙️ Settings', [
                '`/dil <tr|en>` — ' + (isTr ? 'bot dili (duyurular + embedler)' : 'bot language (announcements + embeds)'),
                '`/kopru [on|off]` — ' + (isTr ? 'Discord grup chat koprusu (bos = durumu goster)' : 'Discord group-chat bridge (blank = show state)'),
                '`/zscan [on|off|sn]` — ' + (isTr ? 'otomatik zone tarama (sn = tek seferlik tam dogrulama)' : 'automatic zone scan (sn = one-shot full verify)'),
                '`/gzt [on|off|time]` — ' + (isTr ? 'savas geri sayim duyurulari' : 'war countdown announcements'),
                '`/gname` — ' + (isTr ? 'grup adini yeniden ogren' : 're-learn the group name'),
                '`/setallowedrole [rol]` — ' + (isTr ? 'grup komutlari icin izinli rol (bos = herkes)' : 'allowed role for group commands (blank = everyone)'),
                '`/setchannel <tur> <kanal>` — ' + (isTr ? 'kanal atamasi' : 'channel assignment'),
                '`/setup [kapat]` — ' + (isTr ? 'kanallari olustur / sil' : 'create / delete channels'),
                '`/link <hwid>` — ' + (isTr ? 'HWID bagla' : 'link a HWID') + ' | `/unlink <hwid>` — ' + (isTr ? 'kaldir' : 'unlink')
            ]),
            field(isTr ? '🎮 Oyun Ici (grup chat)' : '\uD83C\uDFAE In-Game (group chat)', [
                '`?help` — ' + (isTr ? 'oyun ici komut listesi' : 'in-game command list'),
                '`?w` — ' + (isTr ? 'war help (Discord @here, 3 dk cooldown)' : 'war help (Discord @here, 3 min cooldown)'),
                '`?ping` `?pl [id]` `?gst` `?az` — ' + (isTr ? 'ping / packetloss / zone / bos zone' : 'ping / packetloss / zone / free zone'),
                '`?zscan` `?gzt` `?zgo` `?zf` `?zsnip` `?za` — ' + (isTr ? 'tarama ve zone savasi' : 'scan and zone war'),
                '`?greet` `?welcomemsg` `?gm` `?discordchat` `?guildid` — ' + (isTr ? 'ayarlar' : 'settings'),
                '`!<mesaj>` — ' + (isTr ? 'grup chatinden Discord kanalina mesaj' : 'message from the group chat to Discord')
            ])
        )
        .setFooter({ text: isTr ? 'Grup komutlari icin izinli rol: /setallowedrole' : 'Allowed role for group commands: /setallowedrole' })
        .setTimestamp();
}

module.exports = {
    data: new SlashCommandBuilder()
        .setName('help')
        .setNameLocalizations({ tr: 'yardim', 'en-US': 'help' })
        .setDescription('Shows the command list')
        .setDescriptionLocalizations({ tr: 'Komut listesini gösterir', 'en-US': 'Shows the command list' })
        .addBooleanOption(o => o
            .setName('share')
            .setNameLocalizations({ tr: 'paylas', 'en-US': 'share' })
            .setDescription('Also post the list to the bot-commands channel (default: no)')
            .setDescriptionLocalizations({ tr: 'Listeyi bot-komut kanalına da gönder (varsayılan: hayır)', 'en-US': 'Also post the list to the bot-commands channel (default: no)' })
            .setRequired(false)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const isTr = guild ? guild.lang === 'tr' : true;
        const embed = buildEmbed(isTr);
        const share = interaction.options.getBoolean('share') || false;

        if (share) {
            const channelId = guild ? guild.botCommandsChannelId : null;
            if (!channelId) {
                return interaction.reply({
                    content: isTr
                        ? '⚠️ Bot komut kanalı ayarlı değil. `/kanalayarla bot-commands <kanal>` ile ayarla veya `/kurulum` çalıştır.'
                        : '⚠️ No bot-commands channel configured. Set it with `/setchannel bot-commands <channel>` or run `/setup`.',
                    ephemeral: true
                });
            }
            const channel = await interaction.client.channels.fetch(channelId).catch(() => null);
            if (!channel || !channel.send) {
                return interaction.reply({ content: isTr ? '❌ Bot komut kanalı bulunamadı.' : '❌ Bot-commands channel not found.', ephemeral: true });
            }
            await channel.send({ embeds: [embed] });
            return interaction.reply({
                content: isTr ? '✅ Komut listesi kanala gönderildi: <#' + channel.id + '>' : '✅ Command list posted to <#' + channel.id + '>.',
                ephemeral: true
            });
        }

        await interaction.reply({ embeds: [embed], ephemeral: true });
    }
};