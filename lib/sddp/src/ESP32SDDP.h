#ifdef ARDUINO_ARCH_ESP32

#ifndef ESP32SDDP_H
#define ESP32SDDP_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

extern "C" {
  #include "SddpPacket.h"
  #include "SddpStatus.h"  
}


/* Copied from SDDP C API */
///////////////////////////////////////
// Defines
///////////////////////////////////////
#define SDDP_MAX_HOST_SIZE                  100
#define SDDP_MAX_FRAME_SIZE                 512 // local buffer size allowed for incoming and outgoing SDDP
#define SDDP_PRODUCT_NAME_SIZE              100
#define SDDP_PRIMARY_PROXY_SIZE             100
#define SDDP_PROXIES_SIZE                   256
#define SDDP_MANIFACT_SIZE                  100
#define SDDP_MODEL_SIZE                     100
#define SDDP_TYPE_SIZE                      100
#define SDDP_DRIVER_NAME_SIZE               100

#define MIN_ADVERT_PERIOD                   60  // Minimum allow advertisement period (0 is disabled)
#define DISABLE_PERIODIC_ADVERTS            0   // Provide as 'max_age' to disable period advertisements
#define MAX_SDDP_FRAME_SIZE                 512 // local buffer size allowed for incoming and outgoing SDDP
#define MAX_HOST_SIZE                       100

#define SDDP_VERSION                        "1.0"

#define SDDP_CONF_URL_SIZE                  128
#define SDDP_PORT                           1902
#define SDDP_MULTICAST_TTL                  32
#define SDDP_MULTICAST_ADDR                 239, 255, 255, 250

/**************************************************************************//**
 @typedef  struct SDDPConfig
           Describes a device by providing an instance of the vendor specific
           SDDP data. This data is product specific and describes the device
           to the SDDP module.
******************************************************************************/
typedef struct {
    char *product_name;  // Product name for SDDP search target - i.e. "c4:television"
    char *type;                   // 
    char *primary_proxy; // Control4 primary proxy type - i.e. "TV"
    char *proxies;            // All Control4 proxy types support - i.e. "TV,DVD"
    char *manufacturer;  // Manufacturer - i.e. "Control4"
    char *model;         // Model number - i.e. "C4-105HCTV2-EB"
    char *driver;        // Control4 driver c4i - i.e. "Control4TVGen.c4i"
    int  max_age;        // Number of seconds advertisement is valid (controls
                         // advertisement interval) - i.e. 1800
    bool local_only;     // If this device information should only be broadcast locally
} SDDPDeviceConfig;

///////////////////////////////////////
// Typedefs
///////////////////////////////////////
/**************************************************************************//**
 @enum     SDDPNotificationSubtype
           Notification type
******************************************************************************/
typedef enum
{
    SDDP_NONE,
    SDDP_ALIVE,
    SDDP_OFFLINE,
    SDDP_IDENTIFY
} NotificationType;


/**************************************************************************//**
 @typedef  struct SDDPPrivateState
           Describes private SDDP information about a device and associated
           controller.
******************************************************************************/
typedef struct {
    bool                search_response;    // True if this a search response
    char               *tran;               // Request transaction
    NotificationType    nt;                 // Determines notification type
    time_t              time_prev;          // Used for periodic announcements
    unsigned int        alive_interval;     // Used for periodic announcements
} SDDPPrivateState;

typedef struct _SDDPDevice {
    SDDPDeviceConfig    config;
    SDDPPrivateState    private_state;
} SDDPDevice;


/**************************************************************************//**
 @typedef  struct SDDPState
           This is a wrapper for SDDP datastructures provided to SDDP API calls.

           'devices' is a list of device specific configuration required prior to
           using SDDP.

           'network_info' provides access to the socket file descriptors used
           by the SDDP module.

           'private_state' is information required by the SDDP module itself,
           and should not be modified.
******************************************************************************/
typedef struct {
    SDDPDevice      *device;
    bool             sddp_enabled;        // True if sddp is running
    char             host[MAX_HOST_SIZE]; // The hostname of the device
} SDDPState;


/**************************************************************************//**
 @enum     SDDPMessageType
           Announce destination
******************************************************************************/
typedef enum {
  SDDP_MULTICAST,
  SDDP_UNICAST
} SDDPMessageType;

/**
 * @brief SDDPTimer
 * Timer module derrived from esp8266 API ESTTimer
 * Keeps time tick record so as to boradcast NOTIFY ALIVE on fixed interval
 */

#define EN_SDDP_DEBUG
#ifdef EN_SDDP_DEBUG
    #define DEBUG_SERIAL  Serial
#endif

class SDDP {

    public:
        SDDP();
        ~SDDP();
        
        bool begin(const char * host);
        void loop();
        void end();        
        
        void setDevice(SDDPDeviceConfig * dev);                      
        bool sendIdentify(void);

    protected:

        SDDPStatus _send(SDDPMessageType type);
        void _update();
        static void _onTimerStatic(SDDP* self);
        SDDPStatus _SDDPCreatePackate(SDDPDevice *sddp_dev, char *dst, int max_len);
        void _setHostName(const char *h);
        
        SDDPDevice * _sddp_device;
        WiFiUDP *_server;
        uint16_t _port;
        uint8_t _ttl;
        IPAddress IP_local;
        IPAddress _respondToAddr;
        uint16_t  _respondToPort;
        uint32_t _timer_notify_identify;

        char _host_name[SDDP_MAX_HOST_SIZE];
        char _conf_url[SDDP_CONF_URL_SIZE];
};

#endif
#endif