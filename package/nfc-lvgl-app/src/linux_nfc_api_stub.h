/**
 * @file linux_nfc_api_stub.h
 * Stub implementation of linux_nfc_api.h for desktop builds
 * 
 * This provides empty implementations of the NFC functions
 * so the app can be built and tested on desktop without NFC hardware.
 */

#ifndef LINUX_NFC_API_STUB_H
#define LINUX_NFC_API_STUB_H

#include <stdint.h>
#include <stdio.h>

/* Technology masks */
#define DEFAULT_NFA_TECH_MASK 0xFF

/* Tag info structure */
typedef struct {
    uint8_t uid[10];
    uint8_t uid_length;
    uint8_t protocol;
    uint8_t technology;
    uint8_t handle;
    uint8_t nfcid1[10];
    uint8_t nfcid1_length;
} nfc_tag_info_t;

/* Callback structure */
typedef struct {
    void (*onTagArrival)(nfc_tag_info_t *tag_info);
    void (*onTagDeparture)(void);
} nfcTagCallback_t;

/* Stub implementations - inline to avoid linker issues */
static inline int nfcManager_doInitialize(void) {
    printf("[NFC STUB] nfcManager_doInitialize() - simulated success\n");
    return 0;  /* Success */
}

static inline void nfcManager_doDeinitialize(void) {
    printf("[NFC STUB] nfcManager_doDeinitialize()\n");
}

static inline void nfcManager_registerTagCallback(nfcTagCallback_t *callbacks) {
    (void)callbacks;
    printf("[NFC STUB] nfcManager_registerTagCallback()\n");
}

static inline void nfcManager_deregisterTagCallback(void) {
    printf("[NFC STUB] nfcManager_deregisterTagCallback()\n");
}

static inline void nfcManager_enableDiscovery(int tech_mask, int enable_host_routing, 
                                               int reader_mode, int restart) {
    (void)tech_mask;
    (void)enable_host_routing;
    (void)reader_mode;
    (void)restart;
    printf("[NFC STUB] nfcManager_enableDiscovery()\n");
}

static inline void nfcManager_disableDiscovery(void) {
    printf("[NFC STUB] nfcManager_disableDiscovery()\n");
}

#endif /* LINUX_NFC_API_STUB_H */
