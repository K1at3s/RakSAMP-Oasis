const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gstats')
        .setNameLocalizations({ tr: 'gistatistik', 'en-US': 'gstats' })
        .setDescription('Bagli RakSAMP istemcisinden grup istatistikleri ister')
        .setDescriptionLocalizations({ tr: 'Bağlı RakSAMP istemcisinden grup istatistikleri ister', 'en-US': 'Requests group stats from the linked RakSAMP client' })
        .addStringOption(o => o.setName('group_id').setDescription('Belirli bir grubun ID si (bos birakilirsa kendi grubun)').setRequired(false)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanin.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        const groupId = interaction.options.getString('group_id') || '';

        // Ilk mesaj yerine "dusunuyor..." goster; oyundan yanit gelince editReply ile sonuc atilir.
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = groupId ? `/gstats ${groupId}` : '/gstats';
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [GSTATS] Komut kuyruga alindi (group_id="${groupId}", requester=${interaction.id})`);
    }
};
