# Enabling TLS-Secured MQTT Connection (MQTTS)

This document describes how to enable TLS-secured MQTT connections (port 8883) for NFC terminals connecting to mqbase.io.

## Overview

Currently, NFC terminals connect to mqbase.io using plain MQTT (port 1883). This document outlines the steps to enable MQTTS (port 8883) using the system CA certificate bundle to validate Let's Encrypt server certificates.

### Current State

| Component | Status |
|-----------|--------|
| MQTT Port | 1883 (unencrypted) |
| OpenSSL | Not installed |
| CA Certificates | Not available |
| Paho MQTT SSL | Disabled |

### Target State

| Component | Status |
|-----------|--------|
| MQTT Port | 8883 (TLS encrypted) |
| OpenSSL | Installed |
| CA Certificates | System bundle (`/etc/ssl/certs/ca-certificates.crt`) |
| Paho MQTT SSL | Enabled |

## Implementation Steps

### Step 1: Update Buildroot Configuration

Edit `configs/nfc_terminal_cm4_defconfig` to enable OpenSSL and CA certificates:

```diff
 # Security packages
+BR2_PACKAGE_OPENSSL=y
+BR2_PACKAGE_CA_CERTIFICATES=y
```

**What this does:**
- `BR2_PACKAGE_OPENSSL`: Installs OpenSSL libraries (libssl, libcrypto) on the target
- `BR2_PACKAGE_CA_CERTIFICATES`: Installs Mozilla's CA certificate bundle to `/etc/ssl/certs/ca-certificates.crt`

The CA bundle includes **ISRG Root X1** - Let's Encrypt's root certificate authority.

**Size impact:** ~200KB additional rootfs size.

### Step 2: Verify Paho MQTT SSL Support

The Paho MQTT C library is already configured to detect OpenSSL automatically in `buildroot/package/paho-mqtt-c/paho-mqtt-c.mk`:

```makefile
ifeq ($(BR2_PACKAGE_OPENSSL),y)
PAHO_MQTT_C_DEPENDENCIES += openssl
PAHO_MQTT_C_CONF_OPTS += -DPAHO_WITH_SSL=TRUE
else
PAHO_MQTT_C_CONF_OPTS += -DPAHO_WITH_SSL=FALSE
endif
```

Once `BR2_PACKAGE_OPENSSL=y` is set, Paho will automatically be built with SSL support.

### Step 3: Update Application Code

Modify `package/nfc-lvgl-app/src/main.c` to support TLS connections.

#### 3.1 Add SSL Header Include

Add near the top of the file with other includes:

```c
#include "MQTTAsync.h"
// SSL support is included via MQTTAsync.h when compiled with SSL
```

#### 3.2 Add Port Configuration

Add a new configuration option for the MQTT port:

```c
#define MQTT_PORT_DEFAULT    8883        /* Default to secure port */
#define MQTT_USE_TLS_DEFAULT 1           /* Enable TLS by default */

static int mqtt_port = MQTT_PORT_DEFAULT;
static int mqtt_use_tls = MQTT_USE_TLS_DEFAULT;
```

Add config file parsing for port:

```c
// In load_config() function, add:
else if (strcmp(key, "mqtt_port") == 0) {
    mqtt_port = atoi(value);
}
else if (strcmp(key, "mqtt_tls") == 0) {
    mqtt_use_tls = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
}
```

#### 3.3 Modify mqtt_init() Function

Update the connection initialization to support TLS:

```c
static int mqtt_init(void)
{
    int rc;
    char url[256];
    
    // Build broker URL with appropriate protocol
    if (mqtt_use_tls) {
        snprintf(url, sizeof(url), "ssl://%s:%d", mqtt_address, mqtt_port);
    } else {
        snprintf(url, sizeof(url), "tcp://%s:%d", mqtt_address, mqtt_port);
    }
    
    rc = MQTTAsync_create(&client, url, mqtt_client_id,
                          MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        fprintf(stderr, "Failed to create MQTT client: %d\n", rc);
        return rc;
    }
    
    // ... existing callback setup ...
    
    return MQTTASYNC_SUCCESS;
}
```

#### 3.4 Update mqtt_connect() Function

Add SSL options to the connection:

```c
static int mqtt_connect(void)
{
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer5;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    int rc;
    
    conn_opts.keepAliveInterval = 10;
    conn_opts.cleansession = 1;
    conn_opts.username = mqtt_username;
    conn_opts.password = mqtt_password;
    conn_opts.MQTTVersion = MQTTVERSION_5;
    conn_opts.automaticReconnect = 1;
    conn_opts.minRetryInterval = 1;
    conn_opts.maxRetryInterval = 60;
    conn_opts.onSuccess5 = on_connect_success;
    conn_opts.onFailure5 = on_connect_failure;
    conn_opts.context = client;
    
    // Configure SSL/TLS if enabled
    if (mqtt_use_tls) {
        ssl_opts.trustStore = "/etc/ssl/certs/ca-certificates.crt";
        ssl_opts.enableServerCertAuth = 1;
        ssl_opts.verify = 1;
        ssl_opts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;  // Minimum TLS 1.2
        conn_opts.ssl = &ssl_opts;
        
        printf("MQTT: Connecting with TLS to %s:%d\n", mqtt_address, mqtt_port);
    } else {
        printf("MQTT: Connecting (unencrypted) to %s:%d\n", mqtt_address, mqtt_port);
    }
    
    rc = MQTTAsync_connect(client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        fprintf(stderr, "Failed to start MQTT connect: %d\n", rc);
        return rc;
    }
    
    return MQTTASYNC_SUCCESS;
}
```

### Step 4: Update Configuration File Format

Update the default config and documentation to include new options:

**Default `/data/nfc-terminal.conf`:**

```ini
mqtt_addr=mqbase.io
mqtt_port=8883
mqtt_tls=1
mqtt_user=guest
mqtt_pswd=guest
```

**For backwards compatibility with existing deployments using plain MQTT:**

```ini
mqtt_addr=mqbase.io
mqtt_port=1883
mqtt_tls=0
mqtt_user=guest
mqtt_pswd=guest
```

### Step 5: Update UI (Optional)

Consider adding a TLS toggle in the settings screen if it doesn't already exist:

```c
// In create_settings_screen() or equivalent
lv_obj_t *tls_switch = lv_switch_create(parent);
lv_obj_add_event_cb(tls_switch, tls_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
if (mqtt_use_tls) {
    lv_obj_add_state(tls_switch, LV_STATE_CHECKED);
}
```

## Build and Test

### Rebuild the Image

```bash
# Clean Paho MQTT to force rebuild with SSL
rm -rf output/build/paho-mqtt-c-*

# Update defconfig
make nfc_terminal_cm4_defconfig

# Rebuild
make
```

### Verify OpenSSL is Included

After building, check the rootfs:

```bash
# Check for OpenSSL libraries
ls -la output/target/usr/lib/libssl* output/target/usr/lib/libcrypto*

# Check for CA certificates
ls -la output/target/etc/ssl/certs/ca-certificates.crt
```

### Test MQTT Connection

1. Flash the new image to an NFC terminal
2. Verify TLS connection in logs:
   ```
   MQTT: Connecting with TLS to mqbase.io:8883
   ```
3. Test with tcpdump/Wireshark - traffic should be encrypted

### Fallback Testing

Test that unencrypted mode still works for environments without MQTTS:

```ini
# In /data/nfc-terminal.conf
mqtt_port=1883
mqtt_tls=0
```

## Troubleshooting

### Connection Fails with Certificate Error

**Symptom:** `SSL certificate problem: unable to get local issuer certificate`

**Cause:** CA bundle not installed or path incorrect

**Fix:** Verify `/etc/ssl/certs/ca-certificates.crt` exists and is readable

### Connection Timeout on Port 8883

**Symptom:** Connection hangs or times out

**Causes:**
1. Firewall blocking port 8883
2. Server not configured for MQTTS

**Fix:** Test with mosquitto_pub:
```bash
mosquitto_pub -h mqbase.io -p 8883 --cafile /etc/ssl/certs/ca-certificates.crt \
    -u guest -P guest -t test/hello -m "test" -d
```

### Paho Not Compiled with SSL

**Symptom:** `MQTTAsync_SSLOptions` not recognized or SSL features missing

**Cause:** Paho built before OpenSSL was enabled

**Fix:** Clean and rebuild Paho:
```bash
rm -rf output/build/paho-mqtt-c-*
make paho-mqtt-c-rebuild
```

## Security Considerations

### Certificate Validation

The implementation enables full server certificate validation:
- `enableServerCertAuth = 1`: Verify server certificate against CA bundle
- `verify = 1`: Enable hostname verification
- `sslVersion = MQTT_SSL_VERSION_TLS_1_2`: Minimum TLS version

### Certificate Updates

The CA certificate bundle is updated with Buildroot releases. For production:
- Consider a mechanism to update CA certificates via OTA
- Monitor Let's Encrypt certificate chain changes

### Fallback Security

When `mqtt_tls=0`, connections are unencrypted. Consider:
- Removing the fallback option in production builds
- Warning users when TLS is disabled

## Files Changed Summary

| File | Change |
|------|--------|
| `configs/nfc_terminal_cm4_defconfig` | Add `BR2_PACKAGE_OPENSSL=y`, `BR2_PACKAGE_CA_CERTIFICATES=y` |
| `package/nfc-lvgl-app/src/main.c` | Add SSL options to MQTT connection |
| `/data/nfc-terminal.conf` | Add `mqtt_port` and `mqtt_tls` options |

## References

- [Paho MQTT C SSL Documentation](https://www.eclipse.org/paho/files/mqttdoc/MQTTAsync/html/ssl.html)
- [Let's Encrypt Chain of Trust](https://letsencrypt.org/certificates/)
- [mqbase.io MQTTS Endpoint](https://mqbase.io) - Port 8883
