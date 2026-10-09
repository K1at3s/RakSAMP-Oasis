const { SlashCommandBuilder } = require('discord.js');
const db = require('../../db');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { hasGroupPermission, getLinkedHwid } = require('../../utils/permissions');

// /gzt [on|off|time] — savas geri sayim duyurulari (oyun ici ?gzt ile ayni).
// on/off anahtari DB'ye client yazar; argumansiz cagrilirsa kayitli durum gosterilir.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('gzt')
        .setNameLocalizations({ tr: 'gzt', 'en-US': 'gzt' })
        .setDescription('Shows or sets the zone-war countdown announcements')
        .setDescriptionLocalizations({ tr: 'Zone savaşı geri sayım duyurularını gösterir/ayarlar', 'en-US': 'Shows or sets the zone-war countdown announcements' })
        .addStringOption(o => o
            .setName('mode')
            .setNameLocalizations({ tr: 'mod', 'en-US': 'mode' })
            .setDescription('on = enable, off = disable, time = remaining time')
            .setDescriptionLocalizations({ tr: 'on = aç, off = kapat, time = kalan süre', 'en-US': 'on = enable, off = disable, time = remaining time' })
            .setRequired(false)
            .addChoices(
                { name: 'on (aç)', value: 'on' },
                { name: 'off (kapat)', value: 'off' },
                { name: 'time (kalan süre)', value: 'time' }
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
                    'SELECT gzt_enabled, updated_at FROM bots WHERE group_name = ? ORDER BY updated_at DESC LIMIT 1',
                    [group]
                );
                if (rows && rows[0]) state = rows[0];
            }
            const stateText = state
                ? (isTr ? `Kayıtlı durum: **${state.gzt_enabled ? 'AÇIK' : 'KAPALI'}**` : `Stored state: **${state.gzt_enabled ? 'ON' : 'OFF'}**`)
                : (isTr ? 'Kayıtlı durum bulunamadı (grup adı öğrenilmemiş olabilir).' : 'No stored state (group name may not be learned yet).');
            return interaction.editReply({
                content: [`⏱️ ${stateText}`, isTr ? 'Aç/kapat: `/gzt mod:on|off|time`' : 'Toggle: `/gzt mode:on|off|time`'].join('\n')
            });
        }

        const hwid = getLinkedHwid(guild);
        if (!hwid) {
            return interaction.reply({ content: '❌ Bağlı RakSAMP istemcisi yok.', ephemeral: true });
        }

        queueCommand(hwid, { type: 'command', cmd: `?gzt ${mode}`, author: interaction.user.username });
        await interaction.reply({
            content: isTr
                ? `✅ \`?gzt ${mode}\` oyuna iletildi. Sonuç grup chatinde görünecek.`
                : `✅ \`?gzt ${mode}\` sent to the game. The result will appear in the group chat.`,
            ephemeral: true
        });
        console.log(`⏱️ [GZT] guild=${interaction.guildId} mode=${mode}`);
    }
};