const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');
const { queueCommand } = require('../utils/commandOutbox');

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

        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'gmembers' });

        await interaction.reply(`✅ GMembers harvest baslatildi. **\`${hwid}\`** istemcisi uye listesi + /astats (account ID) + /gvips (VIP) toplayip Google Sheets'e yazacak. Tamamlaninca tabloyu gunceller.`);
    }
};