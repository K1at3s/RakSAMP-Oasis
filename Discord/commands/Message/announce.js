const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { hasGroupPermission, getLinkedHwid } = require('../../utils/permissions');

// /duyuru <mesaj> — oyun ici grup chatine duyuru atar.
// "!" oneki client'ta sendChat yoluna girer; botun kendi mesaji oldugu icin
// Discord kopru kanalina geri yazilmaz (sadece oyun ici duyuru olur).
module.exports = {
    data: new SlashCommandBuilder()
        .setName('announce')
        .setNameLocalizations({ tr: 'duyuru', 'en-US': 'announce' })
        .setDescription('Broadcasts a message to the in-game group chat')
        .setDescriptionLocalizations({ tr: 'Oyun içi grup chatine duyuru atar', 'en-US': 'Broadcasts a message to the in-game group chat' })
        .addStringOption(o => o
            .setName('mesaj')
            .setNameLocalizations({ tr: 'mesaj', 'en-US': 'message' })
            .setDescription('Message to broadcast (max 120 chars)')
            .setDescriptionLocalizations({ tr: 'Duyurulacak mesaj (en fazla 120 karakter)', 'en-US': 'Message to broadcast (max 120 chars)' })
            .setRequired(true)
            .setMaxLength(120)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!hasGroupPermission(interaction, guild)) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const hwid = getLinkedHwid(guild);
        if (!hwid) {
            return interaction.reply({ content: '❌ Bu sunucu henüz bir RakSAMP istemcisine bağlı değil.', ephemeral: true });
        }

        const text = interaction.options.getString('mesaj').trim().slice(0, 120);
        if (!text) {
            return interaction.reply({ content: '❌ Mesaj boş olamaz.', ephemeral: true });
        }

        queueCommand(hwid, { type: 'command', cmd: '!' + text, author: interaction.user.username });

        await interaction.reply({
            content: `✅ Duyuru oyuna iletildi: **${text}**\n${guild.lang === 'tr' ? 'Grup chatinde görünecek (Discord köprü kanalına gitmez).' : 'It will appear in the group chat (not mirrored to the Discord bridge channel).'}`,
            ephemeral: true
        });
        console.log(`📣 [ANNOUNCE] guild=${interaction.guildId} hwid=${hwid.slice(0, 8)}: ${text}`);
    }
};