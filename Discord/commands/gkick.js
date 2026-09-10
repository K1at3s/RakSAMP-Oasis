const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gkick')
        .setNameLocalizations({ tr: 'gkick', 'en-US': 'gkick' })
        .setDescription('Grup uyesini kickle — interaktif veya direkt (sebep opsiyonel)')
        .setDescriptionLocalizations({ tr: 'Grup üyesini kickle — interaktif veya direkt (sebep opsiyonel)', 'en-US': 'Kick a group member — interactive or direct (reason optional)' })
        .addStringOption(o => o.setName('name').setDescription('Oyuncu adi (bos birakirsan listeden secersin)').setRequired(false))
        .addStringOption(o => o.setName('reason').setDescription('Kick sebebi (bos birakirsan sebepsiz)').setRequired(false)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({ content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil.', ephemeral: true });
        }
        if (guild.allowedRoleId && !interaction.member.roles.cache.has(guild.allowedRoleId) && !interaction.member.permissions.has('Administrator') && !interaction.member.permissions.has('ManageGuild') && interaction.guild.ownerId !== interaction.user.id) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const hwid = guild.linkedHwids[0];
        const name = interaction.options.getString('name');
        const reason = interaction.options.getString('reason') || '';

        // Direkt mod: name verildiyse direk otomasyona gonder (sebep bos olabilir)
        if (name) {
            queueCommand(hwid, { type: 'gkick', target: name, reason, requesterId: interaction.id });
            await interaction.reply({ content: `✅ \`/gkick ${name}\` kuyruga alindi — sayfali listede araniyor...`, ephemeral: true });
            console.log(`👢 [GKICK] Direkt: ${name} (sebep: ${reason || '-'}) (guild=${interaction.guildId})`);
            return;
        }
        // Interaktif mod: once uye listesi, secimden sonra sebep modal
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'gkick_list', requesterId: interaction.id, guildId: interaction.guildId, hwid });
        await interaction.editReply({ content: '⏳ Uye listesi oyundan cekiliyor...' });
        console.log(`👥 [GKICK-LIST] Istek kuyruga alindi (guild=${interaction.guildId}, req=${interaction.id.slice(0,6)})`);
    }
};
