const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { setPendingInteraction } = require('../../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('stats')
        .setNameLocalizations({ tr: 'istatistik', 'en-US': 'stats' })
        .setDescription('RakSAMP istatistikleri: bos birakilirsa kendi, ID verilirse o oyuncu')
        .setDescriptionLocalizations({ tr: 'RakSAMP istatistikleri: boş bırakılırsa kendi, ID verilirse o oyuncu', 'en-US': 'RakSAMP stats: blank for own, or a player ID' })
        .addIntegerOption(o => o.setName('id').setDescription('Oyuncu/hesap ID si (bos birakilirsa kendi istatistiklerin)').setRequired(false)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanin.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        const id = interaction.options.getInteger('id');

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = id ? `/stats ${id}` : '/stats';
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [STATS] Komut kuyruga alindi (id="${id ?? 'kendi'}", requester=${interaction.id})`);
    }
};
