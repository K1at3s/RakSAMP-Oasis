const { SlashCommandBuilder, PermissionFlagsBits, ChannelType } = require('discord.js');
const Guild = require('../models').Guild;

// /kurulum (setup): BARE = kanallari olusturur. Kapatmak icin islem:kapat (EN: off).
//   TR: /kurulum            -> olustur
//   TR: /kurulum işlem:kapat -> sil
//   EN: /setup              -> create
//   EN: /setup action:off   -> close
// Kanallar otomatik OLUSTURULMAZ; yalnizca bu komutla kurulur.
const CATEGORY = '🤖-raksamp';
const CHANNELS = [
    { key: 'warChannelId', name: '⚔️-war' },
    { key: 'gstatsChannelId', name: '📊-gstats' },
    { key: 'groupChatChannelId', name: '💬-grup-chat' },
    { key: 'zoneWarChannelId', name: '⚔️-zone-war' },
    { key: 'backupChannelId', name: '🆘-backup' },
    { key: 'groupLogChannelId', name: '📋-grup-log' },
    { key: 'botCommandsChannelId', name: '🤖-bot-komut' }
];
const CHANNEL_FIELDS = CHANNELS.map(c => c.key);

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setup')
        .setNameLocalizations({ tr: 'kurulum', 'en-US': 'setup' })
        .setDescription('Botun kanallarini olusturur veya kapatir')
        .setDescriptionLocalizations({ tr: 'Botun kanallarını oluşturur veya kapatır', 'en-US': 'Creates or closes the bot channels' })
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
        .addStringOption(o => o
            .setName('islem')
            .setNameLocalizations({ tr: 'islem', 'en-US': 'action' })
            .setDescription('Kapatma islemi (bos = kanallari olustur)')
            .setDescriptionLocalizations({ tr: 'Kapatma işlemi (boş = kanalları oluştur)', 'en-US': 'Close action (empty = create channels)' })
            .setRequired(false)
            .addChoices(
                { name: 'Kapat (kanallari sil)', value: 'kapat' },
                { name: 'Close (delete channels)', value: 'off' }
            )),
    async execute(interaction) {
        const action = interaction.options.getString('islem') || '';
        if (action === 'kapat' || action === 'off') return close(interaction);
        return create(interaction);
    }
};

async function create(interaction) {
    await interaction.deferReply();
    const guild = interaction.guild;
    const gRec = await Guild.findOne({ guildId: guild.id });
    const isTr = gRec ? gRec.lang === 'tr' : true;

    const updates = {};
    const created = [];
    const skipped = [];

    // Once kategori: kanallar onun altinda acilir.
    let parentId = null;
    try {
        const cat = await guild.channels.create({ name: CATEGORY, type: ChannelType.GuildCategory });
        parentId = cat.id;
    } catch (e) { /* kategorisiz devam */ }

    for (const c of CHANNELS) {
        try {
            const ch = await guild.channels.create({ name: c.name, type: ChannelType.GuildText, ...(parentId ? { parent: parentId } : {}) });
            updates[c.key] = ch.id;
            created.push(c.name);
        } catch (e) {
            skipped.push(c.name);
        }
    }

    if (Object.keys(updates).length) {
        await Guild.findOneAndUpdate(
            { guildId: guild.id },
            { $set: updates },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );
    }

    let msg;
    if (isTr) {
        msg = `✅ **${created.length}** kanal olusturuldu ve bu sunucuya baglandi.`;
        if (skipped.length) msg += `\n⚠️ Olusturulamayan: ${skipped.join(', ')} (elle /kanalayarla ile ayarla)`;
        if (!created.length) msg = '❌ Hic kanal olusturulamadi. Botun bu sunucuda kanal acma yetkisi olmali.';
    } else {
        msg = `✅ **${created.length}** channels created and linked to this server.`;
        if (skipped.length) msg += `\n⚠️ Could not create: ${skipped.join(', ')} (set manually with /setchannel)`;
        if (!created.length) msg = '❌ No channels could be created. The bot needs channel permission in this server.';
    }
    await interaction.editReply(msg);
}

async function close(interaction) {
    await interaction.deferReply();
    const guild = interaction.guild;
    const gRec = await Guild.findOne({ guildId: guild.id });
    const isTr = gRec ? gRec.lang === 'tr' : true;

    if (!gRec) {
        return interaction.editReply(isTr ? '❌ Bu sunucu icin kayit yok.' : '❌ No record for this server.');
    }

    const cleared = [];
    const failed = [];
    const updates = {};

    for (const f of CHANNEL_FIELDS) {
        const id = gRec[f];
        if (!id) continue;
        try {
            const ch = await guild.channels.fetch(id);
            if (ch) await ch.delete('RakSAMP /kurulum kapat');
            cleared.push(f);
        } catch (e) {
            failed.push(f); // kanal zaten silinmis olabilir — DB yine de temizlenir
        }
        updates[f] = null;
    }

    if (Object.keys(updates).length) {
        await Guild.findOneAndUpdate({ guildId: guild.id }, { $set: updates });
    }

    // Kategori bossa dustuyse sil.
    try {
        const all = await guild.channels.fetch();
        const cat = [...all.values()].find(c => c && c.type === ChannelType.GuildCategory && c.name === CATEGORY);
        if (cat) {
            const kids = [...all.values()].filter(c => c && c.parentId === cat.id);
            if (!kids.length) await cat.delete('RakSAMP /kurulum kapat');
        }
    } catch (e) {}

    let msg;
    if (isTr) {
        msg = `✅ **${cleared.length}** kanal silindi ve DB den temizlendi.`;
        if (failed.length) msg += `\n⚠️ Discord da bulunamayan (DB temizlendi): ${failed.join(', ')}`;
        if (!Object.keys(updates).length) msg = 'ℹ️ DB de kayitli kanal yok, silinecek bir sey yok.';
    } else {
        msg = `✅ **${cleared.length}** channels deleted and cleared from the DB.`;
        if (failed.length) msg += `\n⚠️ Not found on Discord (DB cleared): ${failed.join(', ')}`;
        if (!Object.keys(updates).length) msg = 'ℹ️ No channels in the DB, nothing to delete.';
    }
    await interaction.editReply(msg);
}