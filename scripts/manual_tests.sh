#!/bin/bash

# Webserv elle test senaryosu (sunum / evo gösterimi için)
#
# Kullanım:
#   ./scripts/manual_tests.sh                          # varsayılan: http://127.0.0.1:8080
#   ./scripts/manual_tests.sh http://127.0.0.1:8092    # başka port/sunucu
#
# Sunucu çalışmıyorsa script, demo.conf ile kendi ./webserver örneğini başlatır
# ve çıkışta kapatır. CGI testlerinde istemciye dönen gövde (çıktı) doğrudan
# terminale yazdırılır; CGI'nin ürettiği veriyi böylece ekranda görürsün.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASE="${1:-http://127.0.0.1:8080}"
CONF="$ROOT/demo.conf"

RED='\033[0;31m'
GREEN='\033[0;32m'
BOLD='\033[1m'
NC='\033[0m'

PASS=0
FAIL=0

started=0
SRV_PID=""

# Sunucu ayakta değilse demo.conf ile kendimiz başlat
if ! curl -s -o /dev/null --max-time 2 "$BASE/varlik-yokluk-kontrolu"; then
    echo "[!] $BASE yanıt vermiyor; $CONF ile ./webserver başlatılıyor (bitince kapatılır)..."
    (cd "$ROOT" && exec ./webserver demo.conf > /tmp/demo_ws.log 2>&1) &
    SRV_PID=$!
    started=1
    trap 'kill "$SRV_PID" 2>/dev/null; rm -f /tmp/demo_ws.log' EXIT
    for _ in $(seq 1 20); do
        curl -s -o /dev/null --max-time 1 "$BASE/varlik-yokluk-kontrolu" && break
        sleep 0.3
    done
fi

check() {
    local name="$1" expected="$2" got="$3"
    if [ "$got" = "$expected" ]; then
        PASS=$((PASS + 1))
        printf "%b[PASS]%b %-58s %s\n" "$GREEN" "$NC" "$name" "$got"
    else
        FAIL=$((FAIL + 1))
        printf "%b[FAIL]%b %-58s beklenen: %s, gerçek: %s\n" "$RED" "$NC" "$name" "$expected" "$got"
    fi
}

section() {
    printf "\n%b== %s ==%b\n" "$BOLD" "$1" "$NC"
}

code() {
    curl -s -o /dev/null -w "%{http_code}" "$@"
}

section "GET"
check "GET / (cat www/index.html)"       200 "$(code "$BASE/")"
check "GET /example.html (statik dosya)" 200 "$(code "$BASE/example.html")"
check "GET /assets/ (autoindex liste)"   200 "$(code "$BASE/assets/")"
check "GET /yok-boyle (404 error page)"  404 "$(code "$BASE/yok-boyle")"
check "GET /old-path (301 redirect)"     301 "$(code "$BASE/old-path")"

section "POST + GET + DELETE (dosya yükle/oku/sil)"
UP="upload/el_test.txt"
BODY="Webserv elle test icerigi - 12345"
check "POST /upload/el_test.txt (yeni arşiv 201)" \
    201 "$(curl -s -o /dev/null -w '%{http_code}' -X POST -d "$BODY" "$BASE/$UP")"
check "GET /files/el_test.txt (dosyayı geri oku)" \
    200 "$(code "$BASE/files/el_test.txt")"
check "DELETE /upload/el_test.txt (sil 204)" \
    204 "$(curl -s -o /dev/null -w '%{http_code}' -X DELETE "$BASE/$UP")"
check "DELETE yine (artık yok -> 404)" \
    404 "$(curl -s -o /dev/null -w '%{http_code}' -X DELETE "$BASE/$UP")"

section "CGI - çıktı terminale basılır (gövde = cgi çıktısı)"
printf "%b-- POST /test.bla --%b cgi_tester gövdeyi base64'e çevirir\n" "$BOLD" "$NC"
curl -s -X POST -d "Merhaba 42" "$BASE/test.bla"
printf '\n%b-- POST /cgi-bin/test.py --%b python3 ortam değişkenlerini gösterir\n' "$BOLD" "$NC"
curl -s -X POST -d "selam dunya" "$BASE/cgi-bin/test.py"
printf '\n'

section "Doğrulamalar (body limit / yasak method)"
LARGE="$(printf 'a%.0s' $(seq 1 200))"
check "POST /post_body 200B (100B limit -> 413)" \
    413 "$(curl -s -o /dev/null -w '%{http_code}' -X POST -d "$LARGE" "$BASE/post_body")"
check "PUT / (method yok -> 405)" \
    405 "$(curl -s -o /dev/null -w '%{http_code}' -X PUT -d x "$BASE/")"

printf '\n%bSonuç: %d geçti, %d başarısız%b\n' "$BOLD" "$PASS" "$FAIL" "$NC"
[ "$FAIL" -eq 0 ]