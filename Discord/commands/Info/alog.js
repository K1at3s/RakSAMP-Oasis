const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const db = require('../../db');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('alog')
        .setNameLocalizations({ tr: 'alog', 'en-US': 'alog' })
        .setDescription('Oyuncunun admin log gecmisini gosterir (ban/mute/kick, mod, sebep)')
        .setDescriptionLocalizations({ tr: 'Oyuncunun admin log geçmişini gösterir (ban/mute/kick, mod, sebep)', 'en-US': "Shows the player's admin log history (ban/mute/kick, mode, reason)" })
        .addStringOption(o => o.setName('oyuncu').setDescription('Oyuncu adi veya ID').setRequired(true)),
    async execute(interaction) {
        const q = interaction.options.getString('oyuncu').trim();
        await interaction.deferReply({ ephemeral: true });

        let rows;
        if (/^\d+$/.test(q)) {
            rows = await db.execute(
                `SELECT id, admin_name, target_name, target_id, action, mode, duration_min, reason, created_at
                 FROM admin_logs
                 WHERE target_id = ?
                 ORDER BY id DESC
                 LIMIT 25`,
                [Number(q)]
            );
        } else {
            rows = await db.execute(
                `SELECT id, admin_name, target_name, target_id, action, mode, duration_min, reason, created_at
                 FROM admin_logs
                 WHERE LOWER(target_name) LIKE LOWER(?)
                 ORDER BY id DESC
                 LIMIT 25`,
                [`%${q}%`]
            );
        }

        if (!rows || rows.length === 0) {
            return interaction.editReply({ content: `❌ Kayit yok: **${q}**` });
        }

        const lines = rows.map(r => {
            const parts = [`${r.action || '?'}`];
            if (r.mode) parts.push(`**${r.mode}**`);
            if (r.duration_min) parts.push(`${r.duration_min} dk`);
            if (r.reason) parts.push(`*${r.reason}*`);
            const ts = r.created_at ? new Date(r.created_at).toLocaleString('tr-TR', { timeZone: 'Europe/Istanbul' }) : '';
            return `\`${r.id}\` \`${ts}\` ${parts.join(' | ')}`;
        }).join('\n');

        const embed = new EmbedBuilder()
            .setTitle(`📋 Admin Log — ${rows[0].target_name}${rows[0].target_id ? ` (${rows[0].target_id})` : ''}`)
            .setDescription(`Son **${rows.length}** kayit:\n\n${lines}`)
            .setColor(0xFF5555)
            .setTimestamp();

        await interaction.editReply({ embeds: [embed] });
    }
};