const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const crypto = require('crypto');
const { Client, GatewayIntentBits, ActionRowBuilder, ButtonBuilder, ButtonStyle, EmbedBuilder, StringSelectMenuBuilder, ChannelType, ModalBuilder, TextInputBuilder, TextInputStyle } = require('discord.js');

const config = require('./config');
const License = require('./models').License;
const Guild = require('./models').Guild;
const guildUtils = require('./utils/guildUtils');
const { hasGroupPermission } = require('./utils/permissions');
const socketStore = require('./utils/socketStore');
const { loadCommands, registerCommands, registerForGuild, handleInteraction } = require('./handlers/commandHandler');
const db = require('./db');

// Dil yardimcisi: botun oyun ici diline gore metin sec (tr -> Turkce, diger -> EN).
const L = (guild, tr, en) => (guild && guild.lang === 'tr') ? tr : en;

// Windows konsolunda emoji/Turkce duzgun gorunsun
if(process.platform==='win32'){ try{ require('child_process').execSync('chcp 65001', {stdio:'ignore'}); }catch{} }

// --- HWID ONAY YAPILANDIRMASI (yalnızca sahibin sunucusu) ---
const OWNER_ID = '1065352859195015260';          // sadece bu kullanıcı onaylayıp reddedebilir
const OWNER_GUILD_ID = '1079118636729520139';    // butonlar bu sunucuda anlamlı
const OWNER_LOG_CHANNEL_ID = '1537910799287459880'; // bildirimler bu kanala otomatik düşer

// --- GROLE/GRANK YETKI KONTROLU (DB -> Guild.allowedRoleId) ---
// Kapi mantigi tek yerde: utils/permissions.js (komutlar da ayni fonksiyonu kullanir).
async function hasGrolePermission(interaction){
    try{
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        return hasGroupPermission(interaction, guild);
    }catch{ return true; }
}

// --- EXPRESS & SOCKET.IO SETUP ---
const app = express();
app.use(express.json());
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

// --- MYSQL (db.js shim) ---
require('./db').init().then(() => console.log(`✔ Veri katmanı hazır (DB=${config.DB})`)).catch(err => console.error(err));

// --- DISCORD BOT SETUP ---
const client = new Client({
    intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildMessages, GatewayIntentBits.MessageContent]
});

client.once('clientReady', async () => {
    loadCommands();
    await registerCommands(client);
    client.user.setPresence({
        status: 'online',
        activities: [{ name: 'RakSAMP', type: 0 }] // type 0 = "Playing" (oynuyor)
    });
    console.log(`🤖 Discord Bot Aktif: ${client.user.tag}`);
    // Kanal olusturma SADECE /kurulum create ile yapilir (otomatik yok).
});

// Yeni sunucuya eklendiğinde komutları o sunucuya kaydet (kanal oluşturma yok; /kurulum ile)
client.on('guildCreate', async (guild) => {
    const data = loadCommands().map(c => c.data.toJSON());
    await registerForGuild(guild, data);
});

client.on('interactionCreate', async (interaction) => {
    // /grole interaktif: uye secimi -> rol secimi (DB izinli rol kontrolu)
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grole_member_select:')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const requesterId = interaction.customId.split(':')[1];
        const selectedMember = interaction.values[0];
        if (selectedMember === '__none__') return interaction.reply({ content: L(gG, '❌ Üye bulunamadı.', '❌ Member not found.'), ephemeral: true });
        const cache = groleMemberCache.get(requesterId);
        groleSelections.set(interaction.user.id, { target: selectedMember, guildId: interaction.guildId, hwid: cache ? cache.hwid : '' });
        const roleSelect = new StringSelectMenuBuilder()
            .setCustomId(`grole_role_select:${requesterId}:${encodeURIComponent(selectedMember)}`)
            .setPlaceholder(L(gG, 'Rol seç...', 'Select role...'))
            .addOptions([
                { label: 'Member', value: 'Member' },
                { label: 'Co-Leader', value: 'Co-Leader' },
            ]);
        const row = new ActionRowBuilder().addComponents(roleSelect);
        const embed = new EmbedBuilder()
            .setTitle(`👤 ${selectedMember}`)
            .setDescription(L(gG, 'Bu üyeye vermek istediğin rolü seç:', 'Select the role you want to give this member:'))
            .setColor(0x2ECC71);
        return interaction.update({ embeds: [embed], components: [row] });
    }
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grole_role_select:')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const parts = interaction.customId.split(':');
        const requesterId = parts[1];
        const member = decodeURIComponent(parts[2] || '');
        const role = interaction.values[0];
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.update({ content: L(guild, '❌ Bağlı RakSAMP istemcisi yok.', '❌ No linked RakSAMP client.'), embeds: [], components: [] });
        }
        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'grole', target: member, role, requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);
        // 2 dk sonra otomatik kapat
        setTimeout(() => { interaction.editReply({ content: L(guild, '✅ İşlem tamamlandı.', '✅ Done.'), embeds: [], components: [] }).catch(()=>{}); removePendingInteraction(interaction.id); }, 2*60*1000);
        const embed = new EmbedBuilder()
            .setTitle('✅ ' + L(guild, 'Gönderildi', 'Sent'))
            .setDescription(L(guild, `\`/grole ${member} → ${role}\` oyuna iletildi — listede aranıp uygulanacak.`, `\`/grole ${member} → ${role}\` sent to the game — will be searched and applied.`))
            .setColor(0x00FF00);
        return interaction.update({ embeds: [embed], components: [] });
    }
    // Sayfali uye listesi butonlari (rate-limit dostu: tek edit ile sayfa degisir)
    if (interaction.isButton() && interaction.customId.startsWith('grole_page_')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const [groleBtn, requesterId, pageStr] = interaction.customId.split(':');
        const dir = groleBtn.replace('grole_page_', '');
        const cache = groleMemberCache.get(requesterId);
        if (!cache) return interaction.reply({ content: L(gG, '❌ Liste süresi doldu, yeniden /grole çalıştır.', '❌ List expired, run /grole again.'), ephemeral: true });
        let page = parseInt(pageStr, 10) || 0;
        if (dir === 'prev') page = Math.max(0, page - 1);
        else if (dir === 'next') page = page + 1;
        grolePageCache.set(requesterId, page);
        const msg = await buildGroleMemberMessage(requesterId, page);
        if (!msg) return interaction.reply({ content: L(gG, '❌ Liste bulunamadı.', '❌ List not found.'), ephemeral: true });
        return interaction.update(msg);
    }
    if (interaction.isButton() && interaction.customId.startsWith('grank_page_')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const [grankBtn, requesterId, pageStr] = interaction.customId.split(':');
        const dir = grankBtn.replace('grank_page_', '');
        const cache = grankMemberCache.get(requesterId);
        if (!cache) return interaction.reply({ content: L(gG, '❌ Liste süresi doldu, yeniden /grank çalıştır.', '❌ List expired, run /grank again.'), ephemeral: true });
        let page = parseInt(pageStr, 10) || 0;
        if (dir === 'prev') page = Math.max(0, page - 1);
        else if (dir === 'next') page = page + 1;
        grankPageCache.set(requesterId, page);
        const msg = await buildGrankMemberMessage(requesterId, page);
        if (!msg) return interaction.reply({ content: L(gG, '❌ Liste bulunamadı.', '❌ List not found.'), ephemeral: true });
        return interaction.update(msg);
    }
    // /grank interaktif: uye secimi -> rank secimi (modal veya select)
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grank_member_select:')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const requesterId = interaction.customId.split(':')[1];
        const selectedMember = interaction.values[0];
        if (selectedMember === '__none__') return interaction.reply({ content: L(gG, '❌ Üye bulunamadı.', '❌ Member not found.'), ephemeral: true });
        const rankSelect = new StringSelectMenuBuilder()
            .setCustomId(`grank_rank_select:${requesterId}:${encodeURIComponent(selectedMember)}`)
            .setPlaceholder(L(gG, 'Rank seç...', 'Select rank...'))
            .addOptions([
                { label: 'Member', value: 'Member' },
                { label: 'Co-Leader', value: 'Co-Leader' },
                { label: 'Leader', value: 'Leader' },
                { label: 'Elder', value: 'Elder' },
                { label: 'Veteran', value: 'Veteran' },
                { label: 'Recruit', value: 'Recruit' },
            ]);
        const row = new ActionRowBuilder().addComponents(rankSelect);
        const embed = new EmbedBuilder()
            .setTitle(`👤 ${selectedMember}`)
            .setDescription(L(gG, 'Bu üyeye vermek istediğin rankı seç (2 dk içinde kapanır):', 'Select the rank to give this member (closes in 2 min):'))
            .setColor(0xE67E22);
        return interaction.update({ embeds: [embed], components: [row] });
    }
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grank_rank_select:')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const parts = interaction.customId.split(':');
        const requesterId = parts[1];
        const member = decodeURIComponent(parts[2] || '');
        const rank = interaction.values[0];
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.update({ content: L(guild, '❌ Bağlı RakSAMP istemcisi yok.', '❌ No linked RakSAMP client.'), embeds: [], components: [] });
        }
        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'grank', target: member, rank, requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);
        setTimeout(() => { interaction.editReply({ content: L(guild, '✅ İşlem tamamlandı.', '✅ Done.'), embeds: [], components: [] }).catch(()=>{}); removePendingInteraction(interaction.id); }, 2*60*1000);
        const embed = new EmbedBuilder()
            .setTitle('✅ ' + L(guild, 'Gönderildi', 'Sent'))
            .setDescription(L(guild, `\`/grank ${member} → ${rank}\` oyuna iletildi.`, `\`/grank ${member} → ${rank}\` sent to the game.`))
            .setColor(0x00FF00);
        return interaction.update({ embeds: [embed], components: [] });
    }
    // /gkick interaktif: uye secimi -> sebep modal
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('gkick_member_select:')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const requesterId = interaction.customId.split(':')[1];
        const selectedMember = interaction.values[0];
        if (selectedMember === '__none__') return interaction.reply({ content: L(gG, '❌ Üye bulunamadı.', '❌ Member not found.'), ephemeral: true });
        const modal = new ModalBuilder()
            .setCustomId(`gkick_reason_modal:${requesterId}:${encodeURIComponent(selectedMember)}`)
            .setTitle(L(gG, 'Kick Sebebi', 'Kick Reason'));
        const input = new TextInputBuilder()
            .setCustomId('gkick_reason_input')
            .setLabel(L(gG, 'Sebep (bos birakabilirsin)', 'Reason (may leave empty)'))
            .setStyle(TextInputStyle.Short)
            .setRequired(false)
            .setMaxLength(120);
        modal.addComponents(new ActionRowBuilder().addComponents(input));
        return interaction.showModal(modal);
    }
    if (interaction.isModalSubmit() && interaction.customId.startsWith('gkick_reason_modal:')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const parts = interaction.customId.split(':');
        const requesterId = parts[1];
        const member = decodeURIComponent(parts[2] || '');
        const reason = (interaction.fields.getTextInputValue('gkick_reason_input') || '').slice(0, 120);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({ content: L(guild, '❌ Bağlı RakSAMP istemcisi yok.', '❌ No linked RakSAMP client.'), ephemeral: true });
        }
        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'gkick', target: member, reason, requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);
        // Secim menusunu kapat (orijinal pending uzerinden)
        try {
            const orig = getPendingInteraction(requesterId);
            if (orig) await orig.editReply({ content: L(guild, `✅ \`${member}\` seçildi, kick iletiliyor...`, `✅ \`${member}\` selected, kicking...`), embeds: [], components: [] }).catch(()=>{});
            removePendingInteraction(requesterId);
        } catch {}
        setTimeout(() => { interaction.editReply({ content: L(guild, '✅ İşlem tamamlandı.', '✅ Done.'), embeds: [], components: [] }).catch(()=>{}); removePendingInteraction(interaction.id); }, 2*60*1000);
        const embed = new EmbedBuilder()
            .setTitle('✅ ' + L(guild, 'Gönderildi', 'Sent'))
            .setDescription(L(guild, `\`/gkick ${member}\` oyuna iletildi${reason ? ` (sebep: ${reason})` : ' (sebepsiz)'}.`, `\`/gkick ${member}\` sent to the game${reason ? ` (reason: ${reason})` : ' (no reason)'}.`))
            .setColor(0x00FF00);
        return interaction.reply({ embeds: [embed], ephemeral: true });
    }
    // Sayfali kick uye listesi butonlari
    if (interaction.isButton() && interaction.customId.startsWith('gkick_page_')) {
        const gG = await Guild.findOne({ guildId: interaction.guildId });
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: L(gG, '❌ Bu menüyü kullanmaya yetkin yok.', '❌ You are not allowed to use this menu.'), ephemeral: true });
        const [gkickBtn, requesterId, pageStr] = interaction.customId.split(':');
        const dir = gkickBtn.replace('gkick_page_', '');
        const cache = gkickMemberCache.get(requesterId);
        if (!cache) return interaction.reply({ content: L(gG, '❌ Liste süresi doldu, yeniden /gkick çalıştır.', '❌ List expired, run /gkick again.'), ephemeral: true });
        let page = parseInt(pageStr, 10) || 0;
        if (dir === 'prev') page = Math.max(0, page - 1);
        else if (dir === 'next') page = page + 1;
        gkickPageCache.set(requesterId, page);
        const msg = await buildGkickMemberMessage(requesterId, page);
        if (!msg) return interaction.reply({ content: L(gG, '❌ Liste bulunamadı.', '❌ List not found.'), ephemeral: true });
        return interaction.update(msg);
    }
    // Lisans Onaylama Butonları (yalnızca sahip; başkası basarsa uyar)
    if (interaction.isButton() && (interaction.customId.startsWith('approve_') || interaction.customId.startsWith('reject_'))) {
        if (interaction.user.id !== OWNER_ID || interaction.guildId !== OWNER_GUILD_ID) {
            return interaction.reply({ content: 'Bu butonları yalnızca sunucu sahibi kullanabilir.', ephemeral: true });
        }
        const hwid = interaction.customId.replace(/^(approve|reject)_/, '');

        if (interaction.customId.startsWith('approve_')) {
            await License.findOneAndUpdate({ hwid }, { isApproved: true }, { upsert: true });
            const embed = new EmbedBuilder()
                .setTitle("✅ HWID Lisansı Onaylandı")
                .setDescription(`**HWID:** \`${hwid}\` cihazına erişim izni verildi.`)
                .setColor(0x00FF00)
                .setTimestamp();
            await interaction.update({ embeds: [embed], components: [] });
        } else {
            await License.findOneAndUpdate({ hwid }, { isApproved: false }, {});
            const embed = new EmbedBuilder()
                .setTitle("❌ HWID Lisansı Reddedildi")
                .setDescription(`**HWID:** \`${hwid}\` cihazına erişim izni verilmedi.`)
                .setColor(0xFF0000)
                .setTimestamp();
            await interaction.update({ embeds: [embed], components: [] });
        }
        return;
    }

    await handleInteraction(interaction);
});

client.login(config.DISCORD_BOT_TOKEN).catch(console.error);

// --- YARDIMCI FONKSİYONLAR ---

// hwid -> oyun içine iletilecek bekleyen Discord mesajları (geçici, bellekte)
const groupChatOutbox = new Map();

function queueOutbox(hwid, payload) {
    if (!hwid) return;
    const list = groupChatOutbox.get(hwid) || [];
    if (list.length >= 50) list.shift(); // taşmayı önle
    list.push(payload);
    groupChatOutbox.set(hwid, list);
}

// Ortak kanal mesajı gönderim yardımcısı (slash komutlar da kullanır)
const { setClient, sendToGuildChannel } = require('./utils/discordNotifier');
setClient(client);

// RakSAMP isteğini bağlı Discord sunucusuna eşle
async function resolveGuildForRakSAMP(body) {
    if (body.groupId) return guildUtils.getGuild(body.groupId);

    let hwid = body.hwid || null;
    if (!hwid && body.sessionToken) {
        try {
            hwid = decryptSession(body.sessionToken).hwid;
        } catch (e) {}
    }
    if (hwid) return guildUtils.getGuildByHwid(hwid);

    return null;
}

// HWID lisans bildirimi — sahibin sabit log kanalına butonlu embed atar (otomatik).
// Kategori: "<grup ismi> - Logs/Logları" (bot diline gore) sahip sunucusunda olusturulur
// ve sabit log kanali o kategorinin icine tasinir. (Metin Turkce sabit; lokalizasyon yok.)
async function notifyUnapprovedDevice(hwid) {
    try {
        // Grup adi + dil: once hwid'e bagli guild, yoksa sahip guild kaydindan.
        const guildRec = (await guildUtils.getGuildByHwid(hwid)) || (await guildUtils.getGuild(OWNER_GUILD_ID));
        const isTr = guildRec ? guildRec.lang === 'tr' : true;
        const groupName = guildRec && guildRec.groupName ? String(guildRec.groupName).trim() : '';

        const ownerGuild = await client.guilds.fetch(OWNER_GUILD_ID);
        if (!ownerGuild) {
            console.log(`⚠️ Sahip sunucusu (${OWNER_GUILD_ID}) bulunamadi, bildirim atlaniyor.`);
            return;
        }

        // Kategori: "<grup ismi> - Logları" (TR) / "<grup ismi> - Logs" (EN)
        const catName = `${groupName ? groupName + ' - ' : ''}${isTr ? 'Logları' : 'Logs'}`;
        let category = ownerGuild.channels.cache.find(ch => ch.type === ChannelType.GuildCategory && ch.name === catName);
        if (!category) {
            category = await ownerGuild.channels.create({ name: catName, type: ChannelType.GuildCategory });
            console.log(`📂 Kategori olusturuldu: ${catName}`);
        }

        // Sabit log kanali: kategori icine tasi (zaten icindeyse dokunma)
        const channel = await client.channels.fetch(OWNER_LOG_CHANNEL_ID);
        if (!channel) {
            console.log(`⚠️ Lisanssız cihaz (HWID: ${hwid}) — log kanalı (${OWNER_LOG_CHANNEL_ID}) bulunamadı, bildirim atlanıyor.`);
            return;
        }
        if (channel.parentId !== category.id) {
            await channel.setParent(category.id, { lockPermissions: false }).catch(() => {});
        }

        const row = new ActionRowBuilder().addComponents(
            new ButtonBuilder()
                .setCustomId(`approve_${hwid}`)
                .setLabel('✅ Lisans Onayla')
                .setStyle(ButtonStyle.Success),
            new ButtonBuilder()
                .setCustomId(`reject_${hwid}`)
                .setLabel('❌ Reddet')
                .setStyle(ButtonStyle.Danger)
        );

        const embed = new EmbedBuilder()
            .setTitle("⚠️ Lisanssız Cihaz Girişi Belirlendi")
            .setDescription(`Yetkisiz bir RakSAMP istemcisi bağlanmaya çalıştı.\n\n**HWID:** \`${hwid}\``)
            .setColor(0xFF0000)
            .setTimestamp();

        await channel.send({ embeds: [embed], components: [row] });
    } catch (e) {
        console.error("Discord bildirim hatası:", e);
    }
}

// Session token'dan hwid çöz (AES)
function decryptSession(sessionToken) {
    const [ivHex, encrypted] = sessionToken.split(':');
    const iv = Buffer.from(ivHex, 'hex');
    const decipher = crypto.createDecipheriv('aes-256-cbc', config.AES_SECRET_KEY, iv);
    let decrypted = decipher.update(encrypted, 'hex', 'utf8');
    decrypted += decipher.final('utf8');
    return JSON.parse(decrypted);
}

// --- REST API: HANDSHAKE & SECURITY ---
app.post('/api/handshake', async (req, res) => {
    const { hwid } = req.body;

    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    let record = await License.findOne({ hwid });

    if (!record || !record.isApproved) {
        if (!record) {
            await License.create({ hwid, isApproved: false });
        }

        await notifyUnapprovedDevice(hwid);

        return res.status(403).json({ success: false, message: "Cihaz lisanslı değil veya onay bekliyor." });
    }

    // Lisanslı Cihaz İçin Güvenli Oturum Token'ı Üret (AES)
    const sessionData = JSON.stringify({ hwid, timestamp: Date.now() });
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipheriv('aes-256-cbc', config.AES_SECRET_KEY, iv);
    let encrypted = cipher.update(sessionData, 'utf8', 'hex');
    encrypted += cipher.final('hex');
    const sessionToken = iv.toString('hex') + ':' + encrypted;

    return res.json({
        success: true,
        sessionToken,
        config: {
            commandPrefix: "?"
        }
    });
});

// RakSAMP /gstats verisi geldiğinde bağlı sunucunun gstats kanalına embed olarak yaz.
// requesterId geldiyse sonuç, komutu çalıştıran Discord kullanıcısına reply olarak gönderilir
// (kanala ayrıca atılmaz).
app.post('/api/gstats', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    // SAMP renk kodlarını ({RRGGBB} gibi) temizle
    const stripColor = (t) => String(t || '').replace(/\{[0-9a-fA-F]{6}\}/g, '').trim();
    const statsText = stripColor(body.statsText);
    const groupName = body.groupName || '';
    const playerName = body.playerName || 'Bir oyuncu';
    const requesterId = body.requesterId || null;

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const channelId = guild.gstatsChannelId || null;
    console.log(`📊 GStats (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${statsText.slice(0, 200)}`);

    // Önce interaction'a reply dene
    let replied = false;
    if (requesterId) {
        const interaction = getPendingInteraction(requesterId);
        if (interaction) {
            try {
                const embed = new EmbedBuilder()
                    .setTitle("📊 Group Stats")
                    .setDescription(statsText.slice(0, 3000))
                    .setColor(0x3498DB)
                    .setTimestamp();
                if (groupName) embed.setAuthor({ name: groupName });
                if (playerName) embed.setFooter({ text: L(guild, `${playerName} tarafından istendi`, `Requested by ${playerName}`) });
                await interaction.editReply({ embeds: [embed] });
                removePendingInteraction(requesterId);
                replied = true;
            } catch (e) {
                console.error("GStats interaction reply hatası:", e);
            }
        }
    }

    if (!replied && channelId) {
        try {
            const channel = await client.channels.fetch(channelId);
            if (channel) {
                const embed = new EmbedBuilder()
                    .setTitle("📊 Group Stats")
                    .setDescription(statsText.slice(0, 3000))
                    .setColor(0x3498DB)
                    .setTimestamp();

                if (groupName) embed.setAuthor({ name: groupName });
                if (playerName) embed.setFooter({ text: L(guild, `${playerName} tarafından istendi`, `Requested by ${playerName}`) });

                await channel.send({ embeds: [embed] });
            }
        } catch (e) {
            console.error("GStats Discord gönderim hatası:", e);
        }
    }

    return res.json({ success: true, channelId, replied });
});

// RakSAMP /admins verisi geldiğinde bağlı sunucunun kanalına embed olarak yaz.
// requesterId geldiyse sonuç, komutu çalıştıran Discord kullanıcısına reply olarak gönderilir.
app.post('/api/admins', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    const stripColor = (t) => String(t || '').replace(/\{[0-9a-fA-F]{6}\}/g, '').trim();
    const adminsText = stripColor(body.adminsText);
    const playerName = body.playerName || 'Bir oyuncu';
    const requesterId = body.requesterId || null;

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    console.log(`🛡️ Admins (guild=${guild.guildId}): ${adminsText.slice(0, 200)}`);

    let replied = false;
    if (requesterId) {
        const interaction = getPendingInteraction(requesterId);
        if (interaction) {
            try {
                const embed = new EmbedBuilder()
                    .setTitle("🛡️ Online Adminler")
                    .setDescription(adminsText.slice(0, 3000))
                    .setColor(0xE67E22)
                    .setTimestamp();
                if (playerName) embed.setFooter({ text: `${playerName} tarafından istendi` });
                await interaction.editReply({ embeds: [embed] });
                removePendingInteraction(requesterId);
                replied = true;
            } catch (e) {
                console.error("Admins interaction reply hatası:", e);
            }
        }
    }

    if (!replied && guild.logChannelId) {
        try {
            const channel = await client.channels.fetch(guild.logChannelId);
            if (channel) {
                const embed = new EmbedBuilder()
                    .setTitle("🛡️ Online Adminler")
                    .setDescription(adminsText.slice(0, 3000))
                    .setColor(0xE67E22)
                    .setTimestamp();
                if (playerName) embed.setFooter({ text: `${playerName} tarafından istendi` });
                await channel.send({ embeds: [embed] });
            }
        } catch (e) {
            console.error("Admins Discord gönderim hatası:", e);
        }
    }

    return res.json({ success: true, replied });
});

// Oyun içi ?w (War Help) — Discord'a @here ile yardım çağrısı, bot diline gore,
// düşman grup dahil (biliniyorsa).
app.post('/api/warhelp', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const isTr = guild.lang === 'tr';
    const playerName = body.playerName || 'Bir oyuncu';
    const ownGroup = body.ownGroup || '';
    const enemyGroup = body.enemyGroup || '';

    let text;
    if (isTr) {
        text = ownGroup && enemyGroup
            ? `@here **${playerName}** - Grup bölgesinde yardıma ihtiyacı var\n**${ownGroup}** vs **${enemyGroup}**`
            : `@here **${playerName}** - Grup bölgesinde yardıma ihtiyacı var`;
    } else {
        text = ownGroup && enemyGroup
            ? `@here **${playerName}** - Needs help in group zone\n**${ownGroup}** vs **${enemyGroup}**`
            : `@here **${playerName}** - Needs help in group zone`;
    }

    // Kanal ayarlanmadıysa null → sadece logla
    const channelId = guild.warChannelId || null;
    console.log(`⚔️ WARHELP (guild=${guild.guildId}, kanal=${channelId || 'null'}, dil=${guild.lang}): ${text}`);

    if (channelId) {
        try {
            const channel = await client.channels.fetch(channelId);
            if (channel) {
                await channel.send(text);
            }
        } catch (e) {
            console.error("WarHelp Discord gönderim hatası:", e);
        }
    }

    return res.json({ success: true, channelId });
});

// GZ sonucu / kill list — oyun içi GZ bittiğinde toplanan sonuç.
// Embed: kazandı/kaybetti başlığı + kırmızı/mavi takım + skor + iki kutu + MVP/feeder.
app.post('/api/gmembers/sheet', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }
    const { writeRoster } = require('./sheets');
    const group = String(body.group || '').trim();
    const rows = Array.isArray(body.rows) ? body.rows : [];
    if (!group || !rows.length) {
        return res.json({ success: false, message: "Grup adı veya üye satırları eksik." });
    }
    try {
        const spreadsheetId = body.spreadsheetId || config.GOOGLE_SHEETS.spreadsheetId;
        const result = await writeRoster(spreadsheetId, group, rows, guild.lang === 'tr' ? 'tr' : 'en');
        if (result.skipped) {
            console.log(`📊 GMEMBERS (guild=${guild.guildId}, grup=${group}): degisiklik yok, full rewrite atlandi (${result.rows} uye).`);
        } else {
            console.log(`📊 GMEMBERS (guild=${guild.guildId}, grup=${group}): ${result.rows} üye yazıldı. Tablo: https://docs.google.com/spreadsheets/d/${result.spreadsheetId}`);
        }
        // Bitis bildirimi: kanala 📊 link + last updated (onceki mesaj varsa duzenlenir, DB: roster_messages)
        try {
            // pendingReplies yardimcilari modul basinda (command/poll blogu) alinir.
            const pending = body.requesterId ? getPendingInteraction(String(body.requesterId)) : null;
            if (pending && pending.channelId) {
                const userId = pending.user && pending.user.id ? pending.user.id : null;
                const link = `https://docs.google.com/spreadsheets/d/${result.spreadsheetId}`;
                const text = `📊 **${group} Roster**\n${link}\n${L(guild, 'Son güncelleme', 'Last updated')}: ${result.stamp}` + (userId ? `\n<@${userId}>` : '');
                const channel = await client.channels.fetch(pending.channelId).catch(() => null);
                if (channel && channel.send) {
                    let msgId = null;
                    try {
                        const old = await db.execute('SELECT message_id, channel_id FROM roster_messages WHERE guild_id=? LIMIT 1', [guild.guildId]);
                        if (old && old[0] && old[0].message_id) {
                            const oldCh = old[0].channel_id ? await client.channels.fetch(old[0].channel_id).catch(() => null) : null;
                            const oldMsg = (oldCh && oldCh.messages) ? await oldCh.messages.fetch(old[0].message_id).catch(() => null) : null;
                            if (oldMsg) { await oldMsg.edit(text); msgId = oldMsg.id; }
                        }
                    } catch {}
                    if (!msgId) {
                        const sent = await channel.send(text);
                        msgId = sent.id;
                    }
                    await db.execute(
                        `INSERT INTO roster_messages (guild_id, channel_id, message_id, spreadsheet_id, member_count, vip_count, updated_at)
                         VALUES (?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE channel_id=VALUES(channel_id), message_id=VALUES(message_id),
                         spreadsheet_id=VALUES(spreadsheet_id), member_count=VALUES(member_count), vip_count=VALUES(vip_count), updated_at=VALUES(updated_at)`,
                        [guild.guildId, channel.id, msgId, result.spreadsheetId, result.rows, result.vipCount || 0, new Date()]
                    );
                }
            }
            if (body.requesterId) removePendingInteraction(String(body.requesterId));
        } catch (e) {
            console.error('GMEMBERS discord mesaj hatasi:', e.message);
        }
        return res.json({ success: true, rows: result.rows, spreadsheetId: result.spreadsheetId, skipped: false });
    } catch (err) {
        console.error(`📊 GMEMBERS hatası (grup=${group}): ${err.message}`);
        return res.json({ success: false, message: err.message });
    }
});

// ?gmembers oncesi: tabloda kayitli account ID'ler (client /astats atlamasi icin)
app.get('/api/gmembers/known', async (req, res) => {
    const hwid = String(req.query.hwid || '').trim();
    const group = String(req.query.group || '').trim();
    if (!hwid || !group) return res.json({ success: false, known: {} });
    const guild = await guildUtils.getGuildByHwid(hwid);
    if (!guild) return res.json({ success: false, known: {} });
    try {
        const { readRosterAccounts } = require('./sheets');
        const spreadsheetId = req.query.spreadsheetId || config.GOOGLE_SHEETS.spreadsheetId;
        const result = await readRosterAccounts(spreadsheetId, group);
        return res.json({ success: true, known: result.known });
    } catch (err) {
        return res.json({ success: false, known: {} });
    }
});

app.post('/api/killlist', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }
    const isTr = guild.lang === 'tr';

    const won = Number(body.won) === 1;
    const redName = String(body.redName || 'Kırmızı').trim();
    const blueName = String(body.blueName || 'Mavi').trim();
    // "~"/"NULL"/"-" = UIF'te tag'siz grup sentinel'i; isim yerine "~" basilmamali.
    let redTag = String(body.redTag || '').trim();
    let blueTag = String(body.blueTag || '').trim();
    if (redTag === '~' || redTag === '-' || redTag.toUpperCase() === 'NULL') redTag = '';
    if (blueTag === '~' || blueTag === '-' || blueTag.toUpperCase() === 'NULL') blueTag = '';
    const redKills = Number(body.redKills) || 0;
    const blueKills = Number(body.blueKills) || 0;
    const redBox = String(body.redBox || '').trim();
    const blueBox = String(body.blueBox || '').trim();
    const mvp = String(body.mvp || '?').trim();
    const feeder = String(body.feeder || '?').trim();

const mvpDmg = Number(body.mvpDmg) || 0;
    const mvpKills = Number(body.mvpKills) || 0;
    const feederDeaths = Number(body.feederDeaths) || 0;
    const firstBlood = String(body.firstBlood || '').trim();
    const topWeapons = String(body.topWeapons || '').trim();

    if (!redBox && !blueBox) {
        return res.json({ success: false, message: "Kill list boş." });
    }

    const t = {
        wonTitle: isTr ? '🏆 Grup Zone Savaşı Kazanıldı' : '🏆 Group Zone War Won',
        lostTitle: isTr ? '💀 Grup Zone Savaşı Kaybedildi' : '💀 Group Zone War Lost',
        scoreKills: isTr ? 'Skor-Kills' : 'Score-Kills',
        mvp: '⭐ MVP',
        feeder: '🎯 Top Feeder',
        firstBlood: isTr ? '🩸 İlk Kan' : '🩸 First Blood',
        topWeapons: isTr ? '🔫 En Çok Kullanılan Silahlar' : '🔫 Top Weapons',
    
    };

    // Veri yoksa alan GÖSTERILMEZ (MVP '?' / feeder '?' / firstBlood-topWeapons bos)
    const fields = [
        { name: `⚔️ **${redTag || redName}**`, value: `\`\`\`\n${redBox}\n\`\`\`` },
        { name: `⚔️ **${blueTag || blueName}**`, value: `\`\`\`\n${blueBox}\n\`\`\`` }
    ];
    if (mvp && mvp !== '?') fields.push({ name: t.mvp, value: `${mvp} (${mvpDmg} dmg, ${mvpKills} kills)`, inline: true });
    if (feeder && feeder !== '?') fields.push({ name: t.feeder, value: `${feeder} (${feederDeaths} deaths)`, inline: true });
    if (firstBlood) fields.push({ name: t.firstBlood, value: `${firstBlood}`, inline: true });
    if (topWeapons) fields.push({ name: t.topWeapons, value: `${topWeapons}`, inline: true });

    const embed = new EmbedBuilder()
        .setTitle(won ? t.wonTitle : t.lostTitle)
        .setDescription(`${redName} vs **${blueName}**\n*${isTr ? 'Grup zone savaşı' : 'Group zone war'}*\n${t.scoreKills}: ${redTag || redName}:${redKills} ${blueTag || blueName}:${blueKills}`)
        .setColor(won ? 0x00FF00 : 0xFF0000)
        .setTimestamp()
        .addFields(fields);

    const channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', { embed });
    console.log(`⚔️ KILLLIST (guild=${guild.guildId}, kanal=${channelId || 'null'}, dil=${guild.lang}): ${won ? 'WIN' : 'LOSE'} ${redName} vs ${blueName} (${redKills}-${blueKills})`);
    return res.json({ success: true, channelId });
});

// Oyun içinden /pm yaniti — Discord'da /pm'i kullanan kisiye etiketli + ephemeral gosterilir.
// /pm gonderimi sonrasi: client hedefin GERCEK oyun ici adini cozduyse Discord'daki
// ilk embed'i o adla gunceller (Discord'da ID yazilsa da "cb650r" gosterir).
app.post('/api/pm/sent', async (req, res) => {
    const body = req.body || {};
    const requesterId = String(body.requesterId || '');
    const interaction = requesterId ? getPendingInteraction(requesterId) : null;
    if (interaction) {
        const targetName = String(body.targetName || body.target || 'oyuncu').slice(0, 64);
        const message = String(body.message || '').slice(0, 200);
        const isTr = (await Guild.findOne({ guildId: interaction.guildId }))?.lang === 'tr';
        try {
            const embed = new EmbedBuilder()
                .setTitle(isTr ? '📨 Mesaj Gönderildi' : '📨 Message Sent')
                .setDescription(isTr
                    ? `\`${targetName}\` oyuncusuna: ${message}`
                    : `To \`${targetName}\`: ${message}`)
                .setColor(0x00FF00)
                .setFooter({ text: isTr ? 'Yanıt gelince burada görünür' : 'Replies will appear here' });
            await interaction.editReply({ embeds: [embed] });
            console.log(`📨 PM-SENT (req=${requesterId.slice(0, 8)}): hedef ${targetName}`);
            return res.json({ success: true });
        } catch (e) {
            console.error('PM-SENT edit hatası:', e.message);
        }
    }
    return res.json({ success: false });
});

app.post('/api/pm/reply', async (req, res) => {
    const body = req.body || {};
    const requesterId = String(body.requesterId || '');
    const reply = String(body.reply || '').slice(0, 200);
    const author = String(body.author || 'Oyuncu');
    const interaction = requesterId ? getPendingInteraction(requesterId) : null;
    console.log(`📥 PM-REPLY geldi (req=${String(requesterId || '').slice(0, 12)}, author=${author}, interaction=${interaction ? 'BULUNDU' : 'YOK'})`);
    if (interaction) {
        try {
            const userId = (interaction.user && interaction.user.id) ? interaction.user.id : requesterId;
            const embed = new EmbedBuilder()
                .setTitle('📥 Oyun İçinden Yanıt')
                .setDescription(`<@${userId}> — **${author}**: ${reply}`)
                .setColor(0x3498DB)
                .setTimestamp();
            await interaction.editReply({ embeds: [embed] });
            removePendingInteraction(requesterId);
            console.log(`📥 PM-REPLY (req=${requesterId.slice(0, 8)}): ${author}: ${reply}`);
            return res.json({ success: true });
        } catch (e) {
            console.error('PM-REPLY edit hatası:', e.message);
        }
    }
    return res.json({ success: false });
});

// ?1414 backup çağrısı — backup kanalına @here ile acil yardım
app.post('/api/backup', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const playerName = body.playerName || 'Bir oyuncu';
    const ownGroup = body.ownGroup || '';

    const channelId = await sendToGuildChannel(guild, 'backupChannelId', {
        content: `@here **${ownGroup || 'Grup'} backup istiyor!**\n${playerName} oyun içinde acil destek çağırıyor.`
    });
    console.log(`🆘 BACKUP (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${playerName} (${ownGroup})`);

    return res.json({ success: true, channelId });
});

// arka plan görevleri başlasın

// Admin eylem logu — oyun içi "ADMIN: X banned Y ... (REASON: Z)" yakalaması
// DB'ye kaydedilir. Discord embed yok (kalabalık yapmasın); /alog ile sorgulanır.
app.post('/api/adminlog', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const targetName = String(body.targetName || '').trim().slice(0, 64);
    const action = String(body.action || '').trim().slice(0, 16);
    if (!targetName || !action) {
        return res.json({ success: false, message: "targetName/action zorunlu." });
    }

    await db.execute(
        `INSERT INTO admin_logs (admin_name, target_name, target_id, action, mode, duration_min, reason)
         VALUES (?, ?, ?, ?, ?, ?, ?)`,
        [
            String(body.adminName || '').trim().slice(0, 64),
            targetName,
            body.targetId ? Number(body.targetId) : null,
            action,
            String(body.mode || '').trim().slice(0, 64),
            body.durationMin ? Number(body.durationMin) : null,
            String(body.reason || '').trim().slice(0, 255)
        ]
    );
    console.log(`📝 ADMINLOG (guild=${guild.guildId}): ${body.adminName || '?'} ${action} ${targetName} (mode=${body.mode || '-'}, reason=${body.reason || '-'})`);
    return res.json({ success: true });
});

// Grup rol değişikliği logu — "(Ad)(accid) changed the rank of ..." yakalaması
app.post('/api/grouplog', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    await db.execute(
        `INSERT INTO group_logs (actor_name, actor_acc, target_name, target_acc, old_rank, new_rank)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [
            String(body.actorName || '').trim().slice(0, 64),
            body.actorAcc ? Number(body.actorAcc) : null,
            String(body.targetName || '').trim().slice(0, 64),
            body.targetAcc ? Number(body.targetAcc) : null,
            String(body.oldRank || '').trim().slice(0, 64),
            String(body.newRank || '').trim().slice(0, 64)
        ]
    );
    console.log(`📝 GROUPLOG (guild=${guild.guildId}): ${body.actorName || '?'} ${body.oldRank || '?'} -> ${body.newRank || '?'} (${body.targetName || '?'})`);
    return res.json({ success: true });
});

// ?post — son grup aksiyonu (rol/rank/kick) gerekceyle grup log kanalina embed gonderilir.
// Dil guild.lang'dan (bot dili) alinir: tr -> Turkce, en -> Ingilizce.
app.post('/api/grouppost', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }
    const isTr = (guild.lang === 'tr');
    const actionType = Number(body.actionType) || 0;
    const reason = String(body.reason || '').trim().slice(0, 255);

    const t = {
        title: isTr ? 'Grup Logu' : 'Group Log',
        action: isTr ? 'Aksiyon' : 'Action',
        actionRole: isTr ? 'Rol Değişikliği' : 'Role Change',
        actionRank: isTr ? 'Rank Değişikliği' : 'Rank Change',
        actionKick: isTr ? 'Kick' : 'Kick',
        by: isTr ? 'Yapan' : 'Action taken by',
        on: isTr ? 'Üzerinde' : 'On whom',
        reason: isTr ? 'Gerekçe' : 'Reason',
        noReason: isTr ? 'Gerekçe verilmedi' : 'Reason not given',
        change: isTr ? 'Değişiklik' : 'Change',
    };
    let action;
    if (actionType === 3) action = t.actionKick;
    else if (actionType === 2) action = t.actionRank;
    else action = t.actionRole;

    const groupName = String(guild.groupName || '').trim();
    const embed = new EmbedBuilder()
        .setTitle(`${groupName ? groupName + ' - ' : ''}${t.title}`)
        .setColor(0xFF8C00)
        .addFields(
            { name: t.action, value: action, inline: false },
            { name: t.by, value: `${body.actorName || '?'}${body.actorAcc ? ` (acc ${body.actorAcc})` : ''}`, inline: true },
            { name: t.on, value: `${body.targetName || '?'}${body.targetAcc ? ` (acc ${body.targetAcc})` : ''}`, inline: true }
        )
        .setTimestamp();

    // Kick icin eski/yeni yok; rol/rank icin degisim gosterilir.
    if (actionType !== 3 && (body.oldRank || body.newRank)) {
        embed.addFields({ name: t.change, value: `\`${body.oldRank || '-'}\` → \`${body.newRank || '-'}\``, inline: true });
    }
    embed.addFields({ name: t.reason, value: reason || t.noReason });

    const channelId = await sendToGuildChannel(guild, 'groupLogChannelId', { embed });
    console.log(`📨 GROUPPOST (guild=${guild.guildId}, kanal=${channelId || 'null'}, dil=${guild.lang}): ${body.actorName} ${action} -> ${body.targetName} (${reason || 'gerekce yok'})`);
    return res.json({ success: true, channelId });
});

// ?alog sorgusu — oyuncu adı / id / accid'e göre admin log sayı özeti
app.post('/api/alog', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const q = String(body.q || '').trim();
    if (!q) {
        return res.json({ success: false, message: "Sorgu boş." });
    }

    // isim (büyük/küçük duyarsız parça) veya ID/accid tam eşleşme
    let rows;
    if (/^\d+$/.test(q)) {
        rows = await db.execute(
            `SELECT MAX(target_name) AS target_name, action, mode, COUNT(*) AS cnt
             FROM admin_logs
             WHERE target_id = ?
             GROUP BY action, mode
             ORDER BY cnt DESC, action, mode`,
            [Number(q)]
        );
    } else {
        rows = await db.execute(
            `SELECT action, mode, COUNT(*) AS cnt
             FROM admin_logs
             WHERE LOWER(target_name) LIKE LOWER(?)
             GROUP BY action, mode
             ORDER BY cnt DESC, action, mode`,
            [`%${q}%`]
        );
    }

    if (!rows || rows.length === 0) {
        return res.json({ success: true, summary: L(guild, `Kayıt yok: ${q}`, `No records: ${q}`) });
    }

    // uL.Hold formati: "ALogs for <isim>: B:0 | M-Bans:1 | D:0 | J:0 | K:0 | M-Kicks:0 | Mutes:3 | SubScores:0"
    const cnt = { B: 0, 'M-Bans': 0, D: 0, J: 0, K: 0, 'M-Kicks': 0, Mutes: 0, SubScores: 0 };
    for (const r of rows) {
        const act = r.action || '';
        const mode = (r.mode || '').trim();
        const n = Number(r.cnt) || 0;
        if (act === 'banned') { mode ? cnt['M-Bans'] += n : cnt.B += n; }
        else if (act === 'disarmed') cnt.D += n;
        else if (act === 'jailed') cnt.J += n;
        else if (act === 'kicked') { mode ? cnt['M-Kicks'] += n : cnt.K += n; }
        else if (act === 'muted') cnt.Mutes += n;
        else cnt.SubScores += n;
    }
    let name = q;
    if (/^\d+$/.test(q) && rows.length && rows[0].target_name) name = rows[0].target_name;
    const summary = `ALogs for ${name}: B:${cnt.B} | M-Bans:${cnt['M-Bans']} | D:${cnt.D} | J:${cnt.J} | K:${cnt.K} | M-Kicks:${cnt['M-Kicks']} | Mutes:${cnt.Mutes} | SubScores:${cnt.SubScores}`;
    console.log(`📊 ALOG (guild=${guild.guildId}): q="${q}" -> ${summary}`);
    return res.json({ success: true, summary });
});
app.post('/api/welcome', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const playerName = body.playerName || 'Bir oyuncu';
    const groupName = body.groupName || '';

    // Kişiye özel welcome mesajı (oyun içi ?welcomemsg ile ayarlanır)
    const welcomeMessage = String(body.welcomeMessage || '').trim().slice(0, 300);

    // HWID'e bağlı welcome açık/kapalı durumunu kontrol et (oyun içi ?greet)
    let enabled = true;
    let hwid = body.hwid || null;
    if (hwid) {
        const lic = await License.findOne({ hwid });
        enabled = lic ? lic.welcomeEnabled : true;
    }

    if (!enabled) {
        return res.json({ success: true, enabled: false, message: "Welcome back mesajları kapalı." });
    }

    // Welcome mesajlari Discord'a GONDERILMEZ (oyun ici kalir); istek sadece loglanir.
    console.log(`👋 WELCOME (guild=${guild.guildId}, Discord'a gonderilmiyor): ${playerName}${welcomeMessage ? ' (' + welcomeMessage.slice(0, 60) + ')' : ''}`);

    return res.json({ success: true, enabled: true, channelId: null });
});

// ?greet — welcome back mesajlarını aç/kapat (kişiye özel, HWID bazlı)
app.post('/api/welcome/toggle', async (req, res) => {
    const body = req.body || {};
    const hwid = body.hwid || null;

    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    let lic = await License.findOne({ hwid });
    if (!lic) {
        lic = await License.create({ hwid, isApproved: false });
    }

    lic.welcomeEnabled = !lic.welcomeEnabled;
    await lic.save();

    console.log(`👋 WELCOME TOGGLE (hwid=${hwid}): ${lic.welcomeEnabled ? 'AÇILDI' : 'KAPANDI'}`);
    return res.json({ success: true, enabled: !!lic.welcomeEnabled });
});

// ?welcomemsg <mesaj> — kişiye özel welcome mesajı ayarla (HWID bazlı)
app.post('/api/welcome/message', async (req, res) => {
    const body = req.body || {};
    const hwid = body.hwid || null;
    const welcomeMessage = String(body.welcomeMessage || '').trim().slice(0, 300);

    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    let lic = await License.findOne({ hwid });
    if (!lic) {
        lic = await License.create({ hwid, isApproved: false });
    }

    lic.welcomeMessage = welcomeMessage;
    await lic.save();

    console.log(`✉️ WELCOME MSG (hwid=${hwid}): ${welcomeMessage || '(silindi)'}`);
    return res.json({ success: true, welcomeMessage });
});

// Welcome ayarını çek (client başlangıçta kişisel mesajı yükler)
app.get('/api/welcome/state', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    const lic = await License.findOne({ hwid });
    return res.json({
        success: true,
        welcomeEnabled: lic ? !!lic.welcomeEnabled : true,
        welcomeMessage: lic ? (lic.welcomeMessage || '') : ''
    });
});

// --- Kisi bazli hos geldin (B): hwid + hedef isim -> ozel mesaj ---
app.post('/api/welcome/peruser', async (req, res) => {
    const { hwid, target, message } = req.body || {};
    if (!hwid || !target) return res.status(400).json({ success: false, message: "hwid ve target gerekli" });
    const t = String(target).trim().slice(0, 64);
    const msg = String(message || '').trim().slice(0, 255);
    if (!msg) {
        await db.execute('DELETE FROM welcome_per_user WHERE hwid=? AND target_name=?', [hwid, t]);
        console.log(`WELCOME PER-USER silindi (hwid=${hwid.slice(0,8)}, target=${t})`);
        return res.json({ success: true, deleted: true });
    }
    await db.execute('INSERT INTO welcome_per_user (hwid, target_name, custom_message, updated_at) VALUES (?,?,?,?) ON DUPLICATE KEY UPDATE custom_message=VALUES(custom_message), updated_at=VALUES(updated_at)', [hwid, t, msg, new Date()]);
    console.log(`WELCOME PER-USER (hwid=${hwid.slice(0,8)}, target=${t}): ${msg}`);
    return res.json({ success: true });
});

app.get('/api/welcome/peruser', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    const target = String(req.query.target || '').trim();
    if (!hwid || !target) return res.json({ success: false, message: '' });
    const rows = await db.execute('SELECT custom_message FROM welcome_per_user WHERE hwid=? AND target_name=? LIMIT 1', [hwid, target]);
    const msg = (rows && rows[0] && rows[0].custom_message) ? rows[0].custom_message : '';
    return res.json({ success: true, message: msg });
});

app.get('/api/welcome/peruser/list', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    if (!hwid) return res.json({ success: false, list: [] });
    const rows = await db.execute('SELECT target_name, custom_message FROM welcome_per_user WHERE hwid=? ORDER BY target_name', [hwid]);
    return res.json({ success: true, list: rows || [] });
});

// Discord'dan oyuna komut iletme (client /api/command/poll ile çeker)
const { queueCommand, drainCommands } = require('./utils/commandOutbox');
const { getPendingInteraction, removePendingInteraction } = require('./utils/pendingReplies');

// Komut bekleme kuyruğunu oyundan çek (polling)
app.get('/api/command/poll', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    const commands = drainCommands(hwid);
    return res.json({ commands });
});

// --- /api/grole: Discord'dan grup rol degisikligi (paginated /grole otomasyonu) ---
app.post('/api/grole', async (req, res) => {
    const { hwid, target, role, requesterId } = req.body || {};
    if (!hwid || !target || !role) return res.status(400).json({ error: 'hwid, target, role gerekli' });
    const roleMap = { mem: 'Member', co: 'Co-Leader', memb: 'Member' };
    const fullRole = roleMap[role] || role;
    queueCommand(hwid, { type: 'grole', target, role: fullRole, requesterId: requesterId || '' });
    console.log(`👤 [GROLE] Kuyruğa alındı: ${target} -> ${fullRole} (hwid=${hwid.slice(0,8)})`);
    return res.json({ success: true });
});

// --- /api/grank: Discord'dan grup rank degisikligi (paginated /grank otomasyonu) ---
app.post('/api/grank', async (req, res) => {
    const { hwid, target, rank, requesterId } = req.body || {};
    if (!hwid || !target || !rank) return res.status(400).json({ error: 'hwid, target, rank gerekli' });
    queueCommand(hwid, { type: 'grank', target, rank, requesterId: requesterId || '' });
    console.log(`🏷️ [GRANK] Kuyruğa alındı: ${target} -> ${rank} (hwid=${hwid.slice(0,8)})`);
    return res.json({ success: true });
});

// --- /grole interaktif: uye listesini oyundan al, Discord select menu goster (butonlu sayfalama, rate-limit uyumlu) ---
const groleMemberCache = new Map(); // requesterId -> { members, guildId, hwid }
const groleSelections = new Map(); // userId -> { target, guildId, hwid }
const grolePageCache = new Map(); // requesterId -> page

async function buildGroleMemberMessage(requesterId, page){
    const cache = groleMemberCache.get(requesterId);
    if(!cache) return null;
    const gRec = cache.guildId ? await Guild.findOne({ guildId: cache.guildId }).catch(() => null) : null;
    const members = cache.members;
    const totalPages = Math.max(1, Math.ceil(members.length / 25));
    const p = Math.max(0, Math.min(page, totalPages-1));
    const slice = members.slice(p*25, p*25+25);
    const options = slice.map(m => ({ label: m.slice(0,100), value: m.slice(0,100) }));
    const select = new StringSelectMenuBuilder()
        .setCustomId(`grole_member_select:${requesterId}`)
        .setPlaceholder(L(gRec, `Üye seç (sayfa ${p+1}/${totalPages})`, `Select member (page ${p+1}/${totalPages})`))
        .addOptions(options.length ? options : [{ label: L(gRec, 'Üye bulunamadı', 'Member not found'), value: '__none__' }]);
    const rows = [new ActionRowBuilder().addComponents(select)];
    if(totalPages > 1){
        const prev = new ButtonBuilder().setCustomId(`grole_page_prev:${requesterId}:${p}`).setLabel('◀ ' + L(gRec, 'Önceki', 'Prev')).setStyle(ButtonStyle.Secondary).setDisabled(p===0);
        const next = new ButtonBuilder().setCustomId(`grole_page_next:${requesterId}:${p}`).setLabel(L(gRec, 'Sonraki', 'Next') + ' ▶').setStyle(ButtonStyle.Secondary).setDisabled(p===totalPages-1);
        const info = new ButtonBuilder().setCustomId('grole_page_info').setLabel(`${p+1}/${totalPages}`).setStyle(ButtonStyle.Secondary).setDisabled(true);
        rows.push(new ActionRowBuilder().addComponents(prev, info, next));
    }
    const embed = new EmbedBuilder()
        .setTitle(L(gRec, '👥 Üye Seç', '👥 Select Member'))
        .setDescription(L(gRec, `${members.length} üye — sayfa ${p+1}/${totalPages}. Menü 5 dk sonra kapanır.`, `${members.length} members — page ${p+1}/${totalPages}. Menu closes in 5 min.`))
        .setColor(0x3498DB);
    return { embeds: [embed], components: rows };
}

app.post('/api/grole/list', async (req, res) => {
    const { requesterId, members, hwid, guildId } = req.body || {};
    if (!requesterId || !Array.isArray(members)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    if (!interaction) {
        console.log(`⚠️ [GROLE-LIST] Pending interaction bulunamadi: ${requesterId}`);
        return res.json({ success: false });
    }
    const cleanMembers = members.map(m => String(m).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean).slice(0, 150);
    groleMemberCache.set(requesterId, { members: cleanMembers, hwid: hwid || '', guildId: guildId || '' });
    grolePageCache.set(requesterId, 0);
    console.log(`👥 [GROLE-LIST] ${cleanMembers.length} uye alindi (req=${requesterId.slice(0,6)})`);
    try {
        const msg = await buildGroleMemberMessage(requesterId, 0);
        await interaction.editReply(msg);
        // 5 dk sonra otomatik kapat (rate-limit dostu: tek edit)
        setTimeout(async () => {
            try{
                const still = getPendingInteraction(requesterId);
                if(still) await still.editReply({ content: '⏰ Menü süresi doldu.', embeds: [], components: [] }).catch(()=>{});
                groleMemberCache.delete(requesterId);
                grolePageCache.delete(requesterId);
            }catch{}
        }, 5*60*1000);
    } catch(e){
        console.error('GROLE-LIST edit hatasi:', e.message);
        try{ await interaction.editReply({ content: `❌ Menü gösterilemedi: ${e.message}` }); }catch{}
    }
    return res.json({ success: true });
});

// --- /gkick interaktif: uye listesi (grole gibi) + sebep modal ---
const gkickMemberCache = new Map(); // requesterId -> { members, guildId, hwid }
const gkickPageCache = new Map(); // requesterId -> page

async function buildGkickMemberMessage(requesterId, page){
    const cache = gkickMemberCache.get(requesterId);
    if(!cache) return null;
    const gRec = cache.guildId ? await Guild.findOne({ guildId: cache.guildId }).catch(() => null) : null;
    const members = cache.members;
    const totalPages = Math.max(1, Math.ceil(members.length / 25));
    const p = Math.max(0, Math.min(page, totalPages-1));
    const slice = members.slice(p*25, p*25+25);
    const options = slice.map(m => ({ label: m.slice(0,100), value: m.slice(0,100) }));
    const select = new StringSelectMenuBuilder()
        .setCustomId(`gkick_member_select:${requesterId}`)
        .setPlaceholder(L(gRec, `Üye seç (sayfa ${p+1}/${totalPages})`, `Select member (page ${p+1}/${totalPages})`))
        .addOptions(options.length ? options : [{ label: L(gRec, 'Üye bulunamadı', 'Member not found'), value: '__none__' }]);
    const rows = [new ActionRowBuilder().addComponents(select)];
    if(totalPages > 1){
        const prev = new ButtonBuilder().setCustomId(`gkick_page_prev:${requesterId}:${p}`).setLabel('◀ ' + L(gRec, 'Önceki', 'Prev')).setStyle(ButtonStyle.Secondary).setDisabled(p===0);
        const next = new ButtonBuilder().setCustomId(`gkick_page_next:${requesterId}:${p}`).setLabel(L(gRec, 'Sonraki', 'Next') + ' ▶').setStyle(ButtonStyle.Secondary).setDisabled(p===totalPages-1);
        const info = new ButtonBuilder().setCustomId('gkick_page_info').setLabel(`${p+1}/${totalPages}`).setStyle(ButtonStyle.Secondary).setDisabled(true);
        rows.push(new ActionRowBuilder().addComponents(prev, info, next));
    }
    const embed = new EmbedBuilder()
        .setTitle(L(gRec, '👢 Üye Seç (Kick)', '👢 Select Member (Kick)'))
        .setDescription(L(gRec, `${members.length} üye — sayfa ${p+1}/${totalPages}. Seçimden sonra sebep sorulur.`, `${members.length} members — page ${p+1}/${totalPages}. Reason is asked after selection.`))
        .setColor(0xE74C3C);
    return { embeds: [embed], components: rows };
}

app.post('/api/gkick/list', async (req, res) => {
    const { requesterId, members, hwid, guildId } = req.body || {};
    if (!requesterId || !Array.isArray(members)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    if (!interaction) {
        console.log(`⚠️ [GKICK-LIST] Pending interaction bulunamadi: ${requesterId}`);
        return res.json({ success: false });
    }
    const cleanMembers = members.map(m => String(m).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean).slice(0, 150);
    gkickMemberCache.set(requesterId, { members: cleanMembers, hwid: hwid || '', guildId: guildId || '' });
    gkickPageCache.set(requesterId, 0);
    console.log(`👢 [GKICK-LIST] ${cleanMembers.length} uye alindi (req=${requesterId.slice(0,6)})`);
    try {
        const msg = await buildGkickMemberMessage(requesterId, 0);
        await interaction.editReply(msg);
        // 5 dk sonra otomatik kapat (rate-limit dostu: tek edit)
        setTimeout(async () => {
            try{
                const still = getPendingInteraction(requesterId);
                if(still) await still.editReply({ content: '⏰ Menü süresi doldu.', embeds: [], components: [] }).catch(()=>{});
                gkickMemberCache.delete(requesterId);
                gkickPageCache.delete(requesterId);
            }catch{}
        }, 5*60*1000);
    } catch(e){
        console.error('GKICK-LIST edit hatasi:', e.message);
        try{ await interaction.editReply({ content: `❌ Menü gösterilemedi: ${e.message}` }); }catch{}
    }
    return res.json({ success: true });
});

// --- /grank interaktif: rank listesi (grole gibi) ---
const grankMemberCache = new Map();
const grankPageCache = new Map();
const grankRankCache = new Map(); // requesterId -> ranks

async function buildGrankMemberMessage(requesterId, page){
    const cache = grankMemberCache.get(requesterId);
    if(!cache) return null;
    const gRec = cache.guildId ? await Guild.findOne({ guildId: cache.guildId }).catch(() => null) : null;
    const members = cache.members;
    const totalPages = Math.max(1, Math.ceil(members.length / 25));
    const p = Math.max(0, Math.min(page, totalPages-1));
    const slice = members.slice(p*25, p*25+25);
    const options = slice.map(m => ({ label: m.slice(0,100), value: m.slice(0,100) }));
    const select = new StringSelectMenuBuilder()
        .setCustomId(`grank_member_select:${requesterId}`)
        .setPlaceholder(L(gRec, `Üye seç (sayfa ${p+1}/${totalPages})`, `Select member (page ${p+1}/${totalPages})`))
        .addOptions(options.length ? options : [{ label: L(gRec, 'Üye bulunamadı', 'Member not found'), value: '__none__' }]);
    const rows = [new ActionRowBuilder().addComponents(select)];
    if(totalPages > 1){
        const prev = new ButtonBuilder().setCustomId(`grank_page_prev:${requesterId}:${p}`).setLabel('◀ ' + L(gRec, 'Önceki', 'Prev')).setStyle(ButtonStyle.Secondary).setDisabled(p===0);
        const next = new ButtonBuilder().setCustomId(`grank_page_next:${requesterId}:${p}`).setLabel(L(gRec, 'Sonraki', 'Next') + ' ▶').setStyle(ButtonStyle.Secondary).setDisabled(p===totalPages-1);
        const info = new ButtonBuilder().setCustomId('grank_page_info').setLabel(`${p+1}/${totalPages}`).setStyle(ButtonStyle.Secondary).setDisabled(true);
        rows.push(new ActionRowBuilder().addComponents(prev, info, next));
    }
    const embed = new EmbedBuilder()
        .setTitle(L(gRec, '👥 Üye Seç (Grank)', '👥 Select Member (Grank)'))
        .setDescription(L(gRec, `${members.length} üye — sayfa ${p+1}/${totalPages}.`, `${members.length} members — page ${p+1}/${totalPages}.`))
        .setColor(0xE67E22);
    return { embeds: [embed], components: rows };
}

app.post('/api/grank/list', async (req, res) => {
    const { requesterId, members, hwid, guildId } = req.body || {};
    if (!requesterId || !Array.isArray(members)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    if (!interaction) return res.json({ success: false });
    const cleanMembers = members.map(m => String(m).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean).slice(0, 150);
    grankMemberCache.set(requesterId, { members: cleanMembers, hwid: hwid||'', guildId: guildId||'' });
    grankPageCache.set(requesterId, 0);
    console.log(`👥 [GRANK-LIST] ${cleanMembers.length} uye alindi (req=${requesterId.slice(0,6)})`);
    try{
        const msg = await buildGrankMemberMessage(requesterId, 0);
        await interaction.editReply(msg);
        setTimeout(async () => {
            try{
                const still = getPendingInteraction(requesterId);
                if(still) await still.editReply({ content: '⏰ Menü süresi doldu.', embeds: [], components: [] }).catch(()=>{});
                grankMemberCache.delete(requesterId);
                grankPageCache.delete(requesterId);
            }catch{}
        }, 5*60*1000);
    }catch(e){ console.error('GRANK-LIST edit hatasi:', e.message); }
    return res.json({ success: true });
});

app.post('/api/grank/ranklist', async (req, res) => {
    const { requesterId, ranks } = req.body || {};
    if (!requesterId || !Array.isArray(ranks)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    // grank member seciminden sonra rank listesi geliyor — ayni interaction uzerinden rank secimi goster
    // interaction zaten member seciminde update edildi, yeni bir deferred yok — editReply ile devam
    // Bu endpoint dogrudan interaction.editReply yapamaz cunku member secimi interaction'i farkli (component interaction)
    // Bu yuzden rank listesini cache'leyip member secim handler'i rank menuyu gosterecek
    // Simdilik direkt cache'le
    grankRankCache.set(requesterId, ranks.map(r=>String(r).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean));
    console.log(`🏷️ [GRANK-RANKLIST] ${ranks.length} rank alindi (req=${requesterId.slice(0,6)})`);
    return res.json({ success: true });
});

// --- SOCKET.IO REALTIME EVENTS ---
io.on('connection', (socket) => {
    console.log(`🔌 Yeni RakSAMP İstemcisi Bağlandı: ${socket.id}`);

    // İstemci HWID'sini bildirir (war/gstats hedeflemesi için)
    socket.on('register', (data) => {
        const hwid = (data && data.hwid) ? String(data.hwid) : null;
        socketStore.register(hwid, socket);
        console.log(`🎯 İstemci kaydedildi: ${socket.id} → HWID: ${hwid || 'bilinmiyor'}`);
    });

    socket.on('gstats_response', (data) => {
        console.log(`📊 /gstats Verisi Alındı:`, data);
        // C++ client httplib kullandığı için socket.emit almaz; burada socket üzerinden gelen
        // veriyi doğrudan /api/gstats aynı kanal mantığıyla işleyelim (fallback).
        const { guildId, statsText } = data || {};
        if (guildId && statsText) {
            resolveGuildForRakSAMP({ groupId: guildId }).then(async (guild) => {
                if (!guild) return;
                const channelId = guild.gstatsChannelId || null;
                if (channelId) {
                    try {
                        const channel = await client.channels.fetch(channelId);
                        if (channel) {
                            const clean = String(statsText).replace(/\{[0-9a-fA-F]{6}\}/g, '').trim();
                            const embed = new EmbedBuilder()
                                .setTitle("📊 Group Stats")
                                .setDescription(clean.slice(0, 3000))
                                .setColor(0x3498DB)
                                .setTimestamp();
                            await channel.send({ embeds: [embed] });
                        }
                    } catch (e) {
                        console.error("GStats socket gönderim hatası:", e);
                    }
                }
            });
        }
    });

    socket.on('disconnect', () => {
        socketStore.unregister(socket);
        console.log(`❌ İstemci Ayrıldı: ${socket.id}`);
    });
});

// Discord grup chat kanalında ! ile başlayan mesajları oyun içine ilet
client.on('messageCreate', async (message) => {
    if (message.author.bot) return;
    if (!message.guild) return;
    if (!message.content || !message.content.startsWith('!')) return;

    const guild = await guildUtils.getGuild(message.guild.id);
    if (!guild || !guild.groupChatEnabled || !guild.groupChatChannelId) return;
    if (message.channel.id !== guild.groupChatChannelId) return;

    const text = message.content.slice(1).trim();
    if (!text) return;

    console.log(`💬 DISCORD→OYUN (guild=${guild.guildId}): ${message.author.username}: ${text}`);
    const payload = { text, author: message.author.username };
    for (const hwid of guild.linkedHwids || []) {
        queueOutbox(hwid, payload);
    }
});

// --- GRUP CHAT <-> DISCORD KÖPRÜSÜ ---
// Oyun içi ?discordchat — MongoDB'deki aç/kapa durumunu değiştir
app.post('/api/groupchat/toggle', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    guild.groupChatEnabled = !guild.groupChatEnabled;
    await guild.save();

    console.log(`💬 GROUPCHAT (guild=${guild.guildId}): ${guild.groupChatEnabled ? 'AÇILDI' : 'KAPANDI'} (kanal=${guild.groupChatChannelId || 'null'})`);
    return res.json({ success: true, enabled: !!guild.groupChatEnabled, channelConfigured: !!guild.groupChatChannelId });
});

// Oyun içinden gelen ! mesajını Discord grup chat kanalına yaz
// Kanal ayarlanmamışsa needChannel=true döner, istemci gruba uyarı mesajı atar
app.post('/api/groupchat', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, enabled: false, needChannel: false });
    }
    if (!guild.groupChatEnabled) {
        return res.json({ success: false, enabled: false, needChannel: false });
    }

    const channelId = guild.groupChatChannelId || null;
    const text = String(body.text || '').slice(0, 200);
    const author = String(body.author || 'Oyuncu');

    if (!channelId) {
        console.log(`⚠️ GROUPCHAT (guild=${guild.guildId}): kanal ayarlanmamış, mesaj atlanıyor.`);
        return res.json({ success: false, enabled: true, needChannel: true });
    }

    try {
        const channel = await client.channels.fetch(channelId);
        if (channel) {
            // Oyun ici -> Discord koprusu: botun diline gore (Oyun) / (In-Game) on-eki
            const tag = guild.lang === 'tr' ? 'Oyun' : 'In-Game';
            await channel.send(`🎮 **${author}** (${tag}): ${text}`);
        }
    } catch (e) {
        console.error("GroupChat Discord gönderim hatası:", e);
    }

    return res.json({ success: true, enabled: true, channelConfigured: true });
});

// Oyun içi ?tr/?en — bot mesaj dilini MongoDB'de sakla (default "en")
app.post('/api/lang/set', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, lang: 'en' });
    }
    guild.lang = (String(body.lang || 'en') === 'tr') ? 'tr' : 'en';
    await guild.save();
    console.log(`🌐 LANG (guild=${guild.guildId}): ${guild.lang}`);
    return res.json({ success: true, lang: guild.lang });
});

// RakSAMP istemcisi bekleyen Discord mesajlarını ve köprü durumunu çeker (polling)
app.get('/api/groupchat/poll', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    const guild = await guildUtils.getGuildByHwid(hwid);
    const enabled = guild ? !!guild.groupChatEnabled : false;
    const channelConfigured = guild ? !!guild.groupChatChannelId : false;
    const messages = (groupChatOutbox.get(hwid) || []).slice();
    groupChatOutbox.delete(hwid);
    const out = { enabled, channelConfigured, messages };

    // Bot'un oyun içi grubunu guild kaydına yaz (poll'dan öğrenilir): log kanalı
    // eşleşmesi/dil bu grup adı üzerinden yürür, guild yoksa hiçbir şey değişmez.
    if (guild && req.query.group) {
        const grp = String(req.query.group).slice(0, 255);
        if (guild.groupName !== grp) {
            guild.groupName = grp;
            await guild.save();
            console.log(`🏷️ GROUP(guild=${guild.guildId}): ${grp}`);
        }
    }
    // Guild yoksa lang alanı GÖNDERİLMEZ: istemci lokal ?tr/?en seçimini korur
    // (guild.lang varsayılanı 'en' yoksa ezerdi).
    if (guild) out.lang = guild.lang;
    return res.json(out);
});

// Oyun içi ?guildid: bu HWID'i Discord sunucusuna bağlar (linkedHwids).
// Boylece botu ekleyen HER grup kendi sunucusuyla kullanabilir;
// mevcut tüm hwid tabanlı çağrılar (lang, groupchat, welcome...) otomatik çözülür.
app.post('/api/guildid', async (req, res) => {
    const body = req.body || {};
    const hwid = String(body.hwid || '').trim();
    const guildId = String(body.guildId || '').trim();
    if (!hwid || !/^\d+$/.test(guildId)) {
        return res.json({ success: false, message: "Gecersiz guild id." });
    }
    const guild = await guildUtils.getGuild(guildId);
    if (!guild.linkedHwids.includes(hwid)) {
        guild.linkedHwids.push(hwid);
        await guild.save();
    }
    console.log(`🔗 GUILDID (hwid=${hwid} -> guild=${guildId})`);
    return res.json({ success: true, guildId: guild.guildId });
});

// RakSAMP client'ı: oyuncu istatistik cache'ini DB'ye push eder (?pl verisi).
// Body: { players: [{ id, name, packetloss, fps, ping }, ...] } — id üzerinden upsert.

app.post('/api/player/cache', async (req, res) => {
    const players = (req.body || {}).players;
    if (!Array.isArray(players)) return res.status(400).json({ success: false, message: "players bekleniyor" });
    const rows = players
        .filter(p => Number.isInteger(p.id) && p.id >= 0 && p.id < 1000)
        .map(p => [p.id, String(p.name || '').slice(0, 64), Number(p.packetloss) || 0, Number(p.fps) || 0, Number(p.ping) || 0, new Date()]);
    if (!rows.length) return res.json({ success: true, count: 0 });
    try {
        // Cache dosyası tek doğru kaynak olduğundan tablo her push'ta sıfırlanıp
        // yeniden yazılır (eski/çıkmış oyuncu verisi burada kalmaz).
        await db.execute('DELETE FROM players');
        await db.query(
            `INSERT INTO players (id, name, packetloss, fps, ping, updated_at) VALUES ?`,
            [rows]
        );
        console.log(`🎮 PLAYER-CACHE: ${rows.length} oyuncu DB'ye yazildi`);
        return res.json({ success: true, count: rows.length });
    } catch (err) {
        console.error('PLAYER-CACHE hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// Client okuma: slot->isim + ?pl FPS/ping icin (player_cache.txt kaldirildi,
// tek kaynak DB'deki players tablosu).
app.get('/api/player/cache', async (req, res) => {
    const id = req.query.id !== undefined ? Number(req.query.id) : NaN;
    const name = String(req.query.name || '').trim().slice(0, 64);
    try {
        let rows;
        if (Number.isInteger(id) && id >= 0 && id < 1000) {
            rows = await db.execute('SELECT id, name, packetloss, fps, ping, updated_at, TIMESTAMPDIFF(SECOND, updated_at, NOW()) AS ageSec FROM players WHERE id = ? LIMIT 2', [id]);
        } else if (name) {
            rows = await db.execute('SELECT id, name, packetloss, fps, ping, updated_at, TIMESTAMPDIFF(SECOND, updated_at, NOW()) AS ageSec FROM players WHERE LOWER(name) = LOWER(?) LIMIT 5', [name]);
        } else {
            return res.status(400).json({ success: false, message: 'id veya name gerekli' });
        }
        return res.json({ success: true, rows });
    } catch (err) {
        console.error('PLAYER-CACHE-GET hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// RakSAMP client: ogrenilen grup adi + kisa tag + KALICI sahip olunan zone
// id'leri tek satir (group_name üzerinden UPSERT). zones JSON = o grubun
// sahip oldugu TUM zone'lar (flashing filtresi yok — kalici harita).
// Client acilista GET /api/groups ile BUGUNKU kayitlari geri yükler;
// boylece dunun verisi otomatik olur, zones.txt gibi dosya YOK.
app.post('/api/groups', async (req, res) => {
    const groups = (req.body || {}).groups;
    if (!Array.isArray(groups)) return res.status(400).json({ success: false, message: "groups bekleniyor" });
    const seen = new Set();
    const rows = [];
    for (const g of groups) {
        const name = String(g.name || '').trim().slice(0, 96);
        if (!name || seen.has(name)) continue;
        seen.add(name);
        const zones = Array.isArray(g.zones)
            ? JSON.stringify(g.zones.map(Number).filter(n => Number.isInteger(n) && n >= 0 && n < 2048))
            : '[]';
        rows.push([name, String(g.tag || '').slice(0, 32), zones, new Date()]);
    }
    try {
        if (!rows.length) return res.json({ success: true, count: 0 });
        await db.query(
            `INSERT INTO gangs (group_name, tag, zones, updated_at) VALUES ?
             ON DUPLICATE KEY UPDATE
               tag=IF(VALUES(tag) IN ('', '~') AND tag NOT IN ('', '~'), tag, VALUES(tag)),
               zones=VALUES(zones),
               updated_at=VALUES(updated_at)`,
            [rows]
        );
        console.log(`🏷️ GROUPS: ${rows.length} grup upsert edildi (kalici zone haritasiyla)`);
        // Tek-sahiplik: bu push'ta bildirilen zoneler baska gruplarda kalmasin.
        // (Tarama tazelerken eski sahip satirinda hayalet zone kaliyordu: #4 uL'de
        // ama HM satirinda da gorunuyordu -> ?zf karisiyordu.)
        try {
            const claimed = new Set();
            for (const r of rows) {
                let zs = [];
                try { zs = JSON.parse(r[2] || '[]'); } catch (e) { zs = []; }
                if (Array.isArray(zs)) zs.map(Number).filter(n => Number.isInteger(n) && n >= 0).forEach(z => claimed.add(z));
            }
            if (claimed.size) {
                const pushed = new Set(rows.map(r => r[0]));
                const all = await db.query('SELECT group_name, zones FROM gangs');
                for (const g of all) {
                    if (pushed.has(g.group_name)) continue;
                    let z = [];
                    try { z = JSON.parse(g.zones); } catch (e) { continue; }
                    if (!Array.isArray(z)) continue;
                    const kept = z.map(Number).filter(n => !claimed.has(n));
                    if (kept.length !== z.length) {
                        await db.execute('UPDATE gangs SET zones = ?, updated_at = ? WHERE group_name = ?', [JSON.stringify(kept), new Date(), g.group_name]);
                        console.log(`🏷️ TEK-SAHIPLIK: zone ${z.length - kept.length} adet '${g.group_name}' satirindan temizlendi`);
                    }
                }
            }
        } catch (e) { console.error('GROUPS tek-sahiplik hatasi:', e.message); }
        return res.json({ success: true, count: rows.length });
    } catch (err) {
        console.error('GROUPS hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// Grup adi -> DB'deki zone listesi. Client bellek restore basarisiz olunca
// (paket kaybi) ?zgo/?zsnip zone bulamaz; buradan dogrudan DB zone'lari alinir.
app.get('/api/groups/zone', async (req, res) => {
    const name = String(req.query.name || '').trim().slice(0, 96);
    if (!name) return res.json({ success: false, zones: [] });
    try {
        const rows = await db.execute('SELECT zones FROM gangs WHERE group_name=? LIMIT 1', [name]);
        let zones = [];
        if (rows && rows[0] && rows[0].zones) {
            try { zones = JSON.parse(rows[0].zones); } catch { /* bos */ }
        }
        return res.json({ success: true, zones: Array.isArray(zones) ? zones : [] });
    } catch (err) {
        return res.status(500).json({ success: false, zones: [], message: err.message });
    }
});

// Client acilista BUGUNKU grup+tag+zone haritasini buradan geri yükler.
// DATE(updated_at)=CURDATE() filtresi sayesinde dunun verisi otomatik elenir.
app.get('/api/groups', async (req, res) => {
    try {
        // db.query() zaten dogrudan satir dizisini dondurur (tekrar destructure ETME).
        // TAG'ler zamansizdir (tum gruplar), ZONE sahipligi gunluktur (bugun).
        // (CURDATE filtresi tag'leri de budayip ?zf'de isim gosteriyordu: 41/200+ vakasi.)
        const tagRows = await db.query(
            'SELECT group_name, MAX(tag) AS tag FROM gangs GROUP BY group_name'
        );
        const zoneRows = await db.query(
            'SELECT group_name, zones FROM gangs WHERE DATE(updated_at) = CURDATE()'
        );
        const zoneByName = {};
        for (const r of (Array.isArray(zoneRows) ? zoneRows : [])) {
            zoneByName[r.group_name] = r.zones ? JSON.parse(r.zones) : [];
        }
        const out = (Array.isArray(tagRows) ? tagRows : [])
            .filter(r => r.group_name)
            .map(r => ({
                name: r.group_name,
                tag: r.tag || '',
                zones: zoneByName[r.group_name] || []
            }));
        return res.json({ success: true, groups: out });
    } catch (err) {
        console.error('GROUPS-GET hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// RakSAMP client: her botun XML'deki ismiyle (name) eşleşen satırı upsert eder
// (her bot için DB'de tek satır). Body: { name, data, groupName, zscan, gzt, zgrp }.
// zscan/gzt/zgrp GONDERILMEDIYSE eski deger korunur (client acilisinda henuz
// DB'den okumadan status yazarsa kayitli tercih silinmesin).
app.post('/api/bot/status', async (req, res) => {
    const body = req.body || {};
    const name = String(body.name || '').trim().slice(0, 64);
    if (!name) return res.json({ success: false, message: "name bekleniyor" });
    const data = (body.data !== undefined) ? JSON.stringify(body.data) : null;

    const fields = ['name', 'data', 'updated_at'];
    const params = [name, data, new Date()];
    // group_name opsiyonel: GELMEDIYSE korunur (client acilista bos gonderip
    // kayitli adi silmesin), GELDİYSE guncellenir (bos dahil - ?gname manuel temizlik).
    if (body.groupName !== undefined) { fields.push('group_name'); params.push(String(body.groupName || '').slice(0, 255)); }
    if (body.zscan !== undefined) { fields.push('zscan_enabled'); params.push(body.zscan ? 1 : 0); }
    if (body.gzt !== undefined) { fields.push('gzt_enabled'); params.push(body.gzt ? 1 : 0); }
    if (body.zgrp !== undefined) { fields.push('zgrp_enabled'); params.push(body.zgrp ? 1 : 0); }
    try {
        const setClause = fields.filter(f => f !== 'name').map(f => `${f}=VALUES(${f})`).join(', ');
        await db.execute(
            `INSERT INTO bots (${fields.join(',')}) VALUES (${fields.map(() => '?').join(',')})
             ON DUPLICATE KEY UPDATE ${setClause}`,
            params
        );
        console.log(`🤖 BOT-STATUS: '${name}' DB'ye yazildi (zscan=${body.zscan !== undefined ? (body.zscan ? 1 : 0) : 'korundu'}, gzt=${body.gzt !== undefined ? (body.gzt ? 1 : 0) : 'korundu'}, zgrp=${body.zgrp !== undefined ? (body.zgrp ? 1 : 0) : 'korundu'})`);
        return res.json({ success: true });
    } catch (err) {
        console.error('BOT-STATUS hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// Bot'un kayitli grup adini oku (acilis: dosya yerine DB'den). Yoksa bos doner,
// client /stats ile ogrenip POST /api/bot/status uzerinden tekrar yazar.
app.get('/api/bot/status', async (req, res) => {
    const name = String(req.query.name || '').trim().slice(0, 64);
    if (!name) return res.json({ success: false, groupName: '' });
    try {
        const rows = await db.execute('SELECT name, group_name FROM bots WHERE name=? LIMIT 1', [name]);
        const r = (rows && rows[0]) || {};
        return res.json({ success: true, groupName: r.group_name || '' });
    } catch (err) {
        console.error('BOT-STATUS GET hatasi:', err.message);
        return res.status(500).json({ success: false, groupName: '', message: err.message });
    }
});

// ?zscan / ?gzt / ?zgrp on/off tercihlerini gruba gore oku: client acilisinda
// grubunun durumunu ceker.
app.get('/api/bot/toggles', async (req, res) => {
    const group = String(req.query.group || '').slice(0, 255);
    if (!group) return res.json({ success: false, zscan: 0, gzt: 1, zgrp: 1, message: "group bekleniyor" });
    try {
        const rows = await db.execute('SELECT zscan_enabled, gzt_enabled, zgrp_enabled FROM bots WHERE group_name=? ORDER BY updated_at DESC LIMIT 1', [group]);
        const r = (rows && rows[0]) || {};
        const zscan = r.zscan_enabled ? 1 : 0;
        const gzt = (r.gzt_enabled === undefined) ? 1 : (r.gzt_enabled ? 1 : 0);
        const zgrp = (r.zgrp_enabled === undefined) ? 1 : (r.zgrp_enabled ? 1 : 0);
        return res.json({ success: true, zscan, gzt, zgrp });
    } catch (err) {
        console.error('BOT-TOGGLES hatasi:', err.message);
        return res.status(500).json({ success: false, zscan: 0, gzt: 1, zgrp: 1, message: err.message });
    }
});

server.listen(config.PORT, () => {
    console.log(`🚀 VDS Backend Aktif! Port: ${config.PORT}`);
});
