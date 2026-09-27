// Tek dosyada model tanimlari (models/ klasoru kaldirildi - kullanici).
// db.js uzerinde mongoose-uyumlu minimal API: findOne / create / findOneAndUpdate.
// Alan adlari kodun geri kalaninin bekledigi gibi (guildId, linkedHwids, ...).
const db = require('./db');

const Guild = db.model('guilds', {
    pkey: 'guildId',
    fields: {
        guildId: { col: 'guild_id', type: 'text', pk: true },
        linkedHwids: { col: 'linked_hwids', type: 'json', def: [] },
        logChannelId: { col: 'log_channel_id', type: 'text', def: null },
        warChannelId: { col: 'war_channel_id', type: 'text', def: null },
        gstatsChannelId: { col: 'gstats_channel_id', type: 'text', def: null },
        groupChatChannelId: { col: 'group_chat_channel_id', type: 'text', def: null },
        zoneWarChannelId: { col: 'zone_war_channel_id', type: 'text', def: null },
        backupChannelId: { col: 'backup_channel_id', type: 'text', def: null },
        groupLogChannelId: { col: 'group_log_channel_id', type: 'text', def: null },
        botCommandsChannelId: { col: 'bot_commands_channel_id', type: 'text', def: null },
        categoryId: { col: 'category_id', type: 'text', def: null },
        groupChatEnabled: { col: 'group_chat_enabled', type: 'int', def: 0 },
        lang: { col: 'lang', type: 'text', def: 'en' },
        groupName: { col: 'group_name', type: 'text', def: '' },
        allowedRoleId: { col: 'allowed_role_id', type: 'text', def: null },
        createdAt: { col: 'created_at', type: 'text', def: () => new Date().toISOString() }
    }
});

const License = db.model('licenses', {
    pkey: 'hwid',
    fields: {
        hwid: { col: 'hwid', type: 'text', pk: true },
        isApproved: { col: 'is_approved', type: 'int', def: 0 },
        welcomeEnabled: { col: 'welcome_enabled', type: 'int', def: 1 },
        welcomeMessage: { col: 'welcome_message', type: 'text', def: '' },
        createdAt: { col: 'created_at', type: 'text', def: () => new Date().toISOString() }
    }
});

module.exports = { Guild, License };