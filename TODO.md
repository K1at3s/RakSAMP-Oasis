# TODO

## ?zsnip — zincir saldırı (uL.Hold gibi)

- [x] `?zsnip <tag>` → o grubun zone'una ışınlan, savaş **bitince** `/gz` ile otomatik saldır
- [x] Saldırı bittikten sonra kullanıcıya sor: "diğer zone'a da saldırayım mı?" (`?yes` bekler)
- [x] `?yes` → o grubun **sonraki zone'una** git, savaş bitince otomatik saldır (zincir); `?no` iptal
- [x] Sıradaki zone'lar DB'deki `gangs.zones` kaydından alınır (`CollectGroupZones` + `/api/groups/zone` fallback)
- [x] GZ giriş-çıkış döngüsü:
  - Zone war başladığında ilk **3 sn** GZ'ye gir (takım/renk öğren) → geri zone taramasına dön *(mevcut ZwJoinCaptureReturn)*
  - **09:03 kala** GZ var mı diye kontrol → o anda tekrar GZ'ye gir → çık → taramaya devam et *(mevcut g_zwCheckJoinSent)*

## Bekleyen
- [ ] Zincir testi (canlı: ?zsnip <tag> → saldırı → ?yes → sonraki zone)
- [ ] Zincir sırasında ?zsnip ile yeni komut gelirse eski zinciri iptal et