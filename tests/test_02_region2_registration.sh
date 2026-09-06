#!/bin/bash

echo "========================================"
echo "TEST 02 - REGION 2 REGISTRACIJA"
echo "========================================"

OUTPUT_FILE="tests/test_02_output.log"

timeout 20s ./smart_meter/client_async_test mostar 002 domacinstvo \
    > "$OUTPUT_FILE" 2>&1

PASS=true

if grep -q "TLS handshake uspjesan" "$OUTPUT_FILE"; then
    echo "[PASS] TLS handshake uspjesan"
else
    echo "[FAIL] TLS handshake nije uspjesan"
    PASS=false
fi

if grep -q "X25519MLKEM768" "$OUTPUT_FILE"; then
    echo "[PASS] PQC grupa X25519MLKEM768 potvrđena"
else
    echo "[FAIL] PQC grupa nije potvrđena"
    PASS=false
fi

if grep -q "URI registracije: smartgrid://mostar/meter/002" "$OUTPUT_FILE"; then
    echo "[PASS] URI registracije ispravan"
else
    echo "[FAIL] URI registracije nije ispravan"
    PASS=false
fi

if grep -q "Smart meter je uspjesno registrovan" "$OUTPUT_FILE"; then
    echo "[PASS] Smart Meter uspjesno registrovan"
else
    echo "[FAIL] Registracija Smart Metera nije uspjela"
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
