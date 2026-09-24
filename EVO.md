# (manuel curl testleri)

---

## curl flag'leri — kısa anlamlar

| Flag | Anlamı |
|------|--------|
| `-s` | `silent` — ilerleme çubuğu/hata yazma, yalnızca gerçek çıktı |
| `-o /dev/null` | yanıtın gövdesini çöpe at (sadece başlık/status önemliyse) |
| `-w "%{http_code}\n"` | yalnızca HTTP durum kodunu bas (200/404/405/413...) |
| `-X POST` / `-X DELETE` | HTTP metodunu belirt |
| `-d "metin"` | POST gövdesi (gövde, `Host` kısmından sonra gönderilir) |
| `-i` | yanıt **başlıklarını** da ekrana bas (status line + header'lar) |
| `-v` | verbose: istek gidişi ve yanıt başlıkları da dahil her şeyi göster |

> `curl` varsayılan olarak GET atar; `-d` kullandığında otomatik POST'a döner.

---

## GET — statik içerik, autoindex, hata sayfası, redirect

**1) Ana sayfa (index)**
```bash
curl -s http://127.0.0.1:8080/
```
Beklenen: `HTTP 200` + `www/index.html` sayfası (HTML gövdesi terminale dolar).

**2) Statik dosya**
```bash
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8080/example.html
```
Beklenen: `200` — dizin altındaki statik dosya servis edilir.

**3) Autoindex (dizin listesi)**
```bash
curl -s http://127.0.0.1:8080/assets/
```
Beklenen: `200` + `assets` dizininin otomatik listesi (autoindex açık).

**4) Var olmayan dosya (404 hata sayfası)**
```bash
curl -s -i http://127.0.0.1:8080/yok-boyle
```
Beklenen: `404 Not Found` + özelleştirilmiş hata sayfası (`www/errors/404.html`).

**5) Redirect (301)**
```bash
curl -s -i http://127.0.0.1:8080/old-path
```
Beklenen: `301 Moved Permanently` + `Location: https://example.com/new-path` başlığı.

---

## POST (dosya yükleme) + DELETE

`/upload` yalnızca POST/DELETE kabul eder, okumaya kapalıdır.

**1) Dosya yükle (POST → 201)**
```bash
curl -s -i -X POST -d "webserv elle icerik 12345" http://127.0.0.1:8080/upload/t.txt
```
Beklenen: `201 Created` + `Location: /upload/t.txt` başlığı, dosya
`www/uploads/files/t.txt` olarak diske yazıldı.

**2) Upload dizinini okuma (405)**
```bash
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8080/upload/t.txt
```
Beklenen: `405 Method Not Allowed` — upload alanında GET'e izin yok.

**3) Dosyayı sil (DELETE → 204)**
```bash
curl -s -i -X DELETE http://127.0.0.1:8080/upload/t.txt
```
Beklenen: `204 No Content` (gövde yok), dosya diskten kaldırıldı.

**4) Tekrar sil (404)**
```bash
curl -s -o /dev/null -w "%{http_code}\n" -X DELETE http://127.0.0.1:8080/upload/t.txt
```
Beklenen: `404 Not Found` — dosya artık yok.

---

## CGI — çıktı terminale basılır

Sunucu, CGI programını çalıştırıp ürettiği **gövdeyi doğrudan yanıt olarak** döner.
Aşağıdaki komutlarda CGI çıktısını ekranda görürsün.

**1) `cgi_tester` (`.bla` uzantılı, Go binary — gövdeyi büyük harfe çevirir)**
```bash
curl -s -X POST -d "Merhaba 42" http://127.0.0.1:8080/test.bla
```
Beklenen terminal çıktısı:
```
MERHABA 42
```

**2) Python CGI — ortam değişkenlerini/gövdeyi raporlar**
```bash
curl -s -X POST -d "selam dunya" http://127.0.0.1:8080/cgi-bin/test.py
```
Beklenen:
```
METHOD=POST
CONTENT_LENGTH=11
CONTENT_TYPE=application/x-www-form-urlencoded
BODY_LEN=11
BODY=[selam dunya]
```
> Bu, sunucunun Linux `/usr/bin/python3` binary'sini çalıştırdığını ve CGI'ya
> `REQUEST_METHOD`, `CONTENT_LENGTH`, `CONTENT_TYPE` ortam değişkenlerini
> doğru aktardığını kanıtlar.

**3) Python CGI — stdin'den kaç byte okundu**
```bash
curl -s -X POST -d "deneme" http://127.0.0.1:8080/cgi-bin/echo_stdin.py
```
Beklenen:
```
READ_BYTES=6
CONTENT=deneme
```

**4) CGI — GET (gövdesiz okuma)**
```bash
curl -s http://127.0.0.1:8080/cgi-bin/echo_stdin.py
```
Beklenen:
```
READ_BYTES=0
CONTENT=
```

---

## Sınırlar ve metot denetimi

**Body boyut limiti (413)**
```bash
curl -s -o /dev/null -w "%{http_code}\n" -X POST \
  -d "$(printf 'a%.0s' $(seq 1 200))" http://127.0.0.1:8080/post_body
```
Beklenen: `413 Payload Too Large` — `/post_body` limiti 100 bayt.


> **Not:** 413 kararı parser'da değil, `routeRequest`'ta verilir — isteğin location'ı kendi `client_max_body_size`'ına sahipse onu (`effectiveBodyLimit`), yoksa server limitini (config'de direktif yoksa default 1 MB) uygular. Parser yalnızca tavanı (`maxBodyCeiling`) uygular; o da default 1 MB'dir.

**Tanımsız metot (405)**
```bash
curl -s -o /dev/null -w "%{http_code}\n" -X PUT -d x http://127.0.0.1:8080/
```
Beklenen: `405 Method Not Allowed` — yalnızca `GET` ve `POST` tanımlı.

---
