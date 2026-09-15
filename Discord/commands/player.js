const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const db = require('../db');
const Guild = require('../models').Guild;

// /oyuncu <isim|id> — client'in push ettigi players cache'i (POST /api/player/cache).
// Degerler oyun ici periyodik "Player [X] Stats -> Packetloss: y | FPS: z | Ping: w"
// mesajindan gelir; satirin yasi (updated_at) tabloda gosterilir.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('player')
        .setNameLocalizations({ tr: 'oyuncu', 'en-US': 'player' })
        .setDescription('Shows cached player stats (packetloss / FPS / ping)')
        .setDescriptionLocalizations({ tr: 'Cache\'deki oyuncu istatistiklerini gösterir (packetloss / FPS / ping)', 'en-US': 'Shows cached player stats (packetloss / FPS / ping)' })
        .addStringOption(o => o
            .setName('oyuncu')
            .setNameLocalizations({ tr: 'oyuncu', 'en-US': 'player' })
            .setDescription('Player name or SAMP player ID')
            .setDescriptionLocalizations({ tr: 'Oyuncu adı veya SAMP player ID', 'en-US': 'Player name or SAMP player ID' })
            .setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const isTr = guild ? guild.lang === 'tr' : true;
        const q = interaction.options.getString('oyuncu').trim();
        await interaction.deferReply({ ephemeral: true });

        const cols = 'id, name, packetloss, fps, ping, updated_at';
        let rows;
        if (/^\d+$/.test(q)) {
            rows = await db.execute(`SELECT ${cols} FROM players WHERE id = ? LIMIT 1`, [Number(q)]);
        } else {
            rows = await db.execute(
                `SELECT ${cols} FROM players WHERE LOWER(name) LIKE LOWER(?) ORDER BY id LIMIT 25`,
                [`%${q}%`]
            );
        }

        if (!rows || rows.length === 0) {
            return interaction.editReply({
                content: isTr
                    ? `❌ Cache'de kayıt yok: **${q}** (client istatistik mesajını görmeden yazılmaz).`
                    : `❌ No cached record: **${q}** (written only after the client sees a stats message).`
            });
        }

        const ageText = (v) => {
            if (!v) return '-';
            const sec = Math.max(0, Math.round((Date.now() - new Date(v).getTime()) / 1000));
            if (sec < 60) return (isTr ? `${sec} sn önce` : `${sec}s ago`);
            if (sec < 3600) return (isTr ? `${Math.round(sec / 60)} dk önce` : `${Math.round(sec / 60)}m ago`);
            return (isTr ? `${Math.round(sec / 3600)} sa önce` : `${Math.round(sec / 3600)}h ago`);
        };

        const lines = rows.map(r => {
            const pl = (r.packetloss === null || r.packetloss === undefined) ? '-' : Number(r.packetloss).toFixed(1);
            return `\`${r.id}\` **${r.name || '?'}** — PL: ${pl} | FPS: ${r.fps ?? '-'} | Ping: ${r.ping ?? '-'} | ${ageText(r.updated_at)}`;
        }).join('\n');

        const embed = new EmbedBuilder()
            .setTitle(isTr ? `🎮 Oyuncu Cache — ${q}` : `🎮 Player Cache — ${q}`)
            .setDescription(lines.slice(0, 3900))
            .setColor(0x3498DB)
            .setFooter({ text: isTr ? 'Veri client push eder, canlı değil' : 'Data pushed by the client, not live' })
            .setTimestamp();

        await interaction.editReply({ embeds: [embed] });
        console.log(`🎮 [PLAYER] guild=${interaction.guildId} q="${q}" -> ${rows.length} satir`);
    }
};