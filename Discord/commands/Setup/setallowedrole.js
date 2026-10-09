const { SlashCommandBuilder, PermissionsBitField, ChannelType, PermissionFlagsBits } = require('discord.js');
const Guild = require('../../models').Guild;

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
        // Rol kanallara da uygulanir: kategoriye yazilir, kanallar senkronlanir.
        try {
            const ow = [{ id: interaction.guild.roles.everyone.id, deny: [PermissionFlagsBits.ViewChannel] }];
            if (role) ow.push({ id: role.id, allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages, PermissionFlagsBits.ReadMessageHistory] });
            const g2 = await Guild.findOne({ guildId: interaction.guildId });
            const ids = [g2.categoryId, g2.warChannelId, g2.gstatsChannelId, g2.groupChatChannelId, g2.zoneWarChannelId, g2.backupChannelId, g2.groupLogChannelId, g2.botCommandsChannelId].filter(Boolean);
            for (const id of ids) {
                try {
                    const ch = await interaction.guild.channels.fetch(id);
                    if (!ch) continue;
                    if (ch.type === ChannelType.GuildCategory) await ch.permissionOverwrites.set(ow);
                    else if (g2.categoryId) { try { await ch.lockPermissions(); } catch (e) {} }
                    else await ch.permissionOverwrites.set(ow);
                } catch (e) {}
            }
        } catch (e) {}
        if (role) {
            await interaction.reply({ content: `✅ Grole/Grank menuleri artik sadece <@&${role.id}> rolundekiler kullanabilir. Kanallar da bu role kapatildi.`, ephemeral: true });
        } else {
            await interaction.reply({ content: '✅ Kisitlama kaldirildi — menulerde herkes kullanabilir. Kanallar sadece yoneticilere gorunur.', ephemeral: true });
        }
        console.log(`🔐 [ALLOWED-ROLE] guild=${interaction.guildId} -> ${role ? role.id : 'null'} (by ${interaction.user.tag})`);
    }
};
