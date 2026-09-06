const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('link')
        .setNameLocalizations({ tr: 'bagla', 'en-US': 'link' })
        .setDescription('Bu sunucuyu RakSAMP HWID\'nize baglar')
        .setDescriptionLocalizations({ tr: "Bu sunucuyu RakSAMP HWID'nize bağlar", 'en-US': "Links this server to your RakSAMP HWID" })
        .addStringOption(o => o.setName('hwid')
            .setDescription('RakSAMP\'in gosterdigi HWID kodu (orn. BURHAN-XXXX)')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const hwid = interaction.options.getString('hwid').trim().toUpperCase();
        if (!hwid) {
            return interaction.reply({ content: '❌ HWID bos olamaz.', ephemeral: true });
        }

        const guild = await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $addToSet: { linkedHwids: hwid } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(
            `✅ Bu sunucu **\`${hwid}\`** HWID\'li RakSAMP istemcisine baglandi.\n` +
            `Bagli HWID\'ler: ${guild.linkedHwids.map(h => `\`${h}\``).join(', ') || 'yok'}`
        );
    }
};
