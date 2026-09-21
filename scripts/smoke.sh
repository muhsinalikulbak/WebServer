#!/bin/bash

# Smoke test script for webserv
# Usage: ./scripts/smoke.sh [generate|compare]
#   generate: Runs tests and saves golden responses to tests/golden/
#   compare:  Runs tests and compares with golden responses

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GOLDEN_DIR="$PROJECT_ROOT/tests/golden"
SERVER_BIN="$PROJECT_ROOT/webserver"
CONFIG_FILE="$PROJECT_ROOT/webserver.conf"
HOST="127.0.0.1"
PORT="8080"
PID_FILE="/tmp/webserv_smoke_test.pid"

# Renkli çıktı
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Mode kontrolü
MODE="${1:-generate}"
if [ "$MODE" != "generate" ] && [ "$MODE" != "compare" ]; then
    echo "Usage: $0 [generate|compare]"
    exit 1
fi

# Golden dizin oluştur
mkdir -p "$GOLDEN_DIR"

# Upload dizinini temizle
cleanup_upload_dir() {
    local upload_dir="$PROJECT_ROOT/www/uploads/files"
    if [ -d "$upload_dir" ]; then
        rm -f "$upload_dir"/*
    fi
}

# Geçici dosyaları temizle
cleanup_temp_files() {
    if [ -n "$TEMP_LARGE_FILE" ] && [ -f "$TEMP_LARGE_FILE" ]; then
        rm -f "$TEMP_LARGE_FILE"
    fi
}

# Tüm temizlik işlemleri
cleanup_all() {
    stop_server
    cleanup_temp_files
}

# Sunucuyu başlat
start_server() {
    echo -e "${YELLOW}Starting webserver...${NC}"
    cd "$PROJECT_ROOT"
    cleanup_upload_dir
    "$SERVER_BIN" "$CONFIG_FILE" &
    SERVER_PID=$!
    echo $SERVER_PID > "$PID_FILE"
    
    # Sunucunun başlamasını bekle
    sleep 2
    
    # Sunucunun çalışıp çalışmadığını kontrol et
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}Failed to start server${NC}"
        exit 1
    fi
    echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"
}

# Sunucuyu durdur
stop_server() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 $PID 2>/dev/null; then
            echo -e "${YELLOW}Stopping server (PID: $PID)...${NC}"
            kill $PID
            wait $PID 2>/dev/null || true
        fi
        rm -f "$PID_FILE"
    fi
}

# Cleanup trap
trap cleanup_all EXIT INT TERM

# Response'u normalize et (Date header gibi değişkenleri kaldır)
normalize_response() {
    local response="$1"
    
    echo "$response" | sed -e 's/^Date:.*$/Date: <REDACTED>/i' -e 's/^Server:.*$/Server: webserv/i'
}

# İstek gönder ve response'u kaydet
run_test() {
    local test_name="$1"
    local method="$2"
    local path="$3"
    local data="$4"
    local headers="$5"
    
    local output_file="$GOLDEN_DIR/${test_name}.txt"
    
    echo -e "${YELLOW}Running test: $test_name${NC}"
    
    local response
    if [ -z "$data" ]; then
        if [ -z "$headers" ]; then
            response=$(curl -s -i -X "$method" "http://${HOST}:${PORT}${path}" 2>/dev/null || echo "FAILED")
        else
            response=$(curl -s -i -X "$method" -H "$headers" "http://${HOST}:${PORT}${path}" 2>/dev/null || echo "FAILED")
        fi
    else
        if [ -z "$headers" ]; then
            response=$(curl -s -i -X "$method" -d "$data" "http://${HOST}:${PORT}${path}" 2>/dev/null || echo "FAILED")
        else
            response=$(curl -s -i -X "$method" -H "$headers" -d "$data" "http://${HOST}:${PORT}${path}" 2>/dev/null || echo "FAILED")
        fi
    fi
    
    local normalized=$(normalize_response "$response")
    
    if [ "$MODE" = "generate" ]; then
        echo "$normalized" > "$output_file"
        echo -e "${GREEN}Saved: $output_file${NC}"
    else
        if [ -f "$output_file" ]; then
            local golden=$(cat "$output_file")
            if [ "$normalized" = "$golden" ]; then
                echo -e "${GREEN}PASS: $test_name${NC}"
            else
                echo -e "${RED}FAIL: $test_name${NC}"
                echo "Expected:"
                echo "$golden"
                echo "Got:"
                echo "$normalized"
                echo ""
                return 1
            fi
        else
            echo -e "${RED}FAIL: $test_name (golden file not found)${NC}"
            return 1
        fi
    fi
}

# Chunked encoding testi için nc kullan
run_chunked_test() {
    local test_name="$1"
    local path="$2"
    local chunked_data="$3"
    
    local output_file="$GOLDEN_DIR/${test_name}.txt"
    
    echo -e "${YELLOW}Running test: $test_name${NC}"
    
    local response
    response=$(echo -ne "$chunked_data" | nc -q 1 "$HOST" "$PORT" 2>/dev/null || echo "FAILED")
    
    local normalized=$(normalize_response "$response")
    
    if [ "$MODE" = "generate" ]; then
        echo "$normalized" > "$output_file"
        echo -e "${GREEN}Saved: $output_file${NC}"
    else
        if [ -f "$output_file" ]; then
            local golden=$(cat "$output_file")
            if [ "$normalized" = "$golden" ]; then
                echo -e "${GREEN}PASS: $test_name${NC}"
            else
                echo -e "${RED}FAIL: $test_name${NC}"
                echo "Expected:"
                echo "$golden"
                echo "Got:"
                echo "$normalized"
                echo ""
                return 1
            fi
        else
            echo -e "${RED}FAIL: $test_name (golden file not found)${NC}"
            return 1
        fi
    fi
}

# Ana test akışı
start_server

echo -e "${YELLOW}Running smoke tests in $MODE mode...${NC}"
echo ""

# Test 1: GET /
run_test "get_root" "GET" "/"

# Test 2: GET /directory (autoindex testi)
run_test "get_directory" "GET" "/assets/"

# Test 3: Olmayan dosyaya GET
run_test "get_not_found" "GET" "/nonexistent.html"

# Test 4: POST /upload 0 byte
run_test "post_0byte" "POST" "/upload/test.txt" ""

# Test 5: POST /upload 100 byte
run_test "post_100byte" "POST" "/upload/test100.txt" "$(printf 'A%.0s' {1..100})"

# Test 6: POST /upload 200 byte
run_test "post_200byte" "POST" "/upload/test200.txt" "$(printf 'A%.0s' {1..200})"

# Test 7: DELETE /upload/test.txt (önce oluştur)
run_test "post_for_delete" "POST" "/upload/test_delete.txt" "test content"
sleep 1
run_test "delete_file" "DELETE" "/upload/test_delete.txt"

# Test 8: CGI GET .py
run_test "cgi_get_py" "GET" "/cgi-bin/echo_stdin.py"

# Test 9: CGI POST .py
run_test "cgi_post_py" "POST" "/cgi-bin/echo_stdin.py" "test data for cgi"

# Test 10: Chunked POST
# Chunked encoding: 5\r\nhello\r\n0\r\n\r\n
run_chunked_test "chunked_post" "/upload/chunked.txt" "POST /upload/chunked.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n"

# Test 11: Redirect (301)
run_test "redirect_301" "GET" "/old-path"

# Test 12: Autoindex açık olan dizin
run_test "autoindex" "GET" "/"

# Test 13: 405 Method Not Allowed (DELETE / ile)
run_test "error_405" "DELETE" "/"

# Test 14: 404 Not Found (zaten test edildi ama tekrar)
run_test "error_404" "GET" "/notfound"

# Test 15: 413 Payload Too Large (büyük body)
# 10M'den büyük bir body gönder
TEMP_LARGE_FILE=$(mktemp)
head -c 11000000 /dev/zero | tr '\0' 'A' > "$TEMP_LARGE_FILE"
response=$(curl -s -i -X POST --data-binary @"$TEMP_LARGE_FILE" "http://${HOST}:${PORT}/upload/large.txt" 2>/dev/null || echo "FAILED")
normalized=$(normalize_response "$response")

if [ "$MODE" = "generate" ]; then
    echo "$normalized" > "$GOLDEN_DIR/error_413.txt"
    echo -e "${GREEN}Saved: $GOLDEN_DIR/error_413.txt${NC}"
else
    if [ -f "$GOLDEN_DIR/error_413.txt" ]; then
        golden=$(cat "$GOLDEN_DIR/error_413.txt")
        if [ "$normalized" = "$golden" ]; then
            echo -e "${GREEN}PASS: error_413${NC}"
        else
            echo -e "${RED}FAIL: error_413${NC}"
            echo "Expected:"
            echo "$golden"
            echo "Got:"
            echo "$normalized"
            echo ""
            exit 1
        fi
    else
        echo -e "${RED}FAIL: error_413 (golden file not found)${NC}"
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}All tests completed in $MODE mode${NC}"
