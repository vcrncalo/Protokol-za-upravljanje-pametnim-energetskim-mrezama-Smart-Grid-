#!/bin/bash

echo "========================================"
echo "TEST 04 - CONSUMPTION I REGION_SYNC"
echo "========================================"

OUTPUT_FILE="tests/test_04_output.log"
CENTRAL_DB="database/central.db"
DEVICE_URI="smartgrid://sarajevo/meter/001"

PASS=true

# Provjera da li centralna baza postoji
if [ ! -f "$CENTRAL_DB" ]; then
    echo "[FAIL] Centralna baza nije pronadjena: $CENTRAL_DB"
    exit 1
fi

# Provjera sqlite3 alata
if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "[FAIL] sqlite3 nije instaliran."
    exit 1
fi

# Broj pojavljivanja uredjaja u centralnoj bazi prije testa
BEFORE_COUNT=$(
    sqlite3 "$CENTRAL_DB" ".dump" 2>/dev/null |
    grep -c "$DEVICE_URI"
)

echo "Zapisa prije testa: $BEFORE_COUNT"

# Pokrecemo Smart Meter dovoljno dugo da posalje nekoliko mjerenja
timeout 18s ./smart_meter/client_async_test sarajevo 001 domacinstvo \
    > "$OUTPUT_FILE" 2>&1

# Dajemo centralnom serveru trenutak da zavrsi upis
sleep 1

# Broj pojavljivanja uredjaja nakon testa
AFTER_COUNT=$(
    sqlite3 "$CENTRAL_DB" ".dump" 2>/dev/null |
    grep -c "$DEVICE_URI"
)

echo "Zapisa poslije testa: $AFTER_COUNT"
echo

# 1. Registracija
if grep -q "Smart meter je uspjesno registrovan" "$OUTPUT_FILE"; then
    echo "[PASS] Smart Meter uspjesno registrovan"
else
    echo "[FAIL] Smart Meter nije registrovan"
    PASS=false
fi

# 2. CONSUMPTION_REPORT
if grep -q "CONSUMPTION_REPORT poslan" "$OUTPUT_FILE"; then
    echo "[PASS] CONSUMPTION_REPORT poslan"
else
    echo "[FAIL] CONSUMPTION_REPORT nije poslan"
    PASS=false
fi

# 3. CONSUMPTION_ACK
if grep -q "CONSUMPTION_ACK primljen" "$OUTPUT_FILE"; then
    echo "[PASS] CONSUMPTION_ACK primljen"
else
    echo "[FAIL] CONSUMPTION_ACK nije primljen"
    PASS=false
fi

# 4. Centralna baza mora imati nove podatke
if [ "$AFTER_COUNT" -gt "$BEFORE_COUNT" ]; then
    DIFFERENCE=$((AFTER_COUNT - BEFORE_COUNT))

    echo "[PASS] REGION_SYNC uspjesan"
    echo "[PASS] Centralna baza dobila $DIFFERENCE novih zapisa"
else
    echo "[FAIL] Nema novih podataka u centralnoj bazi"
    PASS=false
fi

echo "========================================"

if [ "$PASS" = true ]; then
    echo "TEST RESULT: PASS"
    exit 0
else
    echo "TEST RESULT: FAIL"
    echo
    echo "Detaljan izlaz Smart Metera:"
    cat "$OUTPUT_FILE"
    exit 1
fi
