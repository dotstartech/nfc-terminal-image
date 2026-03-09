#!/bin/bash
# Generate development certificates for RAUC bundle signing
#
# These certificates are for DEVELOPMENT ONLY.
# For production, use a proper PKI infrastructure or HSM.
#
# Generated files:
#   certs/ca.key.pem        - CA private key (keep secret!)
#   certs/ca.cert.pem       - CA certificate (installed on target as keyring)
#   certs/signing.key.pem   - Bundle signing private key
#   certs/signing.cert.pem  - Bundle signing certificate
#
# Usage:
#   ./certgen.sh            # Generate new certificates
#   ./certgen.sh --force    # Regenerate (overwrite existing)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CERT_DIR="${SCRIPT_DIR}/certs"

if [ -f "${CERT_DIR}/ca.cert.pem" ] && [ "$1" != "--force" ]; then
    echo "Certificates already exist in ${CERT_DIR}/"
    echo "Use --force to regenerate."
    exit 0
fi

mkdir -p "$CERT_DIR"

echo "=== Generating RAUC Development Certificates ==="
echo ""

# Generate CA key and self-signed certificate
echo "1. Generating CA key and certificate..."
openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 \
    -keyout "${CERT_DIR}/ca.key.pem" \
    -out "${CERT_DIR}/ca.cert.pem" \
    -subj "/O=NFC Terminal/CN=NFC Terminal Development CA" \
    -nodes 2>/dev/null

# Generate signing key and CSR
echo "2. Generating signing key..."
openssl req -new -newkey rsa:4096 \
    -keyout "${CERT_DIR}/signing.key.pem" \
    -out "${CERT_DIR}/signing.csr.pem" \
    -subj "/O=NFC Terminal/CN=NFC Terminal Development Signing" \
    -nodes 2>/dev/null

# Sign the signing certificate with the CA
echo "3. Signing certificate with CA..."
openssl x509 -req -sha256 -days 3650 \
    -in "${CERT_DIR}/signing.csr.pem" \
    -CA "${CERT_DIR}/ca.cert.pem" \
    -CAkey "${CERT_DIR}/ca.key.pem" \
    -CAcreateserial \
    -out "${CERT_DIR}/signing.cert.pem" 2>/dev/null

# Clean up CSR and serial
rm -f "${CERT_DIR}/signing.csr.pem" "${CERT_DIR}/ca.srl"

echo ""
echo "=== Development certificates generated ==="
echo ""
echo "  CA certificate (keyring):  ${CERT_DIR}/ca.cert.pem"
echo "  CA private key:            ${CERT_DIR}/ca.key.pem"
echo "  Signing certificate:       ${CERT_DIR}/signing.cert.pem"
echo "  Signing private key:       ${CERT_DIR}/signing.key.pem"
echo ""
echo "The CA certificate will be installed on the target as the RAUC keyring."
echo "Use the signing key/cert to sign update bundles."
echo ""
echo "WARNING: These are development keys. Do NOT use in production!"
