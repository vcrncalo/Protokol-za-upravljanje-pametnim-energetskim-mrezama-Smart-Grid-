#!/bin/bash

echo "========================================"
echo "TEST 05 - DINAMICKA TARIFA"
echo "========================================"

OUTPUT_FILE="tests/test_05_output.log"

PASS=true

# Pokrecemo Smart Meter dovoljno dugo da posalje 5 mjerenja
# i primi TARIFF_UPDATE nakon procjene opterecenja
timeout 32s ./smart_meter/client_async_test sarajevo 001 domacinstvo \
    > "$OUTPUT_FILE" 2>&1

# 1. Registracija
if grep -q "Smart meter je uspjesno registrovan" "$OUTPUT_FILE"; then
    echo "[PASS] Smart Meter uspjesno registrovan"
else
    echo "[FAIL] Smart Meter nije registrovan"
    PASS=false
fi

# 2. Provjera da je poslano najmanje 5 mjerenja
REPORT_COUNT=$(grep -c "CONSUMPTION_REPORT poslan" "$OUTPUT_FILE")

if [ "$REPORT_COUNT" -ge 5 ]; then
    echo "[PASS] Poslano najmanje 5 mjerenja ($REPORT_COUNT)"
else
    echo "[FAIL] Poslano je samo $REPORT_COUNT mjerenja"
    PASS=false
fi

# 3. Provjera ACK odgovora
ACK_COUNT=$(grep -c "CONSUMPTION_ACK primljen" "$OUTPUT_FILE")

if [ "$ACK_COUNT" -ge 5 ]; then
    echo "[PASS] Primljeni CONSUMPTION_ACK odgovori ($ACK_COUNT)"
else
    echo "[FAIL] Nije primljeno dovoljno CONSUMPTION_ACK odgovora"
    PASS=false
fi

# 4. Provjera dinamičke tarife
if grep -q "TARIFF_UPDATE" "$OUTPUT_FILE"; then
    echo "[PASS] TARIFF_UPDATE primljen"
else
    echo "[FAIL] TARIFF_UPDATE nije primljen"
    PASS=false
fi

echo "========================================"

if [ "$PASS" = true ]; then
    echo "TEST RESULT: PASS"
    exit 0
else
    echo "TEST RESULT: FAIL"
    echo
    echo "Detaljan izlaz:"
    cat "$OUTPUT_FILE"
    exit 1
fi
