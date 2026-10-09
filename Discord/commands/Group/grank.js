const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { setPendingInteraction } = require('../../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('grank')
        .setNameLocalizations({ tr: 'gran', 'en-US': 'grank' })
        .setDescription('Grup uyesine rank ver — interaktif veya direkt')
        .setDescriptionLocalizations({ tr: 'Grup üyesine rank ver — interaktif veya direkt', 'en-US': 'Give a group member a rank — interactive or direct' })
        .addStringOption(o => o.setName('name').setDescription('Oyuncu adi (bos birakirsan listeden secersin)').setRequired(false))
        .addStringOption(o => o.setName('rank').setDescription('Rank adi (or: Co-Leader, Member, vb.)').setRequired(false)),
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
        const rank = interaction.options.getString('rank');
        if (name && rank) {
            queueCommand(hwid, { type: 'grank', target: name, rank, requesterId: interaction.id });
            await interaction.reply({ content: `✅ \`/grank ${name} → ${rank}\` kuyruga alindi — sayfali listede araniyor...`, ephemeral: true });
            console.log(`🏷️ [GRANK] Direkt: ${name} -> ${rank} (guild=${interaction.guildId})`);
            return;
        }
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'grank_list', requesterId: interaction.id, guildId: interaction.guildId, hwid });
        await interaction.editReply({ content: '⏳ Uye listesi oyundan cekiliyor...' });
        console.log(`👥 [GRANK-LIST] Istek kuyruga alindi (guild=${interaction.guildId}, req=${interaction.id.slice(0,6)})`);
    }
};
