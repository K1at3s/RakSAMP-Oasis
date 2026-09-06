const { SlashCommandBuilder, PermissionFlagsBits, EmbedBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { sendToGuildChannel } = require('../utils/discordNotifier');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('zonewar')
        .setNameLocalizations({ tr: 'bolgesavasi', 'en-US': 'zonewar' })
        .setDescription('ZONE WAR bildirimini manuel olarak zonewar kanalina gonderir')
        .setDescriptionLocalizations({ tr: 'ZONE WAR bildirimini manuel olarak zonewar kanalına gönderir', 'en-US': 'Manually sends a ZONE WAR notification to the zonewar channel' })
        .addStringOption(o => o.setName('grup').setDescription('Saldiran grup').setRequired(true))
        .addStringOption(o => o.setName('durum')
            .setDescription('Savas durumu')
            .setRequired(true)
            .addChoices(
                { name: 'Zone tuttu', value: 'keeps' },
                { name: 'Zone aldi', value: 'takes' },
                { name: 'Kazandi', value: 'wins' },
                { name: 'Kaybetti', value: 'loses' }
            ))
        .addStringOption(o => o.setName('rakip').setDescription('Rakip grup (wins/loses icin)').setRequired(false))
        .addStringOption(o => o.setName('bolge').setDescription('Bolge adi').setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) {
            return interaction.reply({ content: '❌ Sunucu kaydi bulunamadi.', ephemeral: true });
        }

        const ownGroup = interaction.options.getString('grup');
        const action = interaction.options.getString('durum');
        const enemyGroup = interaction.options.getString('rakip') || '';
        const zoneName = interaction.options.getString('bolge') || '';

        const actionText = {
            keeps: 'keeps zone',
            takes: 'takes over the zone',
            wins: 'wins against',
            loses: 'loses to'
        }[action] || action;

        let description;
        if (action === 'wins' || action === 'loses') {
            description = `**${ownGroup}** ${actionText} **${enemyGroup || '?'}**`;
        } else {
            description = `**${ownGroup}** ${actionText}${zoneName ? ` **${zoneName}**` : ''}`;
        }

        const embed = new EmbedBuilder()
            .setTitle("⚔️ ZONE WAR")
            .setDescription(description)
            .setColor(action === 'loses' ? 0xFF0000 : 0xFFA500)
            .setTimestamp();

        const channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', { embed });
        await interaction.reply({
            content: channelId
                ? `✅ ZONE WAR bildirimi <#${channelId}> kanalina gonderildi.`
                : '⚠️ zonewar kanali ayarlanmamis. `/setchannel zone-war <kanal>` ile ayarlayin.',
            ephemeral: true
        });
    }
};
