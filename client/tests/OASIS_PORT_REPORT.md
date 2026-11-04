# Oasis uyarlaması — 4 Kasım 2026

Hedef önce tarandı. Başlangıç sürümü: `d97772c`. UIF dosyalarının tamamını kopyalamak yerine Oasis akışlarına uyarlama yapıldı.

## Taramada görülen farklar

- Backend portu 3000, veritabanı `raksamp_oasis`; `Discord/config.js` korunur.
- Oyun adresi, bot adı, parola ve normal konum XML'deki mevcut değerleriyle korunur.
- Oasis zone numarası ile dahili zone indeksi arasında bir fark vardır; replay verileri buna göre düzenlendi.
- Giriş/mod seçimi, iki aşamalı parola değiştirme, üye listesi, takip/whisper, dialog izleyicisi ve `/fr` yeniden denemeleri korunur.
- Bağlantı kurulamadığında devreye giren 60 saniyelik watchdog korunur.
- Savaş sonucu iki ayrı tablo yerine tek kutu gelir. Başlangıç duyurusu `Started at ... - A vs. B.` biçimindedir.

## Aktarılan değişiklikler

- Paket okumalarında uzunluk, oyuncu/zone kimliği ve okuma başarısı kontrolleri; dialog verisi tamamlanmadan etkin dialog değiştirilmez.
- Tam bot adı, MachineGuid ve disk bilgisinden SHA-256 ile üretilen HWID v2; eski lisanslar için önizlemeli, işlemsel geçiş aracı.
- Sınırlı HTTP kuyruğu, süre sınırları, ana döngüde cevap işleme ve bağlantı kesilince eski cevapların geçersizleştirilmesi. İlk lisans el sıkışması süre sınırlı senkron çalışır.
- URL sorgu değerlerinin kodlanması ve `?zsnip` komutunun tam eşleşmesi.
- Yeniden bağlantıda geçici otomasyon durumunun temizlenmesi; ortak işlem kuyruğu, `?durum`/`?status`, `?iptal`/`?cancel`.
- Savaş sonuçlarının atomik dosya kuyruğunda saklanması; backend'de kalıcı teslim kaydı ve belirsiz teslimde Discord geçmişinden doğrulama.
- Oasis başlangıç duyurusundan sabit savaş bağlamı; yanlış takımlı, erken ve yinelenen sonuçların elenmesi, ters takım sırasının desteklenmesi. Beraberlikte ayrıca doğrulanmış kazanma/kaybetme sinyali gerekir.
- Günlük dosyalarına eklemeli log ve gizli veri maskeleme; XML'de merkezi otomasyon ayarları. Oasis tarama örnekleme süresi 7000 ms olarak korunur.
- Gerçek işleyicileri kullanan, sanal saatli ve oyun bağlantısı açmayan replay testleri.

Oasis backend'in mevcut Discord komut cevap akışı korunur. UIF'e özgü grup komutu onay takip sistemi doğrudan taşınmadı; yeni istemcide bu durum bildirimleri yerel loga yazılır.

## Doğrulama

Release/Win32 istemci derlemesi ve dört C++ test projesi kullanılır. Beş genel replay senaryosuna ek olarak üç Oasis senaryosu tek kutulu sonuç, yanlış takım, tekrar gelen dialog, ters takım sırası, yenilgi ve beraberlik için kesin sonuç şartını sınar.

Yeni HWID geçişi ve savaş teslimi JavaScript selftest'leri, bağımlılık taklitleriyle V8 ortamında çalıştırıldı. Savaş sonucu route'u ayrıca tek kutu, yanlış grup reddi ve belirsiz teslimde 503 cevabı için sınandı. `server.js` ve `db.js` sözdizimi doğrulandı. Makinede Node/Bun bulunmadığı için tam backend test paketi çalıştırılamadı.

Canlı oyun, Discord veya MySQL üzerinde işlem yapılmadı. Yeni HWID lisansının eski kayıtla eşleştirilmesi için README'deki geçiş aracı kullanılabilir; canlı veritabanında otomatik çalıştırılmaz. Uygulamadan önce değişen mevcut dosyalar `migration-backups/` altına yedeklenir; yeni dosyalar ve özetler manifestte listelenir.
