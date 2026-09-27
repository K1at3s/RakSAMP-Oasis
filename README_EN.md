# RakSAMP — UIF Group Bot

[**English**](README_EN.md) | [**Türkçe**](README.md)

RakSAMP is an automated group bot for the **UIF (United Islands Freeroam)** server. Built on a SA-MP fake client base, it integrates with a Node.js/Discord backend.

**Bot:** `hakixld` — automatic zone scanning, war tracking, group tag harvesting, Discord integration.

---

## 🎮 Features

- **Automatic zone scanning** — teleports to flashing zones one by one and learns ownership via `/gzinfo`, persisting an ownership map to MySQL (`gangs` table). Normal rounds scan unknown zones only; every 4th round is a full check.
- **Group tag harvest (`?zgrp` / `?tagscan`)** — collects missing short tags through the `/groups` + `/gstats <ID>` chain and writes them to the DB. **Off by default**; triggered manually.
- **War tracking** — zone war start/end, `?gzt` threshold-crossing countdown (10/3/1 guaranteed), kill-list textdraw collection + Discord embed.
- **Kill list embed** — won/lost title, red vs blue teams + tags, two aligned boxes (Name/K/A/D/DMG + Total), ⭐ MVP, 🎯 Top Feeder and ⚡ First Blood (first kill in the GZ, with weapon + distance).
- **Discord integration** — group chat bridge, interactive `/grole` `/grank` menus, per-user welcome, admin/war logs, `?post` group log embed.
- **UIF login fix** — GTA-client format dialog submit (BYTE length), logging into the real account without falling into a tmp account.
- **Group name learning** — `/stats` → "Group" button → correct group name read from the second dialog.
- **Virtual world fix** — spawn menu "Freeroam" reply in GTA-client format; the bot spawns in the real freeroam.
- **Security commands** — `?changepass` / `?email` PM-only with whitelisted names, UIF input menu automation.

---

## 🚀 Setup

1. **MySQL** — create the `raksamp` database (`Discord/schema.sql`); tables are created automatically.
2. **Backend** — start with `bun .` in `Discord/` (Discord bot token + MySQL settings in `config.js`).
3. **Client** — run `client/bin/RakSAMPClient.exe`:
   - `RakSAMPClient.xml` — server address, bot name (`hakixld`), `normal_pos` spawn position.
   - On first launch, log in by typing `/login <password>` in the console (the server session is remembered afterwards).
   - The client shuts itself down if the backend is offline.
4. **Discord** — create the log channels with `/setup` (TR: `/kurulum`). Channels are NOT auto-created; only this command creates them. `/setup off` (TR: `/setup kapat`) deletes them.

---

## ⌨️ In-Game Commands (group chat)

All commands are typed with the `?` prefix in the group chat. Messages prefixed with `!` are forwarded to Discord.

### General
| Command | Description |
|---------|-------------|
| `?help` | Command list |
| `?test` | Bot alive check |
| `?info` | Bot version info |
| `?disc` | Discord server invite |
| `?tr` / `?en` | Switch bot language Turkish/English |
| `?m` | Test message |

### Player / Stats
| Command | Description |
|---------|-------------|
| `?ping` | Ping check |
| `?w` | War help (Discord @here, 3 min cooldown, localized) |
| `?pl [id]` | Player stats (packetloss/FPS/ping) |
| `?gst` | Current zone + active player count |
| `?gstats [id]` | Group stats (sent to Discord) |
| `?astats <id>` | Account stats |
| `?alog <id>` | Admin log summary |
| `?az` | Free zone info |

### Zone / Scan
| Command | Description |
|---------|-------------|
| `?zlist` / `?zwho` | Current zone info (/gzinfo) |
| `?zscan on/off` | Automatic zone scanning on/off |
| `?zgo <#N\|tag\|name>` | Teleport to zone, wait 40s, return |
| `?za` / `?zattack` | Zone attack (/gz) |
| `?zsnip [zone]` | Zone sniper (attacks /gz when war ends, Co-Leader permission) |
| `?zf` | Flashing zone distribution + scan unknowns |

### Group / Tag
| Command | Description |
|---------|-------------|
| `?zgrp` | Group tag harvest (manual scan) |
| `?tagscan` | Alias for `?zgrp` |
| `?tag <name> <tag>` | Manually save a group tag (local + DB) |
| `?gname` | Re-learn the group name |
| `?grole <name> <dep\|leader\|co\|memb\|trial\|passive\|guest>` | In-game role change (Deputy Owner/Leader/Co-Leader/Member/Trial/Passive/Guest) |
| `?grank <name> <rank>` | In-game rank change |
| `?ginvite <id>` | `?ginv` alias — invite a player to the group (result relayed to group) |

### War
| Command | Description |
|---------|-------------|
| `?gzt on/off/time` | War countdown announcements (threshold-crossing) |
| `?gz` / `?gz2` | Join war / call backup |
| `?post [reason]` | Log the last group action as a Discord embed (30s reason window) |

### Security (PM-only, whitelisted names)
| Command | Description |
|---------|-------------|
| `?changepass <current> <new>` | Oasis password change (current-password dialog first, then new-password dialog) |
| `?changemail <password> <email>` | Oasis email change (`?email` alias; password dialog first, then email dialog) |

### Settings
| Command | Description |
|---------|-------------|
| `?greet on/off` | Welcome messages |
| `?welcomemsg <name> <msg>` | Per-user welcome message |
| `?gm <msg>` | Global welcome message alias |
| `?discordchat on/off` | Discord group chat bridge |
| `?guildid <id>` | Bind HWID to Discord server |

---

## 🤖 Discord Slash Commands

Command names are localized per server language (English base, Turkish localization).

| Command (EN / TR) | Description |
|-------|-------------|
| `/link <hwid>` | Link a HWID to this Discord server (admin) |
| `/unlink <hwid>` | Remove the link (admin) |
| `/setup` / `/kurulum` | Create channels (empty) / delete (`off`) (admin) |
| `/setchannel` / `/kanalayarla` | Channel assignment: war, gstats, group-chat, zone-war, backup, group-log, bot-commands |
| `/help` / `/yardim` | Command list; `share:true` posts it to the bot-commands channel |
| `/gstats [group_id]` | Group stats (fetched from the game, ephemeral) |
| `/stats [id]` | Group/player stats (blank = own) |
| `/astats <account_id>` | Account stats |
| `/admins` | Online admin list |
| `/alog <player\|id>` | Admin punishment history (ban/mute/kick) |
| `/glog <player\|id>` | Group role/rank change history |
| `/oyuncu <name\|id>` | Cached packetloss/FPS/ping values (age-stamped) |
| `/bolge <group\|tag>` | Group tag + owned zones (DB `gangs`) |
| `/botdurum` | Bot record: group name + zscan/gzt/zgrp state (DB `bots`) |
| `/send <command>` | Send a free-form command/message in-game (admin) |
| `/pm <player> <message>` / `/mesaj` | Private message to a player; the reply comes back ephemeral |
| `/duyuru <message>` | Broadcast to the group chat (stays in-game, not mirrored to the bridge channel) |
| `/backup <group> [message]` | Emergency call (@here) |
| `/zonewar ...` | Manual ZONE WAR notification |
| `/grole [name] [role]` | Give a member a role (Member/Co-Leader) — interactive member/role picker |
| `/grank [name] [rank]` | Give a member a rank — interactive member/rank picker |
| `/gkick [name] [reason]` | Kick a member — reason modal + paginated list + dialog automation |
| `/ginv <id>` | Invite a player to the group (`/ginvite id`) |
| `/gmembers` | Member list + VIP harvest → Google Sheets roster |
| `/dil <tr\|en>` | Bot language (embeds + in-game announcements; the client syncs within 2s) |
| `/kopru [on\|off]` | Discord ↔ in-game group chat bridge |
| `/zscan [on\|off\|sn]` | Automatic zone scan (`sn` = one-shot full verify) |
| `/gzt [on\|off\|time]` | Zone-war countdown announcements |
| `/gname` | Re-learn the group name |
| `/setallowedrole [role]` | Allowed role for group commands (blank = everyone) |

---

## 🗄️ Database (MySQL `raksamp`)

| Table | Description |
|-------|-------------|
| `gangs` | Group name (PK), tag, zones JSON — persistent zone ownership map |
| `bots` | Per-bot settings: group_name, zscan/gzt/zgrp on-off |
| `licenses` | HWID licenses + welcome message/enabled |
| `welcome_per_user` | Per-user welcome messages (hwid + name) |
| `guilds` | Discord server settings (channels, allowed_role_id, lang) |
| `players` | Player stats cache (packetloss/FPS/ping) |
| `admin_logs` | Admin action logs |
| `group_logs` | Group role change logs |
| `roster_messages` | Roster link message posted/edited on `/gmembers` completion |

---

## 📁 Project Structure

```
client/           C++ client (RakSAMPClient.exe + XML config)
  src/            netrpc.cpp, Security.cpp, cmds.cpp, main.cpp...
  bin/            RakSAMPClient.exe, RakSAMPClient.xml, group_colors.txt
raknet/           RakNet + SA-MP RPC layer (SAMPRPC.cpp)
Discord/          Node.js backend (server.js, db.js, commands/, models/)
CHANGELOG.md      Version history (Turkish + English)
```