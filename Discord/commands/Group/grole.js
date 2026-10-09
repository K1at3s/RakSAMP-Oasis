const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../../models').Guild;
const { queueCommand } = require('../../utils/commandOutbox');
const { setPendingInteraction } = require('../../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('grole')
        .setNameLocalizations({ tr: 'grol', 'en-US': 'grole' })
        .setDescription('Grup uyesine rol ver (Member / Co-Leader) — interaktif veya direkt')
        .setDescriptionLocalizations({ tr: 'Grup üyesine rol ver (Member / Co-Leader) — interaktif veya direkt', 'en-US': 'Give a group member a role (Member / Co-Leader) — interactive or direct' })
        .addStringOption(o => o.setName('name').setDescription('Oyuncu adi (bos birakirsan listeden secersin)').setRequired(false))
        .addStringOption(o => o.setName('role').setDescription('Rol: mem (Member) veya co (Co-Leader)').setRequired(false)
            .addChoices(
                { name: 'Member (mem)', value: 'mem' },
                { name: 'Co-Leader (co)', value: 'co' }
            )),
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
        const role = interaction.options.getString('role');

        // Direkt mod: name + role verildiyse direk otomasyona gonder
        if (name && role) {
            const roleMap = { mem: 'Member', co: 'Co-Leader' };
            const fullRole = roleMap[role] || role;
            queueCommand(hwid, { type: 'grole', target: name, role: fullRole, requesterId: interaction.id });
            await interaction.reply({ content: `✅ \`/grole ${name} → ${fullRole}\` kuyruga alindi — sayfali listede araniyor...`, ephemeral: true });
            console.log(`👤 [GROLE] Direkt: ${name} -> ${fullRole} (guild=${interaction.guildId})`);
            return;
        }
        // Interaktif mod: tum uyeleri cek, select menu goster
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'grole_list', requesterId: interaction.id, guildId: interaction.guildId, hwid });
        await interaction.editReply({ content: '⏳ Uye listesi oyundan cekiliyor...' });
        console.log(`👥 [GROLE-LIST] Istek kuyruga alindi (guild=${interaction.guildId}, req=${interaction.id.slice(0,6)})`);
    }
};
