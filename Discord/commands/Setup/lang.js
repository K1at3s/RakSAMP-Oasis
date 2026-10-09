const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;

// /dil <tr|en> — sunucunun bot dilini ayarlar/gosterir (embed + oyun ici duyurular).
// Client dili 2 sn'lik grup chat poll'undan (GET /api/groupchat/poll -> lang) senkronlar,
// yani oyun ici mesajlar da aninda secilen dile doner.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('lang')
        .setNameLocalizations({ tr: 'dil', 'en-US': 'lang' })
        .setDescription('Sets or shows the bot language of this server')
        .setDescriptionLocalizations({ tr: 'Sunucunun bot dilini ayarlar/gösterir', 'en-US': 'Sets or shows the bot language of this server' })
        .addStringOption(o => o
            .setName('lang')
            .setNameLocalizations({ tr: 'dil', 'en-US': 'lang' })
            .setDescription('tr or en (blank = shows the current one)')
            .setDescriptionLocalizations({ tr: 'tr veya en (boş = mevcut dili gösterir)', 'en-US': 'tr or en (blank = shows the current one)' })
            .setRequired(false)
            .addChoices(
                { name: 'Türkçe (tr)', value: 'tr' },
                { name: 'English (en)', value: 'en' }
            )),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const current = guild && guild.lang ? guild.lang : 'en';
        const choice = interaction.options.getString('lang');

        if (!choice) {
            return interaction.reply({
                content: current === 'tr'
                    ? '🌐 Bu sunucunun dili: **TR** (Türkçe). Değiştirmek için: `/dil dil:en`'
                    : '🌐 This server\'s language: **EN**. To change it: `/lang lang:tr`',
                ephemeral: true
            });
        }

        await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $set: { lang: choice } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply({
            content: choice === 'tr'
                ? '✅ Bot dili **Türkçe** olarak ayarlandı (client 2 sn içinde senkronlar).'
                : '✅ Bot language set to **English** (the client syncs within 2s).',
            ephemeral: true
        });
        console.log(`🌐 [LANG] guild=${interaction.guildId} -> ${choice} (by ${interaction.user.tag})`);
    }
};