#!/bin/bash

echo "========================================"
echo "TEST 03 - NEISPRAVAN URI"
echo "========================================"

OUTPUT_FILE="tests/test_03_output.log"

timeout 15s ./smart_meter/client_async_test mostar 999 domacinstvo \
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

if grep -q "URI registracije: smartgrid://mostar/meter/999" "$OUTPUT_FILE"; then
    echo "[PASS] Poslan je namjerno neispravan URI"
else
    echo "[FAIL] Neispravan URI nije poslan"
    PASS=false
fi

if grep -qi "registracija.*odbij" "$OUTPUT_FILE" || \
   grep -qi "nije uspjesno registrovan" "$OUTPUT_FILE" || \
   grep -qi "registracija nije uspjela" "$OUTPUT_FILE"; then
    echo "[PASS] Regionalni server je odbio registraciju"
else
    echo "[FAIL] Nije potvrđeno odbijanje registracije"
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

