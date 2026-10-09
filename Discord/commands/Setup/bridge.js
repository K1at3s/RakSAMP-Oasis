const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;
const { hasGroupPermission } = require('../../utils/permissions');

// /kopru [on|off] — Discord <-> oyun grup chat koprusu (guilds.group_chat_enabled).
// Client bu degeri 2 sn'lik poll'dan uygular (GET /api/groupchat/poll -> enabled),
// yani ayar DB'de degisince bot oyunda da aninda ac/kapa olur.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('bridge')
        .setNameLocalizations({ tr: 'kopru', 'en-US': 'bridge' })
        .setDescription('Shows or sets the Discord <-> in-game group chat bridge')
        .setDescriptionLocalizations({ tr: 'Discord <-> oyun grup chat köprüsünü gösterir/açar-kapatır', 'en-US': 'Shows or sets the Discord <-> in-game group chat bridge' })
        .addStringOption(o => o
            .setName('durum')
            .setNameLocalizations({ tr: 'durum', 'en-US': 'state' })
            .setDescription('on/off (blank = show the current state)')
            .setDescriptionLocalizations({ tr: 'aç/kapat (boş = mevcut durumu gösterir)', 'en-US': 'on/off (blank = show the current state)' })
            .setRequired(false)
            .addChoices(
                { name: 'Aç (on)', value: 'on' },
                { name: 'Kapat (off)', value: 'off' }
            )),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!hasGroupPermission(interaction, guild)) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const isTr = guild ? guild.lang === 'tr' : true;
        const choice = interaction.options.getString('durum');

        if (!choice) {
            const enabled = guild ? !!guild.groupChatEnabled : false;
            const channelId = guild ? guild.groupChatChannelId : null;
            return interaction.reply({
                content: [
                    isTr ? `💬 Grup chat köprüsü: **${enabled ? 'AÇIK' : 'KAPALI'}**` : `💬 Group chat bridge: **${enabled ? 'ON' : 'OFF'}**`,
                    isTr ? `Kanal: ${channelId ? `<#${channelId}>` : 'ayarlı değil (`/kanalayarla group-chat <kanal>`)'}` : `Channel: ${channelId ? `<#${channelId}>` : 'not set (`/setchannel group-chat <channel>`)'}`,
                    isTr ? 'Aç/kapat: `/kopru durum:aç/kapat`' : 'Toggle: `/bridge state:on/off`'
                ].join('\n'),
                ephemeral: true
            });
        }

        const enabled = choice === 'on';
        const updated = await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $set: { groupChatEnabled: enabled } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        const lines = [enabled
            ? (isTr ? '✅ Grup chat köprüsü **AÇILDI** (client 2 sn içinde uygular).' : '✅ Group chat bridge **ENABLED** (the client applies it within 2s).')
            : (isTr ? '✅ Grup chat köprüsü **KAPATILDI** (client 2 sn içinde uygular).' : '✅ Group chat bridge **DISABLED** (the client applies it within 2s).')];

        if (enabled && !updated.groupChatChannelId) {
            lines.push(isTr
                ? '⚠️ Kanal ayarlı değil: `/kanalayarla group-chat <kanal>` (oyun mesajları kanala düşmez).'
                : '⚠️ No channel set: `/setchannel group-chat <channel>` (game messages are not posted).');
        }

        await interaction.reply({ content: lines.join('\n'), ephemeral: true });
        console.log(`🔗 [BRIDGE] guild=${interaction.guildId} -> ${enabled ? 'ON' : 'OFF'} (by ${interaction.user.tag})`);
    }
};