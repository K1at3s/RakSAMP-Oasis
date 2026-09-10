const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('ginv')
        .setDescription('Gruba oyuncu davet et (/ginvite id)')
        .setDescriptionLocalizations({ tr: 'Gruba oyuncu davet et (/ginvite id)', 'en-US': 'Invite a player to the group (/ginvite id)' })
        .addIntegerOption(o => o.setName('id').setDescription('Oyuncu ID').setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({ content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil.', ephemeral: true });
        }
        if (guild.allowedRoleId && !interaction.member.roles.cache.has(guild.allowedRoleId) && !interaction.member.permissions.has('Administrator') && !interaction.member.permissions.has('ManageGuild') && interaction.guild.ownerId !== interaction.user.id) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const hwid = guild.linkedHwids[0];
        const id = interaction.options.getInteger('id');
        queueCommand(hwid, { type: 'ginvite', target: String(id), requesterId: interaction.id });
        await interaction.reply({ content: `✅ \`/ginvite ${id}\` oyuna iletildi.`, ephemeral: true });
        console.log(`✉️ [GINV] Direkt: ${id} (guild=${interaction.guildId})`);
    }
};
