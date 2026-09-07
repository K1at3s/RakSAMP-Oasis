# TODO

## ?zsnip — zincir saldırı (uL.Hold gibi)

- [ ] `?zsnip <tag>` → o grubun zone'una ışınlan, savaş **bitince** `/gz` ile otomatik saldır
- [ ] Saldırı bittikten sonra kullanıcıya sor (uL.Hold gibi): "diğer zone'a da saldırayım mı?"
- [ ] `?yes` cevabı gelirse → o grubun **sonraki zone'una** git, savaş bitince otomatik saldır (zincir)
- [ ] Sıradaki zone'lar DB'deki `gangs.zones` kaydından alınır (paket kaybı restore fallback'i hazır)
- [ ] GZ giriş-çıkış döngüsü:
  - Zone war başladığında ilk **3 sn** GZ'ye gir (takım/renk öğren) → geri zone taramasına dön
  - **09:03 kala** GZ var mı diye kontrol → o anda tekrar GZ'ye gir → çık → taramaya devam et