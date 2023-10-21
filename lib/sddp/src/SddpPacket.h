/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*                                                                            *
* Copyright (c) 2012, by Control4 Inc.  All rights reserved.                 *
*                                                                            *
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifndef _SDDP_PACKET_H_
#define _SDDP_PACKET_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	SddpPacketUnknown = 0,
	SddpPacketRequest,
	SddpPacketResponse
} SddpPacketType;

typedef struct
{
	SddpPacketType packet_type;
	union
	{
		struct
		{
			char *method;
			char *argument;
			char *version;
		} request;
		struct
		{
			char *status_code;
			char *reason;
			char *version;
		} response;
	} h;
	
	char *from;
	char *host;
	char *tran;
	char *max_age;
	char *timeout;
	char *primary_proxy;
	char *proxies;
	char *manufacturer;
	char *model;
	char *driver;
	char *type;
	
	char *body;
} SddpPacket;

void BuildSDDPResponse(SddpPacket *packet, char *version, char *statusCode, char *reason);
void BuildSDDPRequest(SddpPacket *packet, char *version, char *method, char *argument);
SddpPacket *ParseSDDPPacket(const char *packet);
void FreeSDDPPacket(SddpPacket *packet);
size_t WriteSDDPPacket(const SddpPacket *packet, char *buffer, size_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
