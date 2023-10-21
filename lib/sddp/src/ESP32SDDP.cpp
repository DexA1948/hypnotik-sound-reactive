
#ifdef ARDUINO_ARCH_ESP32 // Tasmota guard for ESP32 devices

#include <functional>
#include "ESP32SDDP.h"
#include "WiFiUdp.h"
#include <Arduino.h>
#define DEBUG_SDDP


static String mac2String(byte ar[]) {
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[3];
    sprintf(buf, "%02X", ar[i]); // J-M-L: slight modification, added the 0 in the format for padding 
    s += buf;
  }
  return s;
}


SDDP::SDDP():
    _server(0),
    _port(1902),
    _ttl(SDDP_MULTICAST_TTL),
    _respondToAddr(0,0,0,0),
    _respondToPort(0),
    _timer_notify_identify(0)
    {
        _sddp_device = new _SDDPDevice();
    }

SDDP::~SDDP() {
    end();
}

bool SDDP::begin(const char * host) {
    end();

    _server = new WiFiUDP; 

    if (!(_server->beginMulticast(IPAddress(SDDP_MULTICAST_ADDR), SDDP_PORT))) {
    #ifdef DEBUG_SDDP
            DEBUG_SERIAL.println("Error begin");
    #endif
        return false;
    }

    /* Broadcast SDDP Notify Online */    
    _setHostName(host);

    IP_local = WiFi.localIP();
    
    /* Needs broadcasting NOTIFY ALIVE at the time of power ON */
    _sddp_device->private_state.nt = SDDP_ALIVE;
    _sddp_device->private_state.search_response = false;

    /* Set Maximum Age of 60 seconds if user has not set Maximum Age parameter */
    if((_sddp_device->config.max_age > DISABLE_PERIODIC_ADVERTS) && (_sddp_device->config.max_age < MIN_ADVERT_PERIOD)) {
        #ifdef DEBUG_SDDP
        DEBUG_SERIAL.printf_P(PSTR("Setting SDDP max_age to minimum of %d seconds"), MIN_ADVERT_PERIOD);
        #endif
        _sddp_device->config.max_age = MIN_ADVERT_PERIOD;
    }

    _sddp_device->private_state.alive_interval = _sddp_device->config.max_age;
    // Send SDDP Notify Online
    if(_send(SDDP_MULTICAST) != SDDP_STATUS_SUCCESS) {
        #ifdef DEBUG_SDDP
        DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed sendig NOTIFY ALIVE response!\r\n"));
        #endif
        return false;
    }
    
    return true;
}

void SDDP::loop() {
    _update();
}

void SDDP::end() {
    
    /* Return if UDPContext object has already been destroyed */
    if(!_server)
        return; 

    /* NOTIFY OFFLINE Response to be boradcasted before going offline */
    _sddp_device->private_state.nt = SDDP_OFFLINE;
    _sddp_device->private_state.search_response = false;
    
    if(_send(SDDP_MULTICAST) != SDDP_STATUS_SUCCESS) {
        #ifdef DEBUG_SDDP
        DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed sendig NOTIFY OFFLINE response!\r\n"));
        #endif
    }
    
    #ifdef DEBUG_SDDP
        DEBUG_SERIAL.printf_P(PSTR("SDDP end ... "));
    #endif

    _server->stop();

    delete (_server);
    delete _sddp_device;

    #ifdef DEBUG_SDDP
        DEBUG_SERIAL.printf_P(PSTR("ok\n"));
    #endif
}

SDDPStatus SDDP::_send(SDDPMessageType type) {

    IPAddress remoteAddr;
    uint16_t remotePort;
    
    SDDPStatus status = SDDP_STATUS_SUCCESS;

    /* Create message buffer */
    char * msg_buf = (char *)malloc(MAX_SDDP_FRAME_SIZE + 1); 
    
    /* Initialize message buffer */ 
    memset(msg_buf, 0, sizeof(MAX_SDDP_FRAME_SIZE + 1));

    /* Create sddp packet */
    status = _SDDPCreatePackate(_sddp_device, msg_buf, MAX_SDDP_FRAME_SIZE);
    if(status == SDDP_STATUS_FATAL_ERROR) {
        #ifdef DEBUG_SDDP
        DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed creating SDDP response packet!\r\n"));
        #endif
    }
        
    if (type == SDDP_MULTICAST) {
        remoteAddr = IPAddress(SDDP_MULTICAST_ADDR);
        remotePort = SDDP_PORT; 
        _server->beginPacket(remoteAddr, remotePort);
        _server->println(msg_buf);       
        if(_server->endPacket() == 0) {
            #ifdef DEBUG_SDDP
            DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed sending SDDP response to multicast address: %s!\r\n"), remoteAddr.toString().c_str());
            #endif
            status = SDDP_STATUS_NETWORK_ERROR;
        }    
    } else {
        remoteAddr = _respondToAddr;
        remotePort = _respondToPort;
        _server->beginPacket(remoteAddr, remotePort);
        _server->println(msg_buf);  
        if(_server->endPacket() == 0) {
            #ifdef DEBUG_SDDP
            DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed sending SDDP response to unicast address: %s!\r\n"), remoteAddr.toString().c_str());
            #endif
            status = SDDP_STATUS_NETWORK_ERROR;
        }        
    }

    free(msg_buf);

    _server->flush();
    
    return status;
}

bool SDDP::sendIdentify(void) {
    _sddp_device->private_state.nt = SDDP_IDENTIFY;
    _sddp_device->private_state.search_response = false;
    if(_send(SDDP_MULTICAST) == SDDP_STATUS_SUCCESS) {
        return true;
    } else {
        return false;
    }
}

void SDDP::_update() {

    SddpPacket *received = (SddpPacket *)malloc(sizeof(*received));
    char * data = (char *)malloc(SDDP_MAX_FRAME_SIZE + 1);
    int recv_len = 0;
        
    if((millis() - _timer_notify_identify) >= (_sddp_device->private_state.alive_interval * 1000)) {
        _timer_notify_identify = millis();
        _sddp_device->private_state.search_response = false;
        _sddp_device->private_state.nt = SDDP_ALIVE;
        if(_send(SDDP_MULTICAST) != SDDP_STATUS_SUCCESS) {
            #ifdef DEBUG_SDDP
            DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed sendig periodic NOTIFY ALIVE response!\r\n"));
            #endif
        }
    }

    _server->parsePacket();

    if(_server && (_server->available() > 0)) {

        _respondToAddr = _server->remoteIP();
        _respondToPort = _server->remotePort();

        recv_len = _server->available();     

        data[recv_len] = 0x00;

        _server->read(data, recv_len);        

        // Limit receive to buffer size
        if (recv_len >= SDDP_MAX_FRAME_SIZE) {
            recv_len = SDDP_MAX_FRAME_SIZE;
        }
    
        received = ParseSDDPPacket(data);

        if (received) {
            if (received->packet_type == SddpPacketRequest) {
                if (!strcmp(received->h.request.version, SDDP_VERSION)) {
                    if (!strcmp(received->h.request.method, "SEARCH")) {
                        if (!strcmp(received->h.request.argument, "*")) {				    
                            _sddp_device->private_state.search_response = true;
                            _sddp_device->private_state.tran = received->tran;
                            if(_send(SDDP_UNICAST) != SDDP_STATUS_SUCCESS) {   /* send search response */
                            #ifdef DEBUG_SDDP
                                DEBUG_SERIAL.printf_P(PSTR("ERROR: Failed sending search response!\r\n"));
                            #endif
                            }                            
                            _sddp_device->private_state.tran = NULL;                            
                        }
                    } else if (!strcmp(received->h.request.method, "NOTIFY")) {
                        // Currently nothing to do here, in future may handle notifications
                    }
                } else {
                    #ifdef DEBUG_SDDP
                    DEBUG_SERIAL.printf_P(PSTR("Unsupported SDDP request packet version: %s"), received->h.request.version);
                    #endif
                }
            } else if (received->packet_type == SddpPacketResponse) {
                if (!strcmp(received->h.request.version, SDDP_VERSION)) {
                    // Currently nothing to do here, in future may handle responses
                } else {
                    #ifdef DEBUG_SDDP
                    DEBUG_SERIAL.printf_P(PSTR("Unsupported SDDP response packet version: %s"), received->h.response.version);
                    #endif
                }
            }    	
    	    FreeSDDPPacket(received);
        } else {
            #ifdef DEBUG_SDDP
            DEBUG_SERIAL.printf_P(PSTR("\r\nUnrecognized SDDP packet: "));
            DEBUG_SERIAL.printf_P(data);
            #endif
        }
    }
    free(data);
}

SDDPStatus SDDP::_SDDPCreatePackate(SDDPDevice *sddp_dev, char *dst, int max_len) {
    char *version = (char *)"1.0";
	char *status_code = (char *)"200";
	char *reason = (char *)"OK";
	char *notify = (char *)"NOTIFY";
	char *alive = (char *)"ALIVE";
	char *offline = (char *)"OFFLINE";
	char *identify = (char *)"IDENTIFY";
	char max_age[10];
    char bffr[30];
	
	sprintf(max_age, "%d", _sddp_device->config.max_age);
	
	SddpPacket * packet = (SddpPacket *)malloc(sizeof(*packet));
	int devInfo = 0;
    
	if (sddp_dev->private_state.search_response) {
		BuildSDDPResponse(packet, version, status_code, reason);        
		packet->tran = sddp_dev->private_state.tran;
		packet->max_age = max_age;
		devInfo = 1;            
	} else {
		if (sddp_dev->private_state.nt == SDDP_ALIVE) {
			BuildSDDPRequest(packet, version, notify, alive);
			packet->max_age = max_age;            
			devInfo = 1;
		} else if (sddp_dev->private_state.nt == SDDP_OFFLINE) {
			BuildSDDPRequest(packet, version, notify, offline);
			packet->type = sddp_dev->config.product_name;
		} else if (sddp_dev->private_state.nt == SDDP_IDENTIFY) {
			BuildSDDPRequest(packet, version, notify, identify);
			devInfo = 1;
		} else {
			return SDDP_STATUS_FATAL_ERROR;
        }  
	}
    
	packet->host = _host_name;
    sprintf(bffr, "%s:%d", IP_local.toString().c_str(), SDDP_PORT);
    packet->from = bffr;
	
	if (devInfo) {
		packet->type = sddp_dev->config.product_name;
		packet->primary_proxy = sddp_dev->config.primary_proxy;
		packet->proxies = sddp_dev->config.proxies;
		packet->manufacturer = sddp_dev->config.manufacturer;
		packet->model = sddp_dev->config.model;
		packet->driver = sddp_dev->config.driver;
	}
	
	if (WriteSDDPPacket(packet, dst, max_len) > (unsigned int)max_len) {
        #ifdef DEBUG_SDDP
		DEBUG_SERIAL.printf_P(PSTR("Create notify could not build the packet"));
		#endif
        dst = NULL;
		return SDDP_STATUS_FATAL_ERROR;
	}	
	return SDDP_STATUS_SUCCESS;
}

void SDDP::setDevice(SDDPDeviceConfig * dev) {
    
    _sddp_device->config.driver = dev->driver;
    _sddp_device->config.manufacturer = dev->manufacturer;
    _sddp_device->config.max_age = dev->max_age;
    _sddp_device->config.model = dev->model;
    _sddp_device->config.primary_proxy = dev->primary_proxy;
    _sddp_device->config.product_name = dev->product_name;
    _sddp_device->config.proxies = dev->proxies;
    _sddp_device->config.type = dev->type;
    
    IPAddress config_ip = WiFi.localIP();
    sprintf(_conf_url, "http://%s/", config_ip.toString().c_str());    
}

void SDDP::_setHostName(const char *h) {
    char host[SDDP_MAX_HOST_SIZE];
    if(h != NULL) {
        sprintf_P(host, "%s", h);
    } else {
        uint64_t EfuseMac = ESP.getEfuseMac(); //WiFi.macAddress().toInt();
        String device_mac = mac2String((byte*) &EfuseMac);          
        sprintf_P(host, "ESP32-%s", device_mac.c_str());
    }
    strlcpy(_host_name, host, sizeof(_host_name));
}

#endif