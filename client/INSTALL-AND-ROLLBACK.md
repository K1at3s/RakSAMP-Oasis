# RakSAMP client release / istemci güncelleme

## Türkçe

UIF ve Oasis ayrı istemcilerdir. UIF release'i yalnızca RakSAMP/UIF klasörüne, Oasis release'i yalnızca RakSAMP-Oasis klasörüne kurun. Proje adını ve release etiketini indirmeden önce kontrol edin.

1. İstemciyi kapatın.
2. Kendi kurulumunuzdaki `client/bin` klasörünü yedekleyin. En azından `RakSAMPClient.exe`, `RakSAMPClient.xml`, `bot_lang.txt`, `player_cache.txt`, `group_colors.txt` ve `logs` klasörünü saklayın. XML sunucu/hesap ayarlarını içerebilir; paylaşmayın.
3. Release sayfasından bu projeye ait `RakSAMPClient.exe`, `SHA256SUMS.txt` ve `Create-DiagnosticReport.ps1` dosyalarını indirin. `INSTALL-AND-ROLLBACK.md` dosyasını da okuyun.
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

### Roll back

Close the client and restore the saved `RakSAMPClient.exe`. If you changed the XML during the update, restore your own XML backup too. Rolling back the EXE does not roll back a separately updated backend; restore that project's previous backend commit/release separately. Keep UIF and Oasis files separate.

To create a diagnostic ZIP, run the tool from your source repository's `client/tests` folder. If you downloaded the standalone release asset, provide the repository root with `-ClientRoot 'C:\path\to\RakSAMP'` (use the Oasis root and `-Project Oasis` for Oasis). Review `diagnostics.txt` before sharing.
