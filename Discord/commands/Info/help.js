const { SlashCommandBuilder, EmbedBuilder, ActionRowBuilder, StringSelectMenuBuilder, blockQuote } = require('discord.js');
const fs = require('fs');
const path = require('path');
const Guild = require('../../models').Guild;

// Kategori klasorleri Discord/commands/<Kategori>/. Loader ile ayni yolu paylasir.
const { listCategories, commandsDir } = require('../../handlers/commandHandler');

const CAT_META = {
    Group: { emoji: '👥', tr: 'Grup Yönetimi', en: 'Group Management' },
    Info: { emoji: '📊', tr: 'Bilgi / Sorgu', en: 'Info / Lookup' },
    War: { emoji: '⚔️', tr: 'Savaş & Zone', en: 'War & Zone' },
    Message: { emoji: '✉️', tr: 'Mesaj & Çağrı', en: 'Message & Call' },
    Setup: { emoji: '⚙️', tr: 'Ayarlar', en: 'Settings' },
};

const meta = (cat, isTr) => {
    const m = CAT_META[cat];
    if (!m) return { emoji: '❓', label: cat };
    return { emoji: m.emoji, label: isTr ? m.tr : m.en };
};

function categories() {
    return listCategories().filter(c => fs.existsSync(path.join(commandsDir, c)));
}

function commandsOf(cat) {
    const dir = path.join(commandsDir, cat);
    return fs.readdirSync(dir)
        .filter(f => f.endsWith('.js'))
        .map(f => require(path.join(dir, f)))
        .filter(c => c && c.data);
}

// Slash komutlarinin lokalizasyonlu ad/aciklamasi
function cmdName(cmd, isTr) {
    return '/' + (isTr ? (cmd.data.name_localizations && cmd.data.name_localizations.tr) || cmd.data.name : cmd.data.name);
}
function cmdDesc(cmd, isTr) {
    const l = cmd.data.description_localizations;
    const d = isTr ? (l && l.tr) || cmd.data.description : cmd.data.description;
    return d || (isTr ? 'Açıklama yok.' : 'No description.');
}

function introEmbed(isTr) {
    const cats = categories();
    const botName = 'RakSAMP';
    const list = cats.map(c => {
        const m = meta(c, isTr);
        return `${m.emoji} ${m.label}`;
    });
    return new EmbedBuilder()
        .setAuthor({ name: `${botName} help command` })
        .setDescription(
            `**${botName} Strengthen your server with**\n` +
            blockQuote(isTr
                ? `Merhaba, sunucunu yönetmene yardım ediyorum. Tüm komutları bu menüden görebilirsin.`
                : `Hello, I help you run your server. You can browse every command from this menu.`)
        )
        .setColor(0x00AEFF)
        .addFields({
            name: isTr ? 'Bir kategori seçerek komutları görüntüleyebilirsiniz.' : 'You can look at commands by selecting a category.',
            value: blockQuote(list.join('\n'))
        })
        .setFooter({ text: isTr ? 'Kategori listesi Discord/commands klasöründen okunur.' : 'Category list is read from the Discord/commands folder.' })
        .setTimestamp();
}

module.exports = {
    data: new SlashCommandBuilder()
        .setName('help')
        .setNameLocalizations({ tr: 'yardim', 'en-US': 'help' })
        .setDescription('Shows the commands by category')
        .setDescriptionLocalizations({ tr: 'Komutları kategori kategori gösterir', 'en-US': 'Shows the commands by category' })
        .addBooleanOption(o => o
            .setName('share')
            .setNameLocalizations({ tr: 'paylas', 'en-US': 'share' })
            .setDescription('Also post the list to the bot-commands channel (default: no)')
            .setDescriptionLocalizations({ tr: 'Listeyi bot-komut kanalına da gönder (varsayılan: hayır)', 'en-US': 'Also post the list to the bot-commands channel (default: no)' })
            .setRequired(false)),

    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const isTr = guild ? guild.lang === 'tr' : true;
        const embed = introEmbed(isTr);
        const share = interaction.options.getBoolean('share') || false;

        if (share) {
            const channelId = guild ? guild.botCommandsChannelId : null;
            if (!channelId) {
                return interaction.reply({
                    content: isTr
                        ? '⚠️ Bot komut kanalı ayarlı değil. `/kanalayarla bot-commands <kanal>` ile ayarla veya `/kurulum` çalıştır.'
                        : '⚠️ No bot-commands channel configured. Set it with `/setchannel bot-commands <channel>` or run `/setup`.',
                    ephemeral: true
                });
            }
            const channel = await interaction.client.channels.fetch(channelId).catch(() => null);
            if (!channel || !channel.send) {
                return interaction.reply({ content: isTr ? '❌ Bot komut kanalı bulunamadı.' : '❌ Bot-commands channel not found.', ephemeral: true });
            }
            await channel.send({ embeds: [embed] });
            return interaction.reply({
                content: isTr ? '✅ Komut listesi kanala gönderildi: <#' + channel.id + '>' : '✅ Command list posted to <#' + channel.id + '>.',
                ephemeral: true
            });
        }

        const options = categories().map(c => {
            const m = meta(c, isTr);
            return { label: m.label, description: m.label, value: c, emoji: m.emoji };
        });
        const select = new StringSelectMenuBuilder()
            .setCustomId('category_select')
            .setPlaceholder(isTr ? 'Bir kategori seçin...' : 'Select a category...')
            .addOptions(options);
        const row = new ActionRowBuilder().addComponents(select);

        await interaction.reply({ embeds: [embed], components: [row], ephemeral: true });

        const filter = i => i.customId === 'category_select' && i.user.id === interaction.user.id;
        const collector = interaction.channel.createMessageComponentCollector({ filter, time: 60000 });

        collector.on('collect', async i => {
            const cat = i.values[0];
            const m = meta(cat, isTr);
            const lines = commandsOf(cat).map(c => `🔹 \`${cmdName(c, isTr)}\`: ${cmdDesc(c, isTr)}`);
            const catEmbed = new EmbedBuilder()
                .setTitle(`${m.emoji} ${m.label} ${isTr ? 'Komutları' : 'Commands'}`)
                .setDescription((lines.length ? lines.join('\n') : (isTr ? 'Bu kategoride komut yok.' : 'No commands in this category.')).slice(0, 4096))
                .setColor(0x00AEFF)
                .setFooter({ text: isTr ? 'Başka kategori seçmek için menüyü kullanabilirsiniz.' : 'You can select another category using the menu.' });

            await i.update({ embeds: [catEmbed], components: [row] }).catch(() => {});
        });

        collector.on('end', collected => {
            if (collected.size === 0) {
                interaction.editReply({
                    content: isTr ? 'Zaman aşımına uğradı, lütfen tekrar deneyin.' : 'Timed out, please try again.',
                    components: []
                }).catch(() => {});
            }
        });
    }
};