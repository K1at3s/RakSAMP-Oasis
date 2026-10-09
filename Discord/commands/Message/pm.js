const { SlashCommandBuilder, EmbedBuilder, MessageFlags } = require('discord.js');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { setPendingInteraction } = require('../../utils/pendingReplies');

// /pm: oyun içindeki bir oyuncuya mesaj gönderir (grup chat'inde "[Discord][isim]: mesaj").
// Oyuncu yanıt verirse Discord'da bu komutu kullanan kişiye etiketli + ephemeral gösterilir.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('pm')
        .setNameLocalizations({ tr: 'mesaj', 'en-US': 'pm' })
        .setDescription('Oyun icindeki bir oyuncuya mesaj gonderir')
        .setDescriptionLocalizations({ tr: 'Oyun içindeki bir oyuncuya mesaj gönderir', 'en-US': 'Sends a message to an in-game player' })
        .addStringOption(o => o.setName('oyuncu')
            .setNameLocalizations({ tr: 'oyuncu', 'en-US': 'player' })
            .setDescription('Hedef oyuncu adi')
            .setDescriptionLocalizations({ tr: 'Hedef oyuncu adı', 'en-US': 'Target player name' })
            .setRequired(true))
        .addStringOption(o => o.setName('mesaj')
            .setNameLocalizations({ tr: 'mesaj', 'en-US': 'message' })
            .setDescription('Gonderilecek mesaj')
            .setDescriptionLocalizations({ tr: 'Gönderilecek mesaj', 'en-US': 'Message to send' })
            .setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({ content: '❌ Bağlı RakSAMP istemcisi yok.', ephemeral: true });
        }
        const hwid = guild.linkedHwids[0];
        const target = interaction.options.getString('oyuncu').slice(0, 63);
        const message = interaction.options.getString('mesaj').slice(0, 200);
        const isTr = guild.lang === 'tr';

        await interaction.deferReply({ flags: MessageFlags.Ephemeral });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'pm', target, message, discordUser: interaction.user.username, requesterId: interaction.id });

        const embed = new EmbedBuilder()
            .setTitle(isTr ? '📨 Mesaj Gönderildi' : '📨 Message Sent')
            .setDescription(isTr
                ? `\`${target}\` oyuncusuna: ${message}`
                : `To \`${target}\`: ${message}`)
            .setColor(0x00FF00)
            .setFooter({ text: isTr ? 'Yanıt gelince burada görünür' : 'Replies will appear here' });
        await interaction.editReply({ embeds: [embed] });
    }
};