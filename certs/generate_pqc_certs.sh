#!/bin/bash

set -e

OPENSSL="/opt/openssl-3.5/bin/openssl"
export LD_LIBRARY_PATH=/opt/openssl-3.5/lib64

CERT_DIR="certs/pqc"

mkdir -p "$CERT_DIR"

echo "========================================"
echo " Generisanje Smart Grid PQC certifikata"
echo "========================================"

# ---------------------------------------------------------
# 1. CA - ML-DSA-44
# ---------------------------------------------------------

echo
echo "[1/6] Generisanje PQC CA..."

$OPENSSL genpkey \
    -algorithm ML-DSA-44 \
    -out "$CERT_DIR/pqc_ca.key"

$OPENSSL req \
    -new \
    -x509 \
    -key "$CERT_DIR/pqc_ca.key" \
    -out "$CERT_DIR/pqc_ca.crt" \
    -days 3650 \
    -subj "/C=BA/O=SmartGrid/OU=ETF/CN=SmartGrid-PQC-CA"


# ---------------------------------------------------------
# Pomocna funkcija za server certifikate
# ---------------------------------------------------------

generate_server_cert()
{
    NAME=$1
    CN=$2

    echo
    echo "Generisem certifikat: $NAME"

    $OPENSSL genpkey \
        -algorithm ML-DSA-44 \
        -out "$CERT_DIR/${NAME}.key"

    $OPENSSL req \
        -new \
        -key "$CERT_DIR/${NAME}.key" \
        -out "$CERT_DIR/${NAME}.csr" \
        -subj "/C=BA/O=SmartGrid/OU=ETF/CN=${CN}"

    cat > "$CERT_DIR/${NAME}_ext.cnf" <<EOF
basicConstraints=CA:FALSE
keyUsage=digitalSignature
extendedKeyUsage=serverAuth
EOF

    $OPENSSL x509 \
        -req \
        -in "$CERT_DIR/${NAME}.csr" \
        -CA "$CERT_DIR/pqc_ca.crt" \
        -CAkey "$CERT_DIR/pqc_ca.key" \
        -CAcreateserial \
        -out "$CERT_DIR/${NAME}.crt" \
        -days 365 \
        -extfile "$CERT_DIR/${NAME}_ext.cnf"
}


# ---------------------------------------------------------
# Pomocna funkcija za Smart Meter certifikate
# ---------------------------------------------------------

generate_meter_cert()
{
    NAME=$1
    URI=$2

    echo
    echo "Generisem Smart Meter certifikat: $NAME"

    $OPENSSL genpkey \
        -algorithm ML-DSA-44 \
        -out "$CERT_DIR/${NAME}.key"

    $OPENSSL req \
        -new \
        -key "$CERT_DIR/${NAME}.key" \
        -out "$CERT_DIR/${NAME}.csr" \
        -subj "/C=BA/O=SmartGrid/OU=ETF/CN=${NAME}"

    cat > "$CERT_DIR/${NAME}_ext.cnf" <<EOF
basicConstraints=CA:FALSE
keyUsage=digitalSignature
extendedKeyUsage=clientAuth
subjectAltName=URI:${URI}
EOF

    $OPENSSL x509 \
        -req \
        -in "$CERT_DIR/${NAME}.csr" \
        -CA "$CERT_DIR/pqc_ca.crt" \
        -CAkey "$CERT_DIR/pqc_ca.key" \
        -CAcreateserial \
        -out "$CERT_DIR/${NAME}.crt" \
        -days 365 \
        -extfile "$CERT_DIR/${NAME}_ext.cnf"
}


# ---------------------------------------------------------
# 2. Central Server
# ---------------------------------------------------------

echo
echo "[2/6] Central Server"
generate_server_cert \
    "central_server" \
    "central.smartgrid.local"


# ---------------------------------------------------------
# 3. Regional Server 1
# ---------------------------------------------------------

echo
echo "[3/6] Regional Server 1"
generate_server_cert \
    "region1_server" \
    "region1.smartgrid.local"


# ---------------------------------------------------------
# 4. Regional Server 2
# ---------------------------------------------------------

echo
echo "[4/6] Regional Server 2"
generate_server_cert \
    "region2_server" \
    "region2.smartgrid.local"


# ---------------------------------------------------------
# 5. Smart Meter 001
# ---------------------------------------------------------

echo
echo "[5/6] Smart Meter 001"
generate_meter_cert \
    "meter001" \
    "smartgrid://sarajevo/meter/001"


# ---------------------------------------------------------
# 6. Smart Meter 002
# ---------------------------------------------------------

echo
echo "[6/6] Smart Meter 002"
generate_meter_cert \
    "meter002" \
    "smartgrid://mostar/meter/002"


echo
echo "========================================"
echo " Certifikati uspjesno generisani."
echo " Lokacija: $CERT_DIR"
echo "========================================"
