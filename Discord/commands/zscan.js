const { SlashCommandBuilder } = require('discord.js');
const db = require('../db');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { hasGroupPermission, getLinkedHwid } = require('../utils/permissions');

// /zscan [on|off|sn] — otomatik zone tarama anahtari (oyun ici ?zscan ile ayni).
// Argumansiz cagrilirsa bots tablosundaki kayitli durum gosterilir.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('zscan')
        .setNameLocalizations({ tr: 'zscan', 'en-US': 'zscan' })
        .setDescription('Shows or sets the automatic zone scan (on / off / one-shot verify)')
        .setDescriptionLocalizations({ tr: 'Otomatik zone taramayı gösterir/ayarlar (aç / kapat / tek seferlik tam doğrulama)', 'en-US': 'Shows or sets the automatic zone scan (on / off / one-shot verify)' })
        .addStringOption(o => o
            .setName('mode')
            .setNameLocalizations({ tr: 'mod', 'en-US': 'mode' })
            .setDescription('on = start, off = stop, sn = one-shot full verify')
            .setDescriptionLocalizations({ tr: 'on = başlat, off = durdur, sn = tek seferlik tam doğrulama', 'en-US': 'on = start, off = stop, sn = one-shot full verify' })
            .setRequired(false)
            .addChoices(
                { name: 'on (başlat)', value: 'on' },
                { name: 'off (durdur)', value: 'off' },
                { name: 'sn (tam doğrulama)', value: 'sn' }
            )),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!hasGroupPermission(interaction, guild)) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const isTr = guild ? guild.lang === 'tr' : true;
        const mode = interaction.options.getString('mode');

        if (!mode) {
            await interaction.deferReply({ ephemeral: true });
            const group = guild && guild.groupName ? String(guild.groupName).trim() : '';
            let state = null;
            if (group) {
                const rows = await db.execute(
                    'SELECT zscan_enabled, updated_at FROM bots WHERE group_name = ? ORDER BY updated_at DESC LIMIT 1',
                    [group]
                );
                if (rows && rows[0]) state = rows[0];
            }
            const stateText = state
                ? (isTr ? `Kayıtlı durum: **${state.zscan_enabled ? 'AÇIK' : 'KAPALI'}**` : `Stored state: **${state.zscan_enabled ? 'ON' : 'OFF'}**`)
                : (isTr ? 'Kayıtlı durum bulunamadı (grup adı öğrenilmemiş olabilir).' : 'No stored state (group name may not be learned yet).');
            return interaction.editReply({
                content: [`🔍 ${stateText}`, isTr ? 'Aç/kapat: `/zscan mod:on|off|sn`' : 'Toggle: `/zscan mode:on\|off\|sn`'].join('\n')
            });
        }

        const hwid = getLinkedHwid(guild);
        if (!hwid) {
            return interaction.reply({ content: isTr ? '❌ Bağlı RakSAMP istemcisi yok.' : ' No linked RakSAMP client.', ephemeral: true });
        }

        queueCommand(hwid, { type: 'command', cmd: `?zscan ${mode}`, author: interaction.user.username });
        await interaction.reply({
            content: isTr
                ? `✅ \`?zscan ${mode}\` oyuna iletildi. Sonuç grup chatinde görünecek.`
                : `✅ \`?zscan ${mode}\` sent to the game. The result will appear in the group chat.`,
            ephemeral: true
        });
        console.log(`🔍 [ZSCAN] guild=${interaction.guildId} mode=${mode}`);
    }
};