const { SlashCommandBuilder, PermissionsBitField } = require('discord.js');
const Guild = require('../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setallowedrole')
        .setNameLocalizations({ tr: 'iznirol', 'en-US': 'setallowedrole' })
        .setDescription('Grole/Grank menulerini kullanabilecek rolu ayarla (DB)')
        .setDescriptionLocalizations({ tr: 'Grole/Grank menülerini kullanabilecek rolü ayarla (DB)', 'en-US': 'Sets the role allowed to use the Grole/Grank menus (DB)' })
        .addRoleOption(o => o.setName('role').setDescription('Izin verilecek rol (bos birakirsan herkes)').setRequired(false)),
    async execute(interaction) {
        if (!interaction.member.permissions.has(PermissionsBitField.Flags.ManageGuild) && interaction.guild.ownerId !== interaction.user.id) {
            return interaction.reply({ content: '❌ Bu komutu sadece sunucu sahibi veya ManageGuild yetkisi olanlar kullanabilir.', ephemeral: true });
        }
        const role = interaction.options.getRole('role');
        let guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) guild = await Guild.create({ guildId: interaction.guildId });
        guild.allowedRoleId = role ? role.id : null;
        await guild.save();
        if (role) {
            await interaction.reply({ content: `✅ Grole/Grank menuleri artik sadece <@&${role.id}> rolundekiler kullanabilir.`, ephemeral: true });
        } else {
            await interaction.reply({ content: '✅ Kisitlama kaldirildi — herkes kullanabilir.', ephemeral: true });
        }
        console.log(`🔐 [ALLOWED-ROLE] guild=${interaction.guildId} -> ${role ? role.id : 'null'} (by ${interaction.user.tag})`);
    }
};
