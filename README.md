# RakSAMP — UIF Grup Botu

[**English**](README_EN.md) | **Türkçe**

RakSAMP, **UIF (United Islands Freeroam)** sunucusu için geliştirilmiş otomatik grup botudur. SA-MP fake client altyapısı üzerine kurulu, Node.js/Discord backend ile entegre çalışır.

**Bot:** `hakixld` — otomatik zone tarama, savaş takibi, grup tag harvest, Discord entegrasyonu.

---

## 🎮 Özellikler

- **Otomatik zone tarama** — flashing zone'ları tek tek teleport + `/gzinfo` ile öğrenir, sahiplik haritasını MySQL'e (`gangs` tablosu) kalıcı yazar. Normal tur bilinmeyen-only, her 4. tur tam kontrol.
- **Grup tag harvest (`?zgrp` / `?tagscan`)** — `/groups` + `/gstats <ID>` zinciri ile eksik kısa tag'leri toplar, DB'ye yazar. Varsayılan **kapalı**; elle tetiklenir.
- **Savaş takibi** — zone war başlangıç/bitiş, `?gzt` eşik geçişli geri sayım (10/3/1 garanti), kill-list textdraw toplama + Discord'a embed.
- **Kill list embed** — kazanıldı/kaybedildi başlığı, kırmızı vs mavi takım + tag'ler, iki hizalı kutu (Name/K/A/D/DMG + Total), ⭐ MVP, 🎯 Top Feeder ve ⚡ First Blood (GZ'deki ilk kill, silah + mesafe).
- **Discord entegrasyonu** — grup chat köprüsü, `/grole` `/grank` interaktif menüler, per-user welcome, admin/war logları, `?post` grup log embed.
- **UIF login fix** — GTA-client formatında dialog submit (BYTE uzunluk), tmp hesaba düşmeden gerçek hesaba giriş.
- **Grup adı öğrenme** — `/stats` → "Group" butonu → ikinci diyalogdan doğru grup adı.
- **Virtual world fix** — spawn menüsü "Freeroam" cevabı GTA-client formatında, bot gerçek freeroam'da spawn olur.
- **Güvenlik komutları** — `?changepass` / `?email` yalnızca PM + yetkili isimlerle, UIF input menüsü otomasyonuyla.

---

## 🚀 Kurulum

1. **MySQL** — `raksamp` veritabanı oluştur (`Discord/schema.sql`), tablolar otomatik kurulur.
2. **Backend** — `Discord/` klasöründe `bun .` ile başlat (Discord bot token + MySQL ayarları `config.js`).
3. **Client** — `client/bin/RakSAMPClient.exe` çalıştır:
   - `RakSAMPClient.xml` — sunucu adresi, bot ismi (`hakixld`), `normal_pos` spawn pozisyonu.
   - İlk açılışta konsola `/login <şifre>` yazarak hesaba gir (sunucu oturumu sonra hatırlar).
   - Backend kapalıysa client kendini kapatır.
4. **Discord** — `/setup` (TR: `/kurulum`) komutuyla log kanallarını kur. Kanallar otomatik oluşturulmaz; yalnızca bu komutla kurulur. `/setup kapat` (EN: `/setup off`) ile silinir.

---

## ⌨️ Oyun İçi Komutlar (grup chat)

Tüm komutlar grup chat'ine `?` önekiyle yazılır. `!` önekli mesajlar Discord'a iletilir.

### Genel
| Komut | Açıklama |
|-------|----------|
| `?help` | Komut listesi |
| `?test` | Bot çalışıyor mu kontrol |
| `?info` | Bot sürüm bilgisi |
| `?disc` | Discord sunucu linki |
| `?tr` / `?en` | Bot dili Türkçe/İngilizce |
| `?m` | Test mesajı |

### Oyuncu / İstatistik
| Komut | Açıklama |
|-------|----------|
| `?ping` | Ping kontrol |
| `?w` | War help (Discord @here, 3 dk cooldown, dile göre metin) |
| `?pl [id]` | Oyuncu istatistiği (packetloss/FPS/ping) |
| `?gst` | Bulunulan zone + aktif oyuncu sayısı |
| `?gstats [id]` | Grup istatistikleri (Discord'a gönderir) |
| `?astats <id>` | Account istatistikleri |
| `?alog <isim\|id\|a:id\|o:id>` | Admin log özeti. Düz sayı = **oyun içi id** (tab menüsü), `a:` = **hesap id** (`/stats`), `o:` = oyun içi id (açık yazım) |
| `?az` | Serbest zone bilgisi |

### Zone / Tarama
| Komut | Açıklama |
|-------|----------|
| `?zlist` / `?zwho` | Bulunulan zone bilgisi (/gzinfo) |
| `?zscan on/off` | Otomatik zone tarama aç/kapa |
| `?zgo <#N\|tag\|isim>` | Zone'a ışınlan, 40 sn bekle, geri dön |
| `?za` / `?zattack` | Zone saldırısı (/gz) |
| `?zsnip [zone]` | Zone sniper (savaş bitince /gz, Co-Leader yetki) |
| `?zf` | Flashing zone dağılımı + bilinmeyenleri tara |

### Grup / Tag
| Komut | Açıklama |
|-------|----------|
| `?zgrp` | Grup tag harvest (manuel tarama) |
| `?tagscan` | `?zgrp` alias |
| `?tag <isim> <tag>` | Grup tag'ini elle kaydet (yerel + DB) |
| `?gname` | Grup adını yeniden öğren |
| `?grole <isim> <dep\|leader\|co\|memb\|trial\|passive\|guest>` | Oyun içi rol değiştirme (Deputy Owner/Leader/Co-Leader/Member/Trial/Passive/Guest) |
| `?grank <isim> <rank>` | Oyun içi rank değiştirme |
| `?ginvite <id>` | `?ginv` alias — gruba oyuncu davet et (sonuç gruba iletilir) |

### Savaş
| Komut | Açıklama |
|-------|----------|
| `?gzt on/off/time` | Savaş geri sayım duyuruları (eşik geçişli) |
| `?gz` / `?gz2` | Savaşa katıl / yedek çağır |
| `?post [neden]` | Son grup aksiyonunu Discord embed olarak logla (30 sn gerekçe penceresi) |

### Güvenlik (yalnızca PM + yetkili isimler)
| Komut | Açıklama |
|-------|----------|
| `?changepass <mevcut> <yeni>` | Oasis şifre değiştirme (önce mevcut, sonra yeni şifre menüsü) |
| `?changemail <sifre> <eposta>` | Oasis e-posta değiştirme (`?email` alias; önce şifre, sonra e-posta menüsü) |

### Ayarlar
| Komut | Açıklama |
|-------|----------|
| `?greet on/off` | Welcome mesajları |
| `?welcomemsg <isim> <mesaj>` | Per-user welcome mesajı (kişi bazlı) |
| `?gm <mesaj>` | Global welcome mesajı alias |
| `?discordchat on/off` | Discord grup chat köprüsü |
| `?guildid <id>` | HWID'i Discord sunucusuna bağla |

---

## 🤖 Discord Slash Komutları

Komut adları sunucu diline göre lokalize edilir (EN taban, TR lokalizasyon).

| Komut (EN / TR) | Açıklama |
|-------|----------|
| `/link <hwid>` | HWID'i Discord sunucusuna bağla (yönetici) |
| `/unlink <hwid>` | Bağlantıyı kaldır (yönetici) |
| `/setup` / `/kurulum` | Kanalları oluştur (boş) / sil (`kapat`) (yönetici) |
| `/setchannel` / `/kanalayarla` | Kanal ataması: war, gstats, group-chat, zone-war, backup, group-log, bot-commands |
| `/help` / `/yardim` | Komut listesi; `paylas:true` ile listeyi bot-komut kanalına bırakır |
| `/gstats [group_id]` | Grup istatistikleri (oyundan çeker, ephemeral döner) |
| `/stats [id]` | Grup/oyuncu istatistikleri (boş = kendi) |
| `/astats <account_id>` | Hesap istatistikleri |
| `/admins` | Online admin listesi |
| `/alog <oyuncu\|id>` | Admin ceza geçmişi (ban/mute/kick) |
| `/glog <oyuncu\|id>` | Grup rol/rank değişiklik geçmişi |
| `/oyuncu <isim\|id>` | Cache'deki packetloss/FPS/ping değerleri (yaş damgalı) |
| `/bolge <grup\|tag>` | Grubun tag'i + sahip olduğu zone'lar (DB `gangs`) |
| `/botdurum` | Bot kaydı: grup adı + zscan/gzt/zgrp durumu (DB `bots`) |
| `/send <komut>` | Oyun içine serbest komut/mesaj gönder (yönetici) |
| `/pm <oyuncu> <mesaj>` / `/mesaj` | Oyuncuya özel mesaj; yanıt ephemeral döner |
| `/duyuru <mesaj>` | Grup chatine duyuru at (oyun içi kalır, köprü kanalına gitmez) |
| `/backup <grup> [mesaj]` | Acil durum çağrısı (@here) |
| `/zonewar ...` | Manuel ZONE WAR bildirimi |
| `/grole [name] [role]` | Üyeye rol ver (Member/Co-Leader) — interaktif üye/rol menüsü |
| `/grank [name] [rank]` | Üyeye rank ver — interaktif üye/rank menüsü |
| `/gkick [name] [reason]` | Üyeyi kickle — sebep modalı + sayfalı liste + dialog otomasyonu |
| `/ginv <id>` | Gruba oyuncu davet et (`/ginvite id`) |
| `/gmembers` | Üye listesi + VIP harvest → Google Sheets roster |
| `/dil <tr\|en>` | Bot dili (embed + oyun içi duyurular; client 2 sn'de senkronlar) |
| `/kopru [on\|off]` | Discord ↔ oyun grup chat köprüsü |
| `/zscan [on\|off\|sn]` | Otomatik zone tarama (`sn` = tek seferlik tam doğrulama) |
| `/gzt [on\|off\|time]` | Zone savaşı geri sayım duyuruları |
| `/gname` | Grup adını yeniden öğren |
| `/setallowedrole [rol]` | Grup komutları için izinli rol (boş = herkes) |

---

## 🗄️ Veritabanı (MySQL `raksamp`)

| Tablo | Açıklama |
|-------|----------|
| `gangs` | Grup adı (PK), tag, zones JSON — kalıcı zone sahiplik haritası |
| `bots` | Bot başına ayarlar: group_name, zscan/gzt/zgrp on-off |
| `licenses` | HWID lisansları + welcome mesajı/enabled |
| `welcome_per_user` | Kişi bazlı welcome mesajları (hwid + isim) |
| `guilds` | Discord sunucusu ayarları (kanallar, allowed_role_id, lang) |
| `players` | Oyuncu istatistik cache (packetloss/FPS/ping) |
| `admin_logs` | Admin eylem logları |
| `group_logs` | Grup rol değişikliği logları |
| `roster_messages` | `/gmembers` bitiminde kanala atılan/düzenlenen roster link mesajı |

---

## 📁 Proje Yapısı

```
client/           C++ client (RakSAMPClient.exe + XML config)
  src/            netrpc.cpp, Security.cpp, cmds.cpp, main.cpp...
  bin/            RakSAMPClient.exe, RakSAMPClient.xml, group_colors.txt
raknet/           RakNet + SA-MP RPC katmanı (SAMPRPC.cpp)
Discord/          Node.js backend (server.js, db.js, commands/, models/)
CHANGELOG.md      Sürüm geçmişi (Türkçe + English)
```