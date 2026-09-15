-- RakSAMP MySQL şeması. ProxySQL'e (6033) veya doğrudan MySQL'e (3306) uygula:
--   mysql -h 127.0.0.1 -P 6033 -u raksamp -p < schema.sql
-- NOT: bot her açılışta eksik kolon/tabloları kendi de ekler (db.js init),
-- yani bu dosya kurulum/elle kurulum referansıdır ve db.js ile aynı olmalıdır.
CREATE DATABASE IF NOT EXISTS raksamp CHARACTER SET utf8mb4;
USE raksamp;

CREATE TABLE IF NOT EXISTS guilds (
    guild_id VARCHAR(255) PRIMARY KEY,
    linked_hwids TEXT NULL,               -- JSON dizisi: ["HWID1","HWID2"]
    log_channel_id VARCHAR(255) NULL,
    war_channel_id VARCHAR(255) NULL,
    gstats_channel_id VARCHAR(255) NULL,
    group_chat_channel_id VARCHAR(255) NULL,
    zone_war_channel_id VARCHAR(255) NULL,
    backup_channel_id VARCHAR(255) NULL,
    group_log_channel_id VARCHAR(255) NULL,
    bot_commands_channel_id VARCHAR(255) NULL,
    group_chat_enabled TINYINT NOT NULL DEFAULT 0,
    lang VARCHAR(8) NOT NULL DEFAULT 'en',
    group_name VARCHAR(255) NOT NULL DEFAULT '',   -- botun oyun içi grubu (poll'dan öğrenilir)
    allowed_role_id VARCHAR(255) NULL,         -- /setallowedrole: grup komutları için izinli rol
    created_at VARCHAR(64) NULL
);

CREATE TABLE IF NOT EXISTS licenses (
    hwid VARCHAR(255) PRIMARY KEY,
    is_approved TINYINT NOT NULL DEFAULT 0,
    welcome_enabled TINYINT NOT NULL DEFAULT 1,
    welcome_message VARCHAR(255) NULL DEFAULT '',
    created_at VARCHAR(64) NULL
);

-- RakSAMP client'ından push edilen oyuncu istatistik cache'i (/?pl ve /oyuncu)
CREATE TABLE IF NOT EXISTS players (
    id INT PRIMARY KEY,                -- SAMP playerid (0-999)
    name VARCHAR(64) NULL,             -- grup başlığından öğrenilen gerçek ad
    packetloss DOUBLE NULL,
    fps INT NULL,
    ping INT NULL,
    updated_at DATETIME NULL
);

-- Her botun XML'deki ismiyle (name) eşleştiği satır; botun verileri JSON data'da.
CREATE TABLE IF NOT EXISTS bots (
    name VARCHAR(64) PRIMARY KEY,      -- RakSAMPClient.xml <server nickname="...">
    data TEXT NULL,                    -- JSON: botun anlık verileri
    group_name VARCHAR(255) NULL,
    zscan_enabled TINYINT NOT NULL DEFAULT 0,
    gzt_enabled TINYINT NOT NULL DEFAULT 1,
    zgrp_enabled TINYINT NOT NULL DEFAULT 1,
    updated_at DATETIME NULL
);

-- Client'ın öğrendiği grup adı + kısa tag (gruba göre TEK satır; zone kaydı yok).
-- zones: JSON dizi — grubun sahip olduğu TÜM zone id'leri (1 satırda birden fazla).
-- 'groups' MySQL reserved word olduğu için tablo adı 'gangs'.
CREATE TABLE IF NOT EXISTS gangs (
    group_name VARCHAR(96) PRIMARY KEY,
    tag VARCHAR(32) NULL,
    zones TEXT NULL,
    updated_at DATETIME NULL
);

-- Admin eylem logları (oyun içi "ADMIN: X banned Y ... (REASON: Z)") — /alog
CREATE TABLE IF NOT EXISTS admin_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    admin_name VARCHAR(64) NULL,
    target_name VARCHAR(64) NULL,
    target_id BIGINT NULL,
    action VARCHAR(16) NULL,
    mode VARCHAR(64) NULL,
    duration_min INT NULL,
    reason VARCHAR(255) NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Kişi bazlı hoş geldin mesajları (hwid + hedef oyuncu ismi)
CREATE TABLE IF NOT EXISTS welcome_per_user (
    id INT AUTO_INCREMENT PRIMARY KEY,
    hwid VARCHAR(255) NOT NULL,
    target_name VARCHAR(64) NOT NULL,
    custom_message VARCHAR(255) NOT NULL,
    updated_at DATETIME NULL,
    UNIQUE KEY uq_hwid_target (hwid, target_name)
);

-- Grup rol/rank değişiklik logları — /glog
CREATE TABLE IF NOT EXISTS group_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    actor_name VARCHAR(64) NULL,
    actor_acc BIGINT NULL,
    target_name VARCHAR(64) NULL,
    target_acc BIGINT NULL,
    old_rank VARCHAR(64) NULL,
    new_rank VARCHAR(64) NULL,
    reason VARCHAR(255) NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ?gmembers bitiminde kanala atılan/düzenlenen roster link mesajı
CREATE TABLE IF NOT EXISTS roster_messages (
    guild_id VARCHAR(255) PRIMARY KEY,
    channel_id VARCHAR(255) NULL,
    message_id VARCHAR(255) NULL,
    spreadsheet_id VARCHAR(255) NULL,
    member_count INT NULL,
    vip_count INT NULL,
    updated_at DATETIME NULL
);

-- ProxySQL için önerilen tek sorgu kuralı: tümü raksamp DB'sine yönlenir.