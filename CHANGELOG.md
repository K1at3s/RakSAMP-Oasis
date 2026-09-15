# beta v4 - 2026-09-14 (beta branch)

## Türkçe
- **Sniper saldırı-doğrulama**: her adımda `/gzinfo` ile sahip zincir grubuyla karşılaştırılıyor; uyuşmazsa `!Zone #N artik X degil (Y), atlaniyor.` + sıradaki zone (AnR→A$AP vakası kapandı)
- **Sahte win kilidi**: `/gz` sonrası savaş gerçekten başlamadan sonuç yazılmıyor (`g_ztkWarSeen` + 20 sn); `This zone can be attacked in` gametext'i anında fail
- **Cooldown okuma**: `can be attacked in` süresi 5 dk üstüyse saldırmadan atlanıyor
- **Savaşta bekleme**: kendi savaşı sürerken `/gz` atılmıyor, `won` gelir gelmez saldırılıyor
- **`?yes/?no` yanıtı**: bekleyen zincir sorusu yoksa sessiz kalınmıyor

## English
- **Snipe verify-before-attack**: each step checks owner via `/gzinfo` against the chain group; on mismatch `!Zone #N is no longer X (Y), skipping.` + next zone (AnR→A$AP case closed)
- **Phantom-win lock**: no outcome until the war actually starts after `/gz` (`g_ztkWarSeen` + 20s); `This zone can be attacked in` gametext fails fast
- **Cooldown read**: zones with `can be attacked in` over 5 min are skipped without attacking
- **Wait while own war runs**: no `/gz` mid-war, attacks the moment `won` arrives
- **`?yes/?no` replies**: no more silence without a pending chain question

# beta v3 - 2026-09-14 (beta branch)

## Türkçe
- **5 periyot döngü**: tarama artık 5 turda bir dönüyor (0-1. turlar tam kontrol, 2-4. turlar bilinmeyen-only)
- **?zscan sn (yeni)**: istenince tüm haritayı tek seferlik tam doğrular; bitince DB senkronu tek-sahipliği kurar (yeni gruba yazar, eskisinden siler); sniper sırasında istenirse kuyruğa alınıp sniper bitince koşar
- **Çevrimdışı sahip düzeltmesi**: üyeleri offline grubun zone'u `Free` sanılıp sniper tarafından vuruluyordu; bilinen isim artık korunuyor

## English
- **5-period cycle**: scan now runs in 5-tour cycles (tours 0-1 full verify, tours 2-4 unknown-only)
- **?zscan sn (new)**: on-demand full-map verification; completion sync enforces single ownership (write to new group, delete from old); queued behind active sniper
- **Offline-owner fix**: zones of offline-member groups were mistaken for `Free` and sniped; known name is now kept

# beta v2 - 2026-09-13 (beta branch)

## Türkçe
- **?alogs slot→isim**: sayı `<1000` ise canlı `playerInfo` + oturum cache'inden isme çevrilir; ikisi de yoksa 120 sn'den taze DB satırına bakılır, o da yoksa yanlış kişi yerine `Slot cevrimdisi` mesajı (`?alogs 45` → uL.Hold vakası kapandı)
- **account-ID banlar**: `banned account ID 3292655` mesajında `/astats` ile isim çözülüp isim + account ID birlikte kaydedilir (kuyruklu, 15 sn fallback); `?alogs 3292655` çalışır
- **Temp hesap filtresi**: `_tmp+sayı` ile biten isimlerin action'ları DB'ye yazılmaz
- **?alog/?alogs çakışması**: çıplak `?alogs` artık `s` harfini sorgulamıyor; boş sorguda kullanım mesajı
- **?zsnip tarama duraklatma**: sniper boyunca zone taraması durur, bitince kaldığı yerden devam eder (GeMs vakası: tarama `/pos`'u sniper `/pos`'uyla çakışıp yanlış zone'a saldırtıyordu)
- **Çevrimdışı sahip koruması**: üyeleri offline grubun zone'u artık `Free` işaretlenmiyor, bilinen isim korunuyor (`?zsnip Free` Dev'in zone'una saldırmaz)
- **player_cache.txt kaldırıldı**: `?pl` FPS/ping + slot çözümleme DB'deki `players` tablosundan (`GET /api/player/cache`, yaş damgalı)
- **Bayat flash temizliği**: 15 dk tazelenmeyen flash bayrakları süpürülür, gün değişiminde sıfırlanır
- **Açılış tam kontrolü**: ilk tarama flash'a bakmadan tüm zone'ları tek tek doğrular (sessiz el değiştirmeler yakalanır)
- **Kill-list Total hizası**: Total satırı K/A/D/DMG sütunlarının altına denk geliyor

## English
- **?alogs slot→name**: numbers `<1000` resolve via live `playerInfo` + session cache; otherwise a fresh (<120s) DB row; if none, an honest `Slot offline` message instead of a wrong person (`?alogs 45` → uL.Hold case closed)
- **Account-ID bans**: `banned account ID 3292655` resolves the name via `/astats` and saves name + account ID together (queued, 15s fallback); `?alogs 3292655` works
- **Temp account filter**: actions of names ending `_tmp+digits` are not written to DB
- **?alog/?alogs collision**: bare `?alogs` no longer queries the letter `s`; usage message on empty query
- **?zsnip scan pause**: zone scan pauses during snipe, resumes where it left off (GeMs case: scan `/pos` clashed with sniper `/pos`+`/gz`, attacking the wrong zone)
- **Offline-owner guard**: zones of offline-member groups are no longer marked `Free`, known name is kept (`?zsnip Free` won't attack Dev's zone)
- **player_cache.txt removed**: `?pl` FPS/ping + slot resolution come from DB `players` table (`GET /api/player/cache`, age-stamped)
- **Stale flash cleanup**: flash flags untouched for 15 min are swept, cleared on day change
- **Boot full verify**: first scan verifies every zone regardless of flash (silent takeovers get caught)
- **Kill-list Total alignment**: Total row lines up under K/A/D/DMG columns

# v1.7.1 - 2026-09-12 (beta branch)

## Türkçe
- **?gkick sebep BYTE uzunluk**: kick sebep dialogu inputu BYTE okuyor (WORD NUL ile başlayıp sebebi boşaltıyordu); sebep artık dolu gidiyor
- **Actor parse (`Name(acc)`)**: kick + rol/rank loglarında isim/parantez ayrımı düzeltildi (`tefeci(3279409)` önceden acc'yi isim sanıyordu)
- **Dialog çapraz-kimlik koruması**: üye listesi olmayan dialog harvest/rol aramasına girmiyor (`Last Active` imzası); Learn/harvest adımı da aynı şekilde korunuyor
- **?zgo sayısal tag**: aralik dışı sayı tag/isim olarak denenir (`?zgo 737` → `7 Young West Java`); Free her tur yeniden doğrulanır; sayısal tag isimle gösterilir
- **SmartToUtf8 rewrite**: karışık buffer (oyun cp1254 + DB/Discord UTF-8) bayt-bayt doğru çevrilir

## English
- **?gkick reason BYTE length**: kick reason dialog reads BYTE length (WORD started text with NUL, emptying the reason); reason now arrives filled
- **Actor parse (`Name(acc)`)**: name/paren split fixed in kick + role/rank logs
- **Dialog cross-identity guard**: non-member dialogs no longer enter harvest/role search (`Last Active` signature); Learn/harvest step guarded the same way
- **?zgo numeric tag**: out-of-range numbers tried as tag/name (`?zgo 737` → `7 Young West Java`); Free re-verified every tour; numeric tags displayed by name
- **SmartToUtf8 rewrite**: mixed buffers (game cp1254 + DB/Discord UTF-8) converted byte-correct

# v1.7.0 - 2026-09-11 (beta branch)

## Türkçe
- **?zsnip süren savaş algılama**: komut verilirken hedef zone flashing (kırmızı yanıp sönüyor) ise düşman savaşı sürüyor demektir — bot artık savaşın ortasında `/gz` atmıyor, "takes over/wins against/loses to" mesajıyla bitişi bekliyor; zincir kararı (sonraki zone'a geç) wins/loses formatında da tetikleniyor
- **?zf savaş koruması**: savaş/kill-list yakalayıcı aktifken flashing zone taraması (teleport + `/gzinfo`) erteleniyor; savaş bitince otomatik tekrar denenir — tarama artık Zone War verisini bozmuyor
- **Log sıfırdan yazılır**: `RakSAMPClient.log` her açılışta eski içerik silinerek başlar (`"wb"` modu + UTF-8 BOM; eskiden var olan log'un üstüne ekleniyordu)
- **?gkick / /gkick (yeni)**: sayfalı üye listesi + sebep modalı (boş bırakılabilir) + dialog otomasyonu (ara + tıkla + sebep gönder); sebep dialogu (`UIF - Group Kick`, INPUT) faz-2 tanıma; sebep son tıklanan item ile gönderilir
- **?ginv / /ginv (yeni)**: `/ginvite id` ile davet (oyun içi + Discord, aynı yetki kapısı)
- **?grank prefix eşleşme**: `mans` → `Manslaughter` (önce satır-başı prefix, sonra substring); faz-2 tanıma tıklama kanıtıyla (özel rank listelerinde `Co-Leader`/`Member` şartı kalktı)
- **gmembers sayfalama**: `Button2 ' >>'` fallback + `cancel/ok` = son sayfa (25 yerine 127 üyenin tamamı); sıra üyeler → gvips (tabloya yaz) → astats → gönderim
- **gmembers parse**: rank/role/last active/VIP generic (çok kelimeli rank, `Non GZ`, `Diamond (14 days)`, son kolon last active); account ID case-insensitive; tabloda kaydı olan atlanır (`/api/gmembers/known`)
- **Sheet**: VIP satırı tier renginde boyanır, VIP kolonu 9px font; değişiklik yoksa skip ama `Updated` damgası her koşulda tazelenir; anti-nuke (yarıdan küçük listede silme yok); damga TR saati (UTC+3)
- **Summary sekmesi**: Total Members / VIP / Updated + sayılar (dilli: TR/EN)
- **Discord**: `/gmembers` ephemeral ack + bitince kanala 📊 link + last updated (aynı mesaj düzenlenir, `roster_messages` tablosu)
- **Admin/grup log ayrımı** (`ADMIN:` ↔ `GROUP:`) + kick actor parse düzeltmesi (`Name(acc)`)
- **Mute**: gametext + chat tespiti (griefer korumalı `Isim(id):` filtresi), anında reconnect; `LearnGroupTag` kirlilik koruması (üye/rol listeleri gangs'e yazılmaz)
- **?zgrp bootstrap** (boş listede keşif), scan/harvest sırası + 30sn spawn koruması, restore-path race fix
- UTF-8 log koruması (cp1254 kaynaklar bayt-güvenli düzenlenir)
- **?zf tag yükleme**: DB tek kaynak; `LoadGroupsFromDb` artık sadece açılışta değil, periyodik sync'te **ve her `?zf` isteğinde** yeniden çekiliyor → DB'de var olan ama tek seferlik GET'e denk gelmeyen tag'ler de hafızaya yükleniyor; `?zf` isim yerine kısa tag gösterir
- **?gmembers Updated saati**: `sheets.js` damgası `Intl.DateTimeFormat` timezone'una bağlıydı; runtime'da timezone verisi yoksa sessizce UTC'ye düşüp 3 saat geriye yazıyordu (23:20 yerine 20:20). Artık sabit UTC+3 aritmetiğiyle deterministik hesaplanıyor

## English
- **?zsnip ongoing-war detection**: when the command fires while the target zone is flashing (red blink), an enemy war is in progress — the bot no longer sends `/gz` mid-war, it waits for the end via "takes over/wins against/loses to" message; chain decision (advance to next zone) now also triggers on wins/loses format
- **?zf war guard**: flashing-zone scan (teleport + `/gzinfo`) is deferred while a war/kill-list capture is active; automatically retried after the war ends — the scan no longer corrupts Zone War data
- **Log starts fresh**: `RakSAMPClient.log` is truncated on every launch (`"wb"` mode + UTF-8 BOM; previously appended to the existing log)
- **?gkick / /gkick (new)**: paginated member list + reason modal (optional) + dialog automation (search + click + submit reason); reason dialog (`UIF - Group Kick`, INPUT) phase-2 detection; reason sent echoing last clicked item
- **?ginv / /ginv (new)**: invite via `/ginvite id` (in-game + Discord, same permission gate)
- **?grank prefix matching**: `mans` → `Manslaughter` (line-start prefix first, substring fallback); phase-2 detection via click evidence (no more `Co-Leader`/`Member` body requirement on custom rank lists)
- **gmembers pagination**: `Button2 ' >>'` fallback + `cancel/ok` = last page (all 127 members, not 25); order members → gvips (write) → astats → send
- **gmembers parsing**: generic rank/role/last-active/VIP (multi-word ranks, `Non GZ`, `Diamond (14 days)`, last column last-active); case-insensitive account ID; skip `/astats` for table-known IDs (`/api/gmembers/known`)
- **Sheet**: full-row VIP tier colors, 9px VIP font; skip-if-unchanged but `Updated` stamp always refreshes; anti-nuke (no deletes when new list < half of old); TR-time stamps (UTC+3)
- **Summary tab**: Total Members / VIP / Updated + counts (localized TR/EN)
- **Discord**: ephemeral `/gmembers` ack + 📊 link + last updated posted to channel on completion (same message edited, `roster_messages` table)
- **Admin/group log split** (`ADMIN:` ↔ `GROUP:`) + kick actor parse fix (`Name(acc)`)
- **Mute**: gametext + chat detection (griefer-safe `Name(id):` filter), instant reconnect; `LearnGroupTag` pollution guards
- **?zgrp bootstrap** (discovery on empty list), scan/harvest ordering + 30s spawn guard, restore-path race fix
- UTF-8 log safety (cp1254 sources edited byte-safe)
- **?zf tag loading**: DB is the single source; `LoadGroupsFromDb` now re-pulls not only at startup but on every periodic sync **and on every `?zf` request** → tags that exist in the DB but missed the one-shot GET also get loaded into memory; `?zf` shows the short tag instead of the full name
- **?gmembers Updated time**: `sheets.js` stamp relied on `Intl.DateTimeFormat` timezone; when the runtime lacks tz data it silently fell back to UTC, writing 3h behind (23:20 shown as 20:20). Now computed deterministically with a fixed UTC+3 offset

# beta — 2026-09-07 (beta branch)

## Türkçe
- `/pm` hedef çözümü + yanıt: `playerInfo` + `g_playerCache`; `g_pmId` ile ID eşleşmesi; Discord embed `targetName` ile güncellenir
- Console/listbox UTF-8 (SmartToUtf8/SmartToCp1254, `DEFAULT_CHARSET` + `ExtTextOutW`, kod sayfası 65001)
- `?zsnip` tag→zone DB fallback bozulduğunda `/api/groups/zone`
- `?alog`/`?alogs` DB sorgusu `MAX(target_name)` + uL.Hold formatı `ALogs for X: B:0 | M-Bans:1 | D:0 ...`
- `?zf` tag placeholder (`tag=isim`) → `tag=""`, `/api/groups` upsert boş tag ile ezmez, `?zf` isim yerine tag gösterir
- Bozuk gruplar temizlendi (`-`, `.`, `...`, `GS` duplicate vb. — 7 satır), renk kodu parantezli `(RRGGBB)` sızıntısı kapatıldı
- `g_spawnTick` → tag harvest spawn'dan 60 sn koruma; `?zgrp on` doğrudan harvest başlatır
- `ScrGameText` `not in a zone war` → savaş durumu tam sıfırlanır
- `IsSniperTargetZone()` + `takes over zone '# N'` mesaj bazlı hedef savaş takibi (timer'a güvenmez), savaştaki zone atlasınır
- `MAX_GROUP_COLORS` 128→512, servis hesabı tek `models.js`

## English
- `/pm` target resolve + reply via name/ID; Discord embed shows resolved in-game name
- Console/listbox UTF-8, Turkish chars fixed
- `?zsnip` DB fallback for zones, `?alogs` uL.Hold format, tag placeholder fixes, upsert guard, junk groups removed, spawn protection, war tracking via messages

# v1.6.1

## Türkçe
- **⚡ First Blood**: kill list embed'ine ilk kan eklendi — bot savaş başında 3 sn GZ'ye girerken chat feed'inden ilk kill yakalanır (`X has killed Y (Weapon: Z) (Distance: Nm)`), embed'de ⚡ First Blood alanı olarak gösterilir (silah + mesafe ile).
- **MVP / Top Feeder inline**: kill list embed'inde ⭐ MVP ve 🎯 Top Feeder yan yana gösterilir.
- **README iki dilli**: `README.md` (Türkçe) + `README_EN.md` (English), komut listeleri güncellendi (yeni komutlar: `?post`, `?grole`, `?grank`, `?changepass`, `?email`, `/pm`, `/setup` lokalizasyonu).
- **CHANGELOG iki dilli**: her sürüm Türkçe + English bölümleriyle.

## English
- **⚡ First Blood**: added to the kill list embed — while the bot enters the GZ for 3s at war start, the first kill is captured from the chat feed (`X has killed Y (Weapon: Z) (Distance: Nm)`) and shown as a ⚡ First Blood field (with weapon + distance).
- **MVP / Top Feeder inline**: ⭐ MVP and 🎯 Top Feeder now sit side by side in the kill list embed.
- **Bilingual README**: `README.md` (Turkish) + `README_EN.md` (English), command lists refreshed (new commands: `?post`, `?grole`, `?grank`, `?changepass`, `?email`, `/pm`, `/setup` localization).
- **Bilingual CHANGELOG**: each version now has Turkish + English sections.

# v1.6.0

## /grole / /grank — tam otomasyon
- **Çift tıklama + inputtext**: UIF diyaloğu üyeyi seçili satırın İLK SÜTUNU (örn. `40  cb650r`) ile tanıyor; üye çift tık + inputtext ile seçiliyor, rol/rank diyaloğu tanınıp faz 2'ye geçiliyor, rol de çift tık + inputtext ile veriliyor. `Buton 0 = sayfalama` (item yok sayılır), seçim `buton 1` ile.
- **Oyun içi `?grole <isim> <memb|co>`** ve **`?grank <isim> <rank>`**: grup chat'inden yetkisiz diyalog akışı olmadan aynı otomasyonla rol/rank değiştirir (memb/mem/member → Member, co → Co-Leader).
- **?grole / ?grank menü lokalizasyonu** (TR/EN).

## Güvenlik komutları (?changepass / ?email)
- Yalnızca **PM + yetkili isimler** (Oreyoo, hakixld, cb650r) kullanabilir; `?help`'te görünmez.
- UIF `/changepass` ve `/email` **input menüsü** açar: değer saklanır, menüye yazılıp submit edilir (max 2 adım: şifre + onay). Onay `/r` ile PM'e döner.

## Grup log sistemi
- **`?post [gerekçe]`**: son grup aksiyonunu (rol/rank değişikliği, kick) `bot-grouplog` kanalına embed olarak atar — Aksiyon/Yapan/Üzerinde/Değişiklik/Gerekçe (dile göre; gerekçe yazılmazsa 30 sn pencere, "Gerekçe verilmedi").
- Kick tespiti eklendi (`GROUP: X kicked Y`).
- `?post` artık `/rankreason` göndermiyor (sunucuda yoktu); eski `?post` yerine hepsi bu akışta.

## Zone War / Kill List
- **War-end tespit düzeltildi**: `?za`/`?gz` cooldown'ı war-end zamanlayıcısını kirletiyordu — "loses to" mesajı yutuluyor, savaş hiç bitmiyordu (`war=1` takılı). Ayrı zamanlayıcıya alındı.
- **Kill list finalize düzeltildi**: savaş bitişinde `g_zwKillListSent` erken true yapılıyordu (sonuç asla gitmiyordu); yeni savaş başlarken bekleyen veri finalize ediliyor.
- **Yeni kill list embed**: kazandı → 🏆 Group Zone War Won / kaybetti → 💀 Group Zone War Lost; kırmızı vs mavi takım + tag'ler, Score-Kills, iki takım kutusu (Name/K/A/D/DMG hizalı + Total — client hesaplar), ⭐ MVP (isim + dmg, kills) ve 🎯 Top Feeder (isim + deaths).
- **?gzt eşik geçişi**: tam saniye eşleşmesi yerine eşik GEÇİŞİ tespiti (10/3/1 garanti, 9:xx geçişte).

## Tag / Harvest
- **Otomatik tag harvest KAPALI varsayılan**: girişte otomatik kayıt yok; elle `?zgrp` (diğer gruplar) / `?tagscan` (manuel tarama) ile tetiklenir.
- **`~` işaretli no-tag gruplar** bir daha taranmaz; `?tag` kısmi isim eşleşmesi (`didnt need` → "Didn't need a group"), apostrof yok sayılır.
- Harvest bitiş duyurusu gruba artık ATILMAZ (sessiz).

## Discord tarafı
- **`/kurulum`** (create) / **`/kurulum kapat`** (off): kanal kur/sil — OTOMATİK kanal oluşturma KALDIRILDI.
- **`/mesaj` (pm)**: oyuna `[Discord][isim]: mesaj` gönderir; oyuncu yanıtlarsa etiketli + ephemeral döner (15 dk).
- **`/kanalayarla`, `/iznirol`** ve tüm komutlar **discord.js lokalizasyonu** ile (TR/EN adlar).
- **`?w`** 3 dk cooldown, `@here`, düşman grup + dile göre metin. Köprü `(In-Game)`/`(Oyun)`.
- **HWID onay**: yeni cihazda sahibin kanalına embed + Onayla/Reddet butonları; kategori `<grup> - Logs/Logları` otomatik.
- **bot-commands/bot-komut** kanalı, `bot-grouplog` DB alanı, `?post` embed'i dile göre.
- Welcome mesajları artık **Discord'a gitmiyor** (oyun içi kalır; kanal/DB alanı kaldırıldı).
- `BOT_KOMUTLARI.txt`: tüm komutlar + işlevleri.

# v1.5.1

## Spawn / Konum
- **Virtual world spawn fix**: spawn menüsü "Freeroam" cevabı GTA-client formatına (BYTE uzunluk) çevrildi — bot artık gerçek freeroam'da spawn oluyor (SF kulesi/virtual world'de kalmıyor).
- **`?zgo` yerin dibi fix**: teleport Z 25 → 100 (yüksekten düşüp zemin Z'sine oturur).

## Tag / Grup
- **`?tag <isim> <tag>`**: grup tag'ini elle kaydet — yerel dosya + DB'ye isim eşleşmeli push (sunucu komutu göndermez). `?zgo GS` gibi tag ile zone'a gitme çalışır.
- Tag boş/placeholder (tag==isim) gruplar `?zgrp`'ta yeniden taranır.

## Karakter / Diğer
- Welcome mesajı İngilizce şubeye `!` eklendi (grup chat'e düşer, normal chate değil).
- Backend kapalıysa client kendini kapatır (60 sn kontrol) + temiz Türkçe uyarı.
- README güncellendi: tüm bot komutları, Discord slash komutları, DB şeması.

# v1.5.0

## UIF Login Fix (tmp hesap çözümü)
- **Login dialog submit GTA-client formatina cevrildi**: BYTE uzunluk + listitem 0xFFFF. Önceden WORD uzunluk gönderiliyordu; sunucu şifreyi yanlış parse edip botu tmp hesaba atıyordu. Artık SA-MP client ile birebir aynı şekilde hesaba giriliyor.
- **Login bekleme mekanizmasi**: baglantida spawn isteği login tamamlanana kadar ertelenir; "You logged in" mesajı sonrası 3 sn beklenip sunucunun oturumu işlemesi sağlanır (tmp hesaba düşme engellenir).
- **Grup adı öğrenme düzeltildi**: `/stats` → "Group" butonuna bas → ikinci diyalogdan doğru grup adı okunur. Harvest ile çakışma, spawn öncesi gönderim, sonsuz kilitlenme giderildi. Doğru öğrenildi: `uL - watchin' u gaze`.

## /grole / /grank (Discord interaktif)
- **Discord `/grole`**: paginated üye listesi butonlu sayfalama (◀ ▶) → üye seç → rol seç (Member/Co-Leader) → oyunda paginated listede hedef aranıp uygulanır. Direkt `/grole name role` da çalışır.
- **Discord `/grank`**: üye seç → rank seç (Member/Co-Leader/Leader/Elder/Veteran/Recruit).
- **DB izinli rol kontrolü**: `/setallowedrole` ile menüleri kullanabilecek rol ayarlanır (DB `allowed_role_id`); boşsa herkes.
- Sayfalama rate-limit dostu tek `editReply` ile; menüler 5 dk sonra otomatik kapanır.

## Welcome (per-user)
- **Per-user welcome**: `?welcomemsg <isim> <mesaj>` — giren kullanıcıya özel mesaj (DB `welcome_per_user`). `%s` desteği, `?welcomemsg list`, `clear`. Global `?welcomemsg <mesaj>` fallback olarak durur.
- Hoş geldin mesajları `!` ile grup chat'e (SAMP'ta `(GROUP)` olarak görünür).

## Zone / War
- **Harvest sonrasi otomatik zone taramasi**: grup harvesti bitince yeni veri varsa zscan off olsa bile tarama başlar.
- **`?zscan on`** savaş biteli uzun süre geçtiyse takılı kalmaz: `takes over` de grup adına bağlı işlenir + 15 dk stuck-savaş temizliği.
- **wins/loses her grup için değil**: sadece botun grubunun adı mesajda geçiyorsa işlenir.
- **?gzt** saniyesi saniyesine: tam eşikte duyurur (09:10/09:03/09:01/00:10/00:03/00:01), geç açılımda geçmiş eşiği tekrar duyurmaz.
- **Serbest zone listesi buffer fix** (scan sonrası crash 0xc0000409 giderildi), dialog satır buffer'ları 512'ye büyütüldü.

## Karakter / Kodlama
- **`/utf-8` derleme bayragi + UTF-8 BOM** tüm C++ kaynaklarında — emoji/karakterler doğru derlenir.
- Backend ve client'ta Türkçe özel karakterler ASCII'ye çevrildi (konsol kod sayfasından bağımsız); emojiler korundu.
- Backend açılış logları düzgün görünür (chcp 65001).
- `?zf`/`?pl` isimlerinde renk kodları temizlenir; chat çıkış kuyruğu 5 sn → 1 sn (çok parçalı mesajlar hızlı gider).