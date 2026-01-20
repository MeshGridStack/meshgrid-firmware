/**
 * meshgrid-firmware version information
 *
 * Set version via platformio.ini build flags:
 *   -DMESHGRID_VERSION=\"0.0.1\"
 *   -DMESHGRID_BUILD_DATE=\"2025-01-07\"
 */

#ifndef MESHGRID_VERSION_H
#define MESHGRID_VERSION_H

/* Version format: MAJOR.MINOR.PATCH */
#ifndef MESHGRID_VERSION
#    define MESHGRID_VERSION "0.0.1"
#endif

/* Build date (set by CI or manual build) */
#ifndef MESHGRID_BUILD_DATE
#    define MESHGRID_BUILD_DATE __DATE__
#endif

/* Git commit hash (set by CI) */
#ifndef MESHGRID_GIT_HASH
#    define MESHGRID_GIT_HASH ""
#endif

/* Firmware type identifiers */
#define MESHGRID_FIRMWARE_ID 0x4D47 /* "MG" */
#define MESHCORE_FIRMWARE_ID 0x4D43 /* "MC" */

/* Protocol compatibility version */
#define MESHGRID_PROTOCOL_COMPAT 1

/*
 * Node type encoding for advertisements
 * Lower 4 bits: device type (compatible with MeshCore ADV_TYPE_*)
 * Upper 4 bits: firmware identifier
 *   0x0X = MeshCore
 *   0x1X = meshgrid
 */
#define MESHGRID_ADV_TYPE_CLIENT 0x11   /* meshgrid client */
#define MESHGRID_ADV_TYPE_REPEATER 0x12 /* meshgrid repeater */
#define MESHGRID_ADV_TYPE_ROOM 0x13     /* meshgrid room */

/* MeshCore types (for identification) */
#define MESHCORE_ADV_TYPE_CHAT 0x01
#define MESHCORE_ADV_TYPE_REPEATER 0x02
#define MESHCORE_ADV_TYPE_ROOM 0x03
#define MESHCORE_ADV_TYPE_SENSOR 0x04

/* Check if a node is meshgrid or meshcore */
#define IS_MESHGRID_NODE(adv_type) (((adv_type)&0x10) != 0)
#define IS_MESHCORE_NODE(adv_type) (((adv_type)&0x10) == 0)

/* Get base device type (client/repeater/room) regardless of firmware */
#define GET_DEVICE_TYPE(adv_type) ((adv_type)&0x0F)

/*
 * Version info structure for display/logging
 */
struct meshgrid_version {
    const char* version;
    const char* build_date;
    const char* git_hash;
    uint16_t firmware_id;
};

static inline void meshgrid_get_version(struct meshgrid_version* v) {
    v->version = MESHGRID_VERSION;
    v->build_date = MESHGRID_BUILD_DATE;
    v->git_hash = MESHGRID_GIT_HASH;
    v->firmware_id = MESHGRID_FIRMWARE_ID;
}

#endif /* MESHGRID_VERSION_H */
