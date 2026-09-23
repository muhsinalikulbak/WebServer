# Webserv elle curl testleri (GET / POST / DELETE / CGI)
# Sunucu:  ./webserver demo.conf      (varsayılan port 8080)
# Farklı port için:  BASE=http://127.0.0.1:8092  yap ve komutları tek tek çalıştır

BASE=http://127.0.0.1:8080

# ----------------------------------- GET ------------------------------------
curl -s "$BASE/"                                      # 200 -> www/index.html
curl -s "$BASE/example.html"                          # 200 -> statik dosya
curl -s "$BASE/assets/"                               # 200 -> autoindex listesi
curl -s "$BASE/yok-boyle"                             # 404 -> hata sayfası
curl -s -o /dev/null -w "%{http_code}\n" "$BASE/old-path"   # 301 -> redirect

# --------------------------- POST (upload) + DELETE -------------------------
curl -s -i -X POST -d "merhaba icerik" "$BASE/upload/el_test.txt"  # 201 + Location
curl -s "$BASE/files/el_test.txt"                     # 200 -> yüklenen dosyayı okur
curl -s -i -X DELETE "$BASE/upload/el_test.txt"       # 204 -> silindi
curl -s -i -X DELETE "$BASE/upload/el_test.txt"       # 404 -> artık yok

# --------------------------- CGI (çıktı terminale) ---------------------------
curl -s -X POST -d "Merhaba 42" "$BASE/test.bla"      # cgi_tester -> base64/echo
curl -s -X POST -d "selam dunya" "$BASE/cgi-bin/test.py"  # python3 -> METHOD/BODY

# ------------------------------ Sınırlar / method -----------------------------
curl -s -o /dev/null -w "%{http_code}\n" -X POST \
  -d "$(printf 'a%.0s' $(seq 1 200))" "$BASE/post_body"   # 413 -> limit 100B
curl -s -o /dev/null -w "%{http_code}\n" -X PUT -d x "$BASE/"  # 405 -> method yok