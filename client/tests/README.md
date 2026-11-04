# İstemci doğrulaması

Visual Studio C++ araçlarıyla `Release|Win32` yapılandırmasında şu projeleri derleyip çalıştır:

- `CommandTrackingTests.vcxproj`: komut sonuçları, üyelik/yetki ve savaş bağlamı.
- `ChatEncodingTests.vcxproj`: CP1254, UTF-8 ve karışık sohbet metinleri.
- `ClientSafetyTests.vcxproj`: kesilmiş bağlantı paketleri, geçersiz ID/uzunluklar, metin tampon sınırları, HWID, URL kodlama ve geciken/kapalı yerel HTTP sunucusu.
- `RuntimeReplayTests.vcxproj`: gerçek istemci işleyicileriyle giriş, yeniden bağlantı, sniper yetki diyaloğu, iptal, savaş duyuruları ve `?zsnip → /gz → savaş sonucu → ?yes → sonraki zone` zinciri.

Güvenlik testi yalnızca kendi `127.0.0.1` sunucusunu açar; oyuna veya gerçek backend'e bağlanmaz. Test çıktıları `obj/` altındadır.

Replay testini depo kökünden `client/tests/obj/runtime-replay/RuntimeReplayTests.exe` ile çalıştır. Senaryolar `fixtures/runtime_scenarios.json` içindedir; alternatif senaryo dosyası ilk argüman olarak verilebilir. Test gerçek mesaj, diyalog ve paket işleyicilerini kullanır; oyun/HTTP gönderimleri kaydedilir, ağ bağlantısı açılmaz. `advance` olayı sanal saati ilerletir. Üretimde saat Windows saatidir; replay yalnızca test programından etkinleştirilir.

Güvenlik testleri ayrıca bağlantı/grup/oyuncu değişiminde kuyruk izolasyonunu, diskte savaş kuyruğunu, kayıp teslim onayını, bozuk dosyanın korunmasını, reddedilen sonuçların ayrılmasını ve loglarda gizli bilgilerin maskelenmesini doğrular.

## Yeni HWID'ye geçiş

Yeni kimlik `BURHAN2-...` biçimindedir. Disk ve Windows MachineGuid bilgisi SHA-256 girdisine dahildir. Bot adının tamamı ayrı özetlenir; noktalama ve ilk 16 karakterden sonraki farklar kimliği ayırır. Kimlik bir yetkilendirme sırrı değildir.

Eski kimlikler çakışabildiğinden yeni istemci eski kimlikle otomatik lisans istemez. Yeni istemcinin ilk açılışında gösterdiği kimliği, ilgili eski kayıtla eşleştir.

Backend ve etkilenen istemciler kapalıyken `Discord/` içinde:

```text
bun migrateHwid.js ESKI_HWID YENI_HWID
bun migrateHwid.js ESKI_HWID YENI_HWID --apply
```

İlk komut yalnızca planı gösterir. İkinci komut lisansı, Discord bağlantısını, kişiye özel karşılama ayarlarını ve ilgili kayıtları tek veritabanı işlemiyle taşır. Yeni kimlikte ayar veya bağlantı varsa üzerine yazmaz. Birden fazla Discord sunucusuna bağlı eski kimliklerde önce belirsizlik giderilmelidir. Bu araç canlı veritabanında otomatik çalıştırılmaz.

## Canlı doğrulama

Otomatik testlerden sonra Oasis üzerinde giriş, yeniden bağlanma, grup değiştirme, `?zsnip` / `?zsnip off`, `?yes` ile sonraki zone, savaş sonucu ve Discord komut sonuçları denenmelidir. Eski istemci kapatılıp yeni derleme başlatılmalıdır.

## Kullanım ve toparlanma

- `?durum` / `?status`: bağlantı ve giriş durumu, aktif işlem, beklenen diyalog, işlem sayısı, backend durumu, bekleyen savaş sonuçları ve son hata.
- `?iptal` / `?cancel`: oyuncu kendi bekleyen işlemlerini iptal eder; kendi aktif işlemini de durdurabilir. Bot/konsol tüm manuel işlemleri ve bekleyen komutları iptal edebilir. Savaş takibi bu komutla silinmez.
- Diyalog ve teleport kullanan grup/Discord komutları ortak kuyruğa girer. Tarama, mevcut zone örneğinin sınırında manuel işleme yer verir; iş bittikten sonra devam eder. Yetki kontrolü hâlâ oyun sunucusunun güncel üye diyaloğuyla yapılır.
- Kopunca oyuncu slotları, diyaloglar, yarım işlemler, tarama ve savaşın geçici durumu temizlenir. Önceki bağlantının komutları çalıştırılmaz. Giriş tamamlanınca harita/ayarlar yeniden yüklenir. Önceki saldırı otomatik tekrar edilmez.
- Çalışırken backend kesilirse istemci oyunda kalır. İlk açılıştaki lisans kontrolü yine gereklidir.

İlk lisans kontrolü dışındaki HTTP çağrıları arka plandadır. DB yedek okumaları önbellekten yapılır ve yenileme başlatılır; ilk okumada henüz sonuç yoksa sonraki denemede kullanılabilir. Oyuncu istatistiklerinin yaş bilgisine önbellekte geçen süre eklenir. Ağ çalışanı oyun durumuna erişmez; tamamlanan yanıtlar ana döngüde uygulanır.

Loglar `logs/RakSAMPClient-YYYY-MM-DD.log`, `logs/RakSAMPClient-scan-YYYY-MM-DD.log` ve `logs/TextDraws-YYYY-MM-DD.log` dosyalarına eklenir. Önceki çalışmanın logu silinmez. Login/güvenlik girdileri, şifre ve token alanları maskelenir.

## Kalıcı savaş kuyruğu

Sonuç gönderilmeden önce `war-outbox-HWID.json` dosyasına atomik olarak yazılır; ekip, grup ve savaş kimliği sonradan değiştirilmez. Backend kapalıysa yeniden denenir, yeniden açılışta dosyadan devam edilir. Disk geçici olarak yazılamazsa sonuç o oturumda bellekte tutulur ve hata gösterilir. Bozuk kuyruk dosyasının üzerine yazılmaz; elle incelenmelidir.

Kalıcı olarak reddedilen 4xx sonuçları `.json.failed` dosyasında korunur; diğer sonuçların gönderimini durdurmaz. 408/429 ve 5xx hataları yeniden denenir.

Backend `war_result_receipts` tablosunu başlangıçta oluşturur. Teslim edilen savaş kimlikleri yeniden başlatmada da hatırlanır. Gönderim sırasında bağlantı kopup teslim belirsiz kalırsa, yeniden göndermek yerine Discord kanalının son 100 mesajında sabit sonuç işareti aranır. Bulunamazsa kayıt beklemede korunur; otomatik ikinci mesaj gönderilmez. Bu durumda kanal geçmişi/izinleri ve kayıt elle kontrol edilmelidir. Botun savaş kanalındaki mesaj geçmişini okuyabilmesi gerekir.

Backend testleri: `bun run test` veya `npm run test:node`. `warResultDelivery.selftest.js`, sahte MySQL ve Discord ile yeniden açılış, bot izolasyonu, belirsiz teslim, geçmişten doğrulama ve güvenli yeniden denemeyi sınar.

## Tek yerden ayarlar

`client/bin/RakSAMPClient.xml` içindeki `<automation>` alanı kullanılır. Eksik alanlar varsayılanı alır; geçersiz sayı/adres reddedilir. Değişiklikler yeniden başlatmada uygulanır.

| Alan | Varsayılan | İşlev |
| --- | --- | --- |
| `backend_url` | `http://127.0.0.1:3000` | Backend kök adresi; bu derleme HTTP destekler |
| `connect_timeout_ms`, `read_timeout_ms`, `write_timeout_ms`, `total_timeout_ms` | 2000 / 3000 / 3000 / 5000 | Ağ süre sınırları |
| `poll_interval_ms`, `db_sync_interval_ms` | 2000 / 60000 | Discord ve DB sorgu aralıkları |
| `scan_period_ms`, `scan_sample_ms`, `scan_teleport_ms`, `scan_gap_ms` | 900000 / 7000 / 3500 / 2000 | Tarama periyodu ve beklemeleri |
| `reconnect_ms` | 2000 | Yeniden bağlanmadan önce bekleme |
| `operation_timeout_ms`, `queue_timeout_ms` | 1800000 / 120000 | Aktif manuel işlem ve kuyruk süresi |
| `result_retry_ms` | 5000 | Savaş sonucu yeniden deneme aralığı |
| `war_notice_1`, `war_notice_2`, `war_notice_3` | 10 / 3 / 1 | Son dakika duyuru eşikleri; azalan sırada olmalıdır |


Oasis'e ait ek senaryolar: `client/tests/obj/runtime-replay/RuntimeReplayTests.exe client/tests/fixtures/oasis_native.json`. Bunlar tek kutulu sonuc, yanlis takim, tekrar gelen dialog, ters takim sirasi, yenilgi ve beraberlikte kesin sonuc sinyali gereksinimini dogrular.
