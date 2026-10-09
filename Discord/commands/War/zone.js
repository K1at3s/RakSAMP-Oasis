const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const db = require('../../db');
const Guild = require('../../models').Guild;

// /bolge <grup|tag> — gangs tablosu: grup adi (PK), kisa tag ve sahip oldugu TUM zone id'leri.
// Kaynak: client'in zone taramasi (POST /api/groups) + ?tag ile elle kayit.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('zone')
        .setNameLocalizations({ tr: 'bolge', 'en-US': 'zone' })
        .setDescription('Shows a group tag and the zones it owns')
        .setDescriptionLocalizations({ tr: 'Grubun tag\'ini ve sahip olduğu zone\'ları gösterir', 'en-US': 'Shows a group tag and the zones it owns' })
        .addStringOption(o => o
            .setName('grup')
            .setNameLocalizations({ tr: 'grup', 'en-US': 'group' })
            .setDescription('Group name or tag (partial match)')
            .setDescriptionLocalizations({ tr: 'Grup adı veya tag (kısmi eşleşme)', 'en-US': 'Group name or tag (partial match)' })
            .setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const isTr = guild ? guild.lang === 'tr' : true;
        const q = interaction.options.getString('grup').trim();
        await interaction.deferReply({ ephemeral: true });

        const rows = await db.execute(
            `SELECT group_name, tag, zones, updated_at FROM gangs
             WHERE LOWER(group_name) LIKE LOWER(?) OR LOWER(tag) = LOWER(?)
             ORDER BY group_name LIMIT 25`,
            [`%${q}%`, q]
        );

        if (!rows || rows.length === 0) {
            return interaction.editReply({
                content: isTr
                    ? `❌ Zone kaydı yok: **${q}** (tarama henüz bu grubu görmedi).`
                    : `❌ No zone record: **${q}** (the scan has not seen this group yet).`
            });
        }

        const embed = new EmbedBuilder()
            .setTitle(isTr ? `🗺️ Zone Sahipliği — ${q}` : `🗺️ Zone Ownership — ${q}`)
            .setColor(0x9B59B6)
            .setTimestamp();

        for (const r of rows.slice(0, 10)) {
            let zones = [];
            try { zones = (r.zones && r.zones !== '') ? JSON.parse(r.zones) : []; } catch { zones = []; }
            if (!Array.isArray(zones)) zones = [];
            const ids = zones.map(z => `#${z}`).join(', ');
            const ts = r.updated_at ? new Date(r.updated_at).toLocaleString('tr-TR', { timeZone: 'Europe/Istanbul' }) : '';
            embed.addFields({
                name: `${r.group_name}${r.tag ? ` [${r.tag}]` : ''}`,
                value: (zones.length ? ids.slice(0, 900) : (isTr ? 'zone yok' : 'no zones'))
                    + (ts ? `\n${isTr ? 'Güncelleme' : 'Updated'}: ${ts}` : '')
            });
        }
        if (rows.length > 10) embed.setFooter({ text: isTr ? `+${rows.length - 10} grup daha` : `+${rows.length - 10} more groups` });

        await interaction.editReply({ embeds: [embed] });
        console.log(`🗺️ [ZONE] guild=${interaction.guildId} q="${q}" -> ${rows.length} grup`);
    }
};