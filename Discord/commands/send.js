const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('send')
        .setNameLocalizations({ tr: 'gonder', 'en-US': 'send' })
        .setDescription('Oyun icindeki RakSAMP istemcisine komut/sohbet mesaji iletir')
        .setDescriptionLocalizations({ tr: 'Oyun içindeki RakSAMP istemcisine komut/sohbet mesajı iletir', 'en-US': 'Sends a command/chat message to the in-game RakSAMP client' })
        .addStringOption(o => o.setName('komut')
            .setDescription('Iletilecek komut (orn. ?au Merhaba, /gstats, selam)')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const cmd = interaction.options.getString('komut').trim();
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanin.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'command', cmd, author: interaction.user.username });

        await interaction.reply(`✅ Komut kuyruga alindi. **\`${hwid}\`** istemcisine iletiliyor: \`${cmd.slice(0, 150)}\``);
    }
};
