#!/bin/bash

echo "========================================"
echo "TEST 06 - REDUCE CONSUMPTION"
echo "========================================"

OUTPUT_FILE="tests/test_06_output.log"

PASS=true

# Koristimo industrijski Smart Meter jer generise vece
# opterecenje i time aktivira REDUCE komandu.
#
# Ostavljamo dovoljno vremena za vise ciklusa u slucaju
# da prvi ciklus ne predje prag za REDUCE.
timeout 70s ./smart_meter/client_async_test mostar 002 industrija \
    > "$OUTPUT_FILE" 2>&1

# 1. Registracija
if grep -q "Smart meter je uspjesno registrovan" "$OUTPUT_FILE"; then
    echo "[PASS] Smart Meter uspjesno registrovan"
else
    echo "[FAIL] Smart Meter nije registrovan"
    PASS=false
fi

# 2. Mora biti poslano najmanje 5 mjerenja
REPORT_COUNT=$(grep -c "CONSUMPTION_REPORT poslan" "$OUTPUT_FILE")

if [ "$REPORT_COUNT" -ge 5 ]; then
    echo "[PASS] Poslano najmanje 5 mjerenja ($REPORT_COUNT)"
else
    echo "[FAIL] Poslano je samo $REPORT_COUNT mjerenja"
    PASS=false
fi

# 3. REDUCE komanda
if grep -q "REDUCE_CONSUMPTION_CMD" "$OUTPUT_FILE"; then
    echo "[PASS] REDUCE_CONSUMPTION_CMD primljen"
else
    echo "[FAIL] REDUCE_CONSUMPTION_CMD nije primljen"
    PASS=false
fi

# 4. COMMAND_ACK
if grep -q "COMMAND_ACK" "$OUTPUT_FILE"; then
    echo "[PASS] COMMAND_ACK poslan"
else
    echo "[FAIL] COMMAND_ACK nije potvrđen"
    PASS=false
fi

# 5. Provjera da je Smart Meter poslao mjerenje nakon REDUCE komande
if grep -qi "REDUCE.*CONSUMPTION_REPORT\|CONSUMPTION_REPORT.*REDUCE\|smanjen" \
    "$OUTPUT_FILE"; then
    echo "[PASS] Poslano mjerenje nakon REDUCE komande"
else
    echo "[FAIL] Nije pronađena potvrda mjerenja nakon REDUCE komande"
    PASS=false
fi

# 6. Mora biti primljen ACK nakon izvrsavanja komande
if grep -q "CONSUMPTION_ACK" "$OUTPUT_FILE"; then
    echo "[PASS] CONSUMPTION_ACK primljen"
else
    echo "[FAIL] CONSUMPTION_ACK nije primljen"
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
