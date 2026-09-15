// Ortak yetki + baglanti yardimcilari (komutlar arasi tekrari onlemek icin).
// grole/grank/gkick/ginv/pm gibi yazma komutlari bu kapidan gecer.

// guild.allowedRoleId ayarliysa o rol (veya Administrator/ManageGuild/sunucu sahibi) gerekir.
// Ayarlanmamis ise kisitlama yoktur (herkes kullanir).
function hasGroupPermission(interaction, guild) {
    if (!guild || !guild.allowedRoleId) return true;
    const member = interaction.member;
    if (!member) return false;
    if (member.roles && member.roles.cache.has(guild.allowedRoleId)) return true;
    if (member.permissions && (member.permissions.has('Administrator') || member.permissions.has('ManageGuild'))) return true;
    if (interaction.guild && interaction.guild.ownerId === interaction.user.id) return true;
    return false;
}

// Sunucuya bagli ilk RakSAMP HWID'sini dondur (bagli istemci yoksa null).
function getLinkedHwid(guild) {
    if (!guild || !Array.isArray(guild.linkedHwids) || guild.linkedHwids.length === 0) return null;
    return guild.linkedHwids[0];
}

module.exports = { hasGroupPermission, getLinkedHwid };
