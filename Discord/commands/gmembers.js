const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gmembers')
        .setNameLocalizations({ tr: 'gmembers', 'en-US': 'gmembers' })
        .setDescription('Uye listesi + account ID + VIP toplayip Google Sheets roster tablosuna yazar (harvest)')
        .setDescriptionLocalizations({ tr: 'Üye listesi + account ID + VIP toplayıp Google Sheets roster tablosuna yazar (harvest)', 'en-US': 'Collects member list + account IDs + VIP and writes to the Google Sheets roster (harvest)' })
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanın.',
                ephemeral: true
            });
        }

        const L = (tr, en) => (guild.lang === 'tr' ? tr : en);
        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'gmembers', requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);

        await interaction.reply({
            content: `✅ ${L('GMembers harvest başlatıldı. Bitince tablo linki bu kanala gelecek.', 'GMembers harvest started. The sheet link will be posted in this channel when done.')}`,
            ephemeral: true
        });
    }
};