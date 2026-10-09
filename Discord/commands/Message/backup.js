const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../../models').Guild;
const { sendToGuildChannel } = require('../../utils/discordNotifier');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('backup')
        .setNameLocalizations({ tr: 'destek', 'en-US': 'backup' })
        .setDescription('Backup cagrisi gonderir (backup kanalina @here acil yardim)')
        .setDescriptionLocalizations({ tr: 'Backup çağrısı gönderir (backup kanalına @here acil yardım)', 'en-US': 'Sends a backup call (@here urgent help to the backup channel)' })
        .addStringOption(o => o.setName('grup').setDescription('Grup adi').setRequired(true))
        .addStringOption(o => o.setName('mesaj').setDescription('Ek mesaj').setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) {
            return interaction.reply({ content: '❌ Sunucu kaydi bulunamadi.', ephemeral: true });
        }

        const group = interaction.options.getString('grup');
        const extra = interaction.options.getString('mesaj') || '';

        const content = `@here **${group} backup istiyor!**${extra ? `\n${extra}` : ''}`;

        const channelId = await sendToGuildChannel(guild, 'backupChannelId', { content });
        await interaction.reply({
            content: channelId
                ? `✅ Backup cagrisi <#${channelId}> kanalina gonderildi.`
                : '⚠️ backup kanali ayarlanmamis. `/setchannel backup <kanal>` ile ayarlayin.',
            ephemeral: true
        });
    }
};
