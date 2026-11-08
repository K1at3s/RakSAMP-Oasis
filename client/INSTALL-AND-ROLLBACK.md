# RakSAMP client release / istemci güncelleme

## Türkçe

UIF ve Oasis ayrı istemcilerdir. UIF release'i yalnızca UIF kurulumuna, Oasis release'i yalnızca Oasis kurulumuna kurun. İki depo da özel olduğundan otomatik güncelleme için GitHub CLI (`gh`) kurulmuş ve `gh auth login` ile hesabınıza giriş yapılmış olmalıdır.

### Otomatik güncelleme

Release içindeki `Update-RakSAMPClient.ps1` dosyasını indirin. PowerShell'de uygun proje kökünü hedef gösterin:

```powershell
./Update-RakSAMPClient.ps1 -Project UIF -InstallRoot 'C:\path\to\RakSAMP'
./Update-RakSAMPClient.ps1 -Project Oasis -InstallRoot 'C:\path\to\RakSAMP-YeniSunucu'
```

Script seçilen projenin son release'inden EXE ve SHA-256 dosyasını indirir. Sürümü sabitlemek için `-Tag beta-v7` veya `-Tag v1.9.2` ekleyin. `-WhatIf` indirip checksum'ı kontrol eder, fakat dosyayı kurmaz. Kurulum öncesi açık onay ister; eski EXE'yi `client/bin/backups` altına yedekler ve yalnızca EXE'yi değiştirir. İstemci açıksa kapatmanızı ister. Bir Git checkout'u hedeflendiğinde uzak deponun seçilen UIF/Oasis projesiyle eşleştiğini de doğrular.

1. İstemciyi kapatın.
2. Elle kurulum yapacaksanız kendi `client/bin` klasörünüzü yedekleyin. En azından `RakSAMPClient.exe`, `RakSAMPClient.xml`, `bot_lang.txt`, `player_cache.txt`, `group_colors.txt` ve `logs` klasörünü saklayın. XML sunucu/hesap ayarlarını içerebilir; paylaşmayın.
3. Otomatik kurulum için release'deki `Update-RakSAMPClient.ps1` dosyasını kullanın. Elle kurulumda bu projeye ait `RakSAMPClient.exe` ve `SHA256SUMS.txt` dosyalarını indirin. Tanılama aracı gerekiyorsa `Create-DiagnosticReport.ps1` dosyasını da indirin.
4. İndirme klasöründe PowerShell açıp SHA-256 değerini doğrulayın:

   ```powershell
   $expected = ((Get-Content .\SHA256SUMS.txt -Raw) -split '\s+')[0].ToLowerInvariant()
   $actual = (Get-FileHash .\RakSAMPClient.exe -Algorithm SHA256).Hash.ToLowerInvariant()
   if ($actual -ne $expected) { throw 'SHA-256 uyuşmuyor; dosyayı kurmayın.' }
   ```

5. Doğrulanan EXE'yi kendi projenizin `client/bin/RakSAMPClient.exe` dosyasının üzerine kopyalayın. XML'i veya backend yapılandırmasını release içeriğiyle değiştirmeyin. İstemciyi başlatıp normal çalıştığını kontrol edin.

### Önceki sürüme dönme

İstemciyi kapatın; 2. adımda sakladığınız `RakSAMPClient.exe` yedeğini geri kopyalayın. Güncellemeden sonra XML'i ayrıca değiştirdiyseniz, kendi XML yedeğinizi de geri yükleyin. Backend'i ayrıca güncellediyseniz istemci EXE'sini geri almak backend'i geri almaz; bu projedeki önceki backend commit/release'ini de ayrıca dağıtın. Eski ve yeni UIF/Oasis dosyalarını birbirine karıştırmayın.

Tanılama ZIP'i hazırlamak için araç dosyasını kaynak deponuzun `client/tests` klasöründen çalıştırın. Release asset'inden indirdiyseniz depo kökünü belirtin: `./Create-DiagnosticReport.ps1 -Project UIF -ClientRoot 'C:\path\to\RakSAMP'` (Oasis için proje kökü ve `-Project Oasis` kullanın). ZIP içindeki `diagnostics.txt` dosyasını paylaşmadan önce inceleyin.

## English

UIF and Oasis are separate clients. Install a RakSAMP/UIF release only into its UIF folder, and a RakSAMP-Oasis release only into its Oasis folder. Check the project and release tag before downloading.

1. Close the client.
2. Back up your own `client/bin` folder. Keep at least `RakSAMPClient.exe`, `RakSAMPClient.xml`, `bot_lang.txt`, `player_cache.txt`, `group_colors.txt`, and `logs`. The XML may contain server/account settings; do not share it.
3. Download this repository's `RakSAMPClient.exe`, `SHA256SUMS.txt`, and `Create-DiagnosticReport.ps1` from its release. Read this guide too.
4. In PowerShell opened in the download folder, verify the checksum with the command above.
5. Copy the verified EXE over your own project's `client/bin/RakSAMPClient.exe`. Do not replace your XML or backend configuration with release files. Start the client and confirm it works.

### Automatic update

Download `Update-RakSAMPClient.ps1` from the release. Both repositories are private, so install GitHub CLI (`gh`) and sign in with `gh auth login`. In PowerShell, point the script at the right project root:

```powershell
./Update-RakSAMPClient.ps1 -Project UIF -InstallRoot 'C:\path\to\RakSAMP'
./Update-RakSAMPClient.ps1 -Project Oasis -InstallRoot 'C:\path\to\RakSAMP-YeniSunucu'
```

It downloads the EXE and SHA-256 manifest from the selected project's latest release. Pin a version with `-Tag beta-v7` or `-Tag v1.9.2`. `-WhatIf` downloads and checks the hash without installing. Installation requires confirmation, saves the old EXE under `client/bin/backups`, and replaces only the EXE. It stops if the client is running. When the target is a Git checkout, it verifies the origin repository matches the selected project.

### Roll back

Close the client and restore the saved `RakSAMPClient.exe`. If you changed the XML during the update, restore your own XML backup too. Rolling back the EXE does not roll back a separately updated backend; restore that project's previous backend commit/release separately. Keep UIF and Oasis files separate.

After using the updater, the previous EXE is in `client/bin/backups`; close the client and copy that file over `client/bin/RakSAMPClient.exe` to roll back.

To create a diagnostic ZIP, run the tool from your source repository's `client/tests` folder. If you downloaded the standalone release asset, provide the repository root with `-ClientRoot 'C:\path\to\RakSAMP'` (use the Oasis root and `-Project Oasis` for Oasis). Review `diagnostics.txt` before sharing.
