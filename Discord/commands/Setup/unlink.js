const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('unlink')
        .setNameLocalizations({ tr: 'cikar', 'en-US': 'unlink' })
        .setDescription('Bir HWID baglantisini kaldirir')
        .setDescriptionLocalizations({ tr: 'Bir HWID bağlantısını kaldırır', 'en-US': 'Removes an HWID link' })
        .addStringOption(o => o.setName('hwid')
            .setDescription('Kaldirilacak HWID kodu')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const hwid = interaction.options.getString('hwid').trim().toUpperCase();

        const guild = await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $pull: { linkedHwids: hwid } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(
            `✅ **\`${hwid}\`** baglantisi kaldirildi.\n` +
            `Bagli HWID\'ler: ${guild.linkedHwids.map(h => `\`${h}\``).join(', ') || 'yok'}`
        );
    }
};
