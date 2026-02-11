#!/bin/sh
#
# NFC Diagnostic Script
# Run this on the target to debug NFC kernel driver and libnfc-nci
#

echo "=== NFC Diagnostic ==="
echo ""

echo "1. Kernel Module Status:"
if lsmod | grep -q pn5xx; then
    echo "   pn5xx_i2c module: LOADED"
    lsmod | grep pn5xx
else
    echo "   pn5xx_i2c module: NOT LOADED"
    echo "   Try: modprobe pn5xx_i2c"
fi
echo ""

echo "2. Device Node:"
if [ -c /dev/pn544 ]; then
    echo "   /dev/pn544: EXISTS"
    ls -la /dev/pn544
else
    echo "   /dev/pn544: NOT FOUND"
fi
echo ""

echo "3. I2C NFC Device (should show 28 or UU):"
if command -v i2cdetect >/dev/null 2>&1; then
    i2cdetect -y 1 2>/dev/null | grep -E "^20:|^10:"
else
    echo "   i2cdetect not found"
fi
echo ""

echo "4. GPIO Pin Status (controlled by kernel driver):"
if command -v gpioinfo >/dev/null 2>&1; then
    gpioinfo 2>/dev/null | grep -E "line\s+(5|6):" || echo "   GPIO 5/6 should be claimed by pn5xx driver"
else
    echo "   gpioinfo not found"
fi
echo ""

echo "5. Kernel Messages (NFC driver):"
dmesg | grep -i "pn54x\|nfc_int\|nfc_ven" | tail -10
echo ""

echo "6. NFC Service Status:"
if [ -f /var/run/nfcDemoApp.pid ]; then
    PID=$(cat /var/run/nfcDemoApp.pid)
    if kill -0 "$PID" 2>/dev/null; then
        echo "   nfcDemoApp: RUNNING (PID $PID)"
    else
        echo "   nfcDemoApp: NOT RUNNING (stale PID file)"
    fi
else
    echo "   nfcDemoApp: NOT RUNNING"
fi
echo ""

echo "7. NFC Log (last 10 lines):"
if [ -f /var/log/nfc.log ]; then
    tail -10 /var/log/nfc.log
else
    echo "   /var/log/nfc.log not found"
fi
echo ""

echo "8. libnfc-nci Config:"
if [ -f /etc/libnfc-nci.conf ]; then
    grep -E "NXP_NFC_DEV_NODE|HOST_LISTEN" /etc/libnfc-nci.conf 2>/dev/null || cat /etc/libnfc-nci.conf
else
    echo "   /etc/libnfc-nci.conf not found"
fi
echo ""

echo "=== End of Diagnostic ==="
