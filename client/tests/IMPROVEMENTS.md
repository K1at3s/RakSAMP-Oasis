# Backend, HTTP devamı ve otomatik doğrulama

Bu çalışma UIF ve Oasis için ayrı ayrı uygulanır. Canlı oyun sunucusu testi ve Oasis Discord komut onay takibi kapsam dışındadır.

## Backend testleri

`Discord/` içinde `bun run test` tam selftest paketini çalıştırır. Gerçek SQL testi için `bun run test:mysql` kullanılır; `TEST_MYSQL_URL` açıkça verilmelidir. Test, uygulamanın `config.js` dosyasını okumaz. Kendine ait rastgele `raksamp_test_...` şemasını oluşturur ve sonunda kaldırır.

Windows'ta ayrı bir MySQL sürecini geçici veri dizini ve boş yerel portla başlatmak için:

```powershell
./Discord/tests/Run-LocalMySql.ps1
```

Varsayılan MySQL yolu `C:/Program Files/MySQL/MySQL Server 8.4` değeridir; `-MySqlBase` ve `-Runtime` ile değiştirilebilir. Mevcut MySQL servisi, yapılandırması ve verileri kullanılmaz. Test süreci sonunda yalnızca başlatılan geçici sunucu kapatılır.

Gerçek SQL testi şema oluşturmayı, JSON sorgularını, HWID önizlemesini, geçişi ve hata halinde işlemin geri alınmasını; eşzamanlı teslim kaydı, yeniden başlatma ve bot izolasyonunu doğrular.

## İlk HTTP cevabında devam

Önbellek boşken zone/grup sorguları cevabı bekler, sonra özgün komut ortak işlem kuyruğuna döner. Üye listesi toplama, bilinen hesap sorgusundan sonra devam eder. `?welcomemsg list` ve oyun stats cevabı gelmeyince kullanılan HTTP yedeği de ilk cevabı işler. Ağ kapalıyken oyun üzerinden stats istemi yapılmaya devam eder.

HTTP okumaları birleştirilir. Önbellek 256 kayıt, aynı okumayı bekleyenler 16 kişi ile sınırlıdır. İptal, oturum/grup değişimi ve oyuncu slotunun yeniden kullanılması eski cevabın işlem başlatmasını engeller. Boş istatistik önbelleği `%0 / 0 FPS` yerine bilinmiyor olarak tutulur; bayat veya başka isimli DB istatistikleri kabul edilmez.

`client/tests/fixtures/cold_backend.json`, bu davranışları gerçek istemci işleyicileriyle ve geciktirilmiş HTTP tamamlanmalarıyla sınar.

## Otomatik derleme ve release

`.github/workflows/build-test-release.yml`, `beta` push'larında ve pull request'lerde backend selftest'lerini, MySQL 8.4 entegrasyon testini, Windows/Win32 derlemesini, C++ testlerini ve 60 saniyelik yük testini çalıştırır. Windows 2022 runner üzerinde v143 kullanılır; yerel projeler mevcut v145 ayarını korur.

Başarılı derlemenin EXE ve SHA-256 dosyası artifact olarak saklanır. `beta-v*` veya `v*` etiketi push edildiğinde, backend ve istemci işleri başarılıysa bu depoya ait artifact doğrulanır ve release'e yüklenir. Yapılandırma, parolalar ve çalışma dosyaları release paketine eklenmez.

Her pazartesi 02:30 UTC'de 20 dakikalık çevrimdışı dayanıklılık testi çalışır. Aynı test Actions ekranından `long_stress` seçilerek başlatılabilir.

## Yerel derleme ve dayanıklılık testi

```powershell
./client/tests/Build-And-Test.ps1 -StressSeconds 60
./client/tests/obj/stress/RuntimeStressTests.exe 1200
```

Uzun test yalnızca kendi loopback HTTP sunucusunu kullanır. Yavaş/başarısız cevaplar, kapanmış bağlantı, 128 işlik HTTP kuyruğunun doyması, işlem kuyruğu izolasyonu, dosya kilidi ile gerçek yazma hatası ve yeniden açılışta sonuçların korunması sınanır. Fiziksel diski doldurmaz.

Çıktı `client/tests/obj/stress/summary.json` dosyasındadır. Bellek, beş saniyelik ısınma sonrasındaki değerin 32 MiB üzerine çıkmamalı; ana thread cevap işleme süresi 500 ms altında kalmalı; kabul edilen HTTP işleri sonunda tamamlanmış olmalıdır. Bu ölçümler canlı sunucu performansı yerine çevrimdışı dayanıklılığı gösterir.

## Release kurulumu, geri alma ve hata raporu

Her release EXE, SHA-256 kontrol dosyası, `INSTALL-AND-ROLLBACK.md` ve yerel tanılama aracı içerir. UIF ve Oasis klasörlerini karıştırmayın. Güncellemeden önce kendi `client/bin` klasörünüzü, özellikle EXE ve XML'i yedekleyin; EXE hash'ini doğruladıktan sonra yalnızca EXE'yi değiştirin. Geri almak için yedek EXE'yi geri koyun. İstemciyi geri almak ayrı dağıtılmış backend'i geri almaz.

`Update-RakSAMPClient.ps1`, `gh auth login` sonrası seçilen projenin release'ini indirip checksum doğrular, açık onay ister ve önceki EXE'yi `client/bin/backups` altına alarak sadece EXE'yi değiştirir. Örnek: `./client/Update-RakSAMPClient.ps1 -Project UIF -InstallRoot 'C:\path\to\RakSAMP'`; Oasis için proje kökünü ve `-Project Oasis` değerini kullanın. `-Tag` sürüm sabitler, `-WhatIf` indirme ve doğrulama yapıp kurulumdan çıkar.

Depo kökünden PowerShell'de `./client/tests/Create-DiagnosticReport.ps1 -Project UIF` (Oasis için `-Project Oasis`) çalıştırın. Release asset'inden aracı ayrı indirdiyseniz `-ClientRoot 'C:\path\to\RakSAMP'` ile depo kökünü verin. Araç son günlük dosyasından yalnızca seçili teknik olayları alır; sohbet, tarama/textdraw kayıtları, XML, backend yapılandırması, bellek dökümleri, tam dosya yolları ve otomatik yükleme içermez. IP, e-posta, parola/token anahtarları ve uzun kimlik kalıpları maskelenir. ZIP'i açıp `diagnostics.txt` dosyasını incelemeden kimseyle paylaşmayın.
