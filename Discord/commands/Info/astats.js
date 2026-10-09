const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { setPendingInteraction } = require('../../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('astats')
        .setNameLocalizations({ tr: 'aistatistik', 'en-US': 'astats' })
        .setDescription('RakSAMP hesap istatistikleri (account ID gerekli)')
        .setDescriptionLocalizations({ tr: 'RakSAMP hesap istatistikleri (account ID gerekli)', 'en-US': 'RakSAMP account stats (account ID required)' })
        .addStringOption(o => o.setName('account_id').setDescription('Hesap (account) ID si').setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanin.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        const accountId = interaction.options.getString('account_id');

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = `/astats ${accountId}`;
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [ASTATS] Komut kuyruga alindi (account_id="${accountId}", requester=${interaction.id})`);
    }
};
