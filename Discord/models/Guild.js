// Guild: MySQL/ProxySQL shim (db.js). Alan adlar─▒ kodun geri kalan─▒n─▒n bekledi─si gibi.
module.exports = require('../db').model('guilds', {
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
        groupChatEnabled: { col: 'group_chat_enabled', type: 'int', def: 0 },
        lang: { col: 'lang', type: 'text', def: 'en' },
        groupName: { col: 'group_name', type: 'text', def: '' },
        allowedRoleId: { col: 'allowed_role_id', type: 'text', def: null },
        createdAt: { col: 'created_at', type: 'text', def: () => new Date().toISOString() }
    }
});
