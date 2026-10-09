const fs = require('fs');
const path = require('path');
const { Collection } = require('discord.js');

const commands = new Collection();
const commandsDir = path.join(__dirname, '..', 'commands');

// commands/ ve commands/<Kategori>/ altindaki tum komutlari yukle
// (help komutu kategori listesini bu klasorlerden okuyor)
function listCategories() {
    if (!fs.existsSync(commandsDir)) return [];
    return fs.readdirSync(commandsDir, { withFileTypes: true })
        .filter(d => d.isDirectory())
        .map(d => d.name)
        .sort();
}

function loadCommands() {
    commands.clear();
    // kokte duz .js (geriye uyumluluk) + kategori klasorleri
    const files = fs.readdirSync(commandsDir).filter(f => f.endsWith('.js'));
    for (const file of files) {
        const cmd = require(path.join(commandsDir, file));
        if (cmd && cmd.data) commands.set(cmd.data.name, cmd);
    }
    for (const cat of listCategories()) {
        const catDir = path.join(commandsDir, cat);
        for (const file of fs.readdirSync(catDir).filter(f => f.endsWith('.js'))) {
            const cmd = require(path.join(catDir, file));
            if (cmd && cmd.data) commands.set(cmd.data.name, cmd);
        }
    }
    console.log(`✅ ${commands.size} komut yuklendi (${listCategories().length} kategori).`);
    return commands;
}

// Slash komutlari botun bulundugu her sunucuya kaydet (aninda gorunur)
async function registerCommands(client) {
    const data = commands.map(c => c.data.toJSON());
    for (const guild of client.guilds.cache.values()) {
        await guild.commands.set(data);
    }
    console.log(`🆕 ${data.length} slash komut ${client.guilds.cache.size} sunucuya kaydedildi.`);
}

async function registerForGuild(guild, data) {
    await guild.commands.set(data);
}

async function handleInteraction(interaction) {
    if (!interaction.isChatInputCommand()) return;

    const cmd = commands.get(interaction.commandName);
    if (!cmd) return;

    try {
        await cmd.execute(interaction);
    } catch (e) {
        console.error(`Komut hatasi (${interaction.commandName}):`, e);
        const msg = { content: '❌ Komut calistirilirken hata olustu.', ephemeral: true };
        if (interaction.replied || interaction.deferred) {
            await interaction.followUp(msg).catch(() => {});
        } else {
            await interaction.reply(msg).catch(() => {});
        }
    }
}

module.exports = { loadCommands, registerCommands, registerForGuild, handleInteraction, commands, listCategories, commandsDir };
