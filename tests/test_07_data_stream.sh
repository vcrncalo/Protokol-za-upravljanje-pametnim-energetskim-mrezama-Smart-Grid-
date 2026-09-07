#!/bin/bash

echo "======================================"
echo "TEST 07 - DATA STREAM"
echo "======================================"

OUTPUT_FILE="tests/test_07_output.log"

rm -f "$OUTPUT_FILE"

echo "Pokrecem Smart Meter Sarajevo..."
echo "Test traje oko 20 sekundi."
echo

timeout 20s ./smart_meter/client_async_test \
sarajevo 001 domacinstvo \
> "$OUTPUT_FILE" 2>&1

STREAM_COUNT=$(grep -c "DATA_STREAM_SAMPLE #" "$OUTPUT_FILE")
ACK_COUNT=$(grep -c "CONSUMPTION_ACK primljen" "$OUTPUT_FILE")

echo "Broj DATA_STREAM_SAMPLE poruka: $STREAM_COUNT"
echo "Broj CONSUMPTION_ACK poruka: $ACK_COUNT"
echo

if [ "$STREAM_COUNT" -ge 3 ] && [ "$ACK_COUNT" -ge 3 ]; then
    echo "PASS: DATA-STREAM razmjena uspjesno demonstrirana."
    echo "Smart Meter kontinuirano salje DATA_STREAM_SAMPLE poruke."
    echo "Postojeca CONSUMPTION_REPORT/ACK komunikacija i dalje radi."
    exit 0
else
    echo "FAIL: DATA-STREAM razmjena nije potvrdena."
    echo
    echo "Provjeri izlaz:"
    echo "$OUTPUT_FILE"
    exit 1
fi
