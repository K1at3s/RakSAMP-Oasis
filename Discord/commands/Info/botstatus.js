const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const db = require('../../db');
const Guild = require('../../models').Guild;

// /botdurum — bots tablosu: bot adi, ogrenilen grup adi, zscan/gzt/zgrp anahtarlari,
// son guncelleme. Kayitlar guild.groupName ile eslesir; boylece ayni DB'yi paylasan
// diger gruplarin bot kayitlari listelenmez.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('botstatus')
        .setNameLocalizations({ tr: 'botdurum', 'en-US': 'botstatus' })
        .setDescription('Shows this group\'s bot record (group name, scan toggles, last update)')
        .setDescriptionLocalizations({ tr: 'Bu grubun bot kaydını gösterir (grup adı, tarama anahtarları, son güncelleme)', 'en-US': 'Shows this group\'s bot record (group name, scan toggles, last update)' }),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const isTr = guild ? guild.lang === 'tr' : true;
        await interaction.deferReply({ ephemeral: true });

        const group = guild && guild.groupName ? String(guild.groupName).trim() : '';
        if (!group) {
            return interaction.editReply({
                content: isTr
                    ? '⚠️ Grup adı henüz öğrenilmedi (client bağlanınca ilk grup chat poll\'unda öğrenilir).'
                    : '⚠️ Group name not learned yet (learned on the client\'s first group-chat poll).'
            });
        }

        const rows = await db.execute(
            `SELECT name, group_name, zscan_enabled, gzt_enabled, zgrp_enabled, updated_at
             FROM bots WHERE group_name = ? ORDER BY updated_at DESC LIMIT 5`,
            [group]
        );

        if (!rows || rows.length === 0) {
            return interaction.editReply({ content: isTr ? `❌ Bot kaydı yok: **${group}**` : `❌ No bot record: **${group}**` });
        }

        const embed = new EmbedBuilder()
            .setTitle(isTr ? `🤖 Bot Durumu — ${group}` : `🤖 Bot Status — ${group}`)
            .setColor(0x2ECC71)
            .setTimestamp();

        const onOff = (v) => (v ? (isTr ? 'açık' : 'on') : (isTr ? 'kapalı' : 'off'));
        for (const r of rows) {
            const ts = r.updated_at ? new Date(r.updated_at).toLocaleString('tr-TR', { timeZone: 'Europe/Istanbul' }) : '-';
            embed.addFields({
                name: r.name || '?',
                value: [
                    `zscan: **${onOff(r.zscan_enabled)}**`,
                    `gzt: **${onOff(r.gzt_enabled)}**`,
                    `zgrp: **${onOff(r.zgrp_enabled)}**`,
                    `${isTr ? 'Güncelleme' : 'Updated'}: ${ts}`
                ].join(' | ')
            });
        }

        await interaction.editReply({ embeds: [embed] });
        console.log(`🤖 [BOTSTATUS] guild=${interaction.guildId} group="${group}" -> ${rows.length} kayit`);
    }
};