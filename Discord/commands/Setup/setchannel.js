const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setchannel')
        .setNameLocalizations({ tr: 'kanalayarla', 'en-US': 'setchannel' })
        .setDescription('Kanal ayarlar (ayarlanmazsa null olarak kalir)')
        .setDescriptionLocalizations({ tr: 'Kanal ayarlar (ayarlanmazsa null olarak kalır)', 'en-US': 'Sets a channel (stays null if not set)' })
        .addStringOption(o => o.setName('type')
            .setDescription('Kanal turu')
            .setRequired(true)
            .addChoices(
                { name: '⚔️ War Mesajlari', value: 'war' },
                { name: '📊 GStats Verileri', value: 'gstats' },
                { name: '💬 Grup Chat Koprusu', value: 'group-chat' },
                { name: '⚔️ ZONE WAR Bildirimleri', value: 'zone-war' },
                { name: '🆘 Backup Cagrilari (?1414)', value: 'backup' },
                { name: '📋 Grup Log (?post)', value: 'group-log' },
                { name: '🤖 Bot Komut Kanali', value: 'bot-commands' }
            ))
        .addChannelOption(o => o.setName('kanal')
            .setDescription('Hedef kanal')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const type = interaction.options.getString('type');
        const channel = interaction.options.getChannel('kanal');

        const fieldMap = {
            war: 'warChannelId',
            gstats: 'gstatsChannelId',
            'group-chat': 'groupChatChannelId',
            'zone-war': 'zoneWarChannelId',
            backup: 'backupChannelId',
            'group-log': 'groupLogChannelId',
            'bot-commands': 'botCommandsChannelId'
        };
        const field = fieldMap[type] || `${type}ChannelId`;

        await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $set: { [field]: channel.id } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(`✅ **${type}** kanali <#${channel.id}> olarak ayarlandi.`);
    }
};
