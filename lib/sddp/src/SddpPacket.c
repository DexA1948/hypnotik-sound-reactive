/**************************************************************************//**

 @file     SddpPacket.c

 @brief    Implements an SDDP packet parser 
 
 @section  Detail Detail
            This module provides a SDDP packet parser.

******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "SddpPacket.h"

static void freePacketHeader(char **header)
{
	if (*header)
	{
		free(*header);
		*header = NULL;
	}
}

static const char *skipSpaces(const char *line, const char *lineEnd)
{
	while (line < lineEnd)
	{
		if (*line != ' ')
			return line;
		line++;
	}
	
	return NULL;
}

static const char *skipToken(const char *line, const char *lineEnd)
{
	line = skipSpaces(line, lineEnd);
	if (line && line < lineEnd)
	{
		while (line < lineEnd)
		{
			if (*line == ' ')
				return line;
			line++;
		}
		
		return lineEnd;
	}
	
	return NULL;
}

static const char *findToken(const char *line, const char *lineEnd, const char *current)
{
	if (current)
	{
		line = skipToken(current, lineEnd);
		if (!line)
			return NULL;
	}
	
	return skipSpaces(line, lineEnd);
}

static char *dupLine(const char *line, const char *lineEnd)
{
	char *str;
	size_t len = 0;
	
	if (lineEnd >= line)
		len = lineEnd - line;
	str = malloc(len + 1);
	if (str)
	{
		memcpy(str, line, len);
		str[len] = '\0';
	}
	
	return str;
}

static char *dupToken(const char *token, const char *lineEnd)
{
	const char *tokenEnd;
	
	tokenEnd = skipToken(token, lineEnd);
	if (!tokenEnd)
		tokenEnd = lineEnd;
	assert(tokenEnd <= lineEnd);
	
	return dupLine(token, tokenEnd);
}

static SddpPacket *allocPacket(SddpPacketType type)
{
	SddpPacket *packet = malloc(sizeof(*packet));
	if (packet)
	{
		memset(packet, 0, sizeof(*packet));
		packet->packet_type = type;
	}
	return packet;
}

static const char *parseHeaderValue(const char *line, const char *lineEnd, size_t *headerLen, const char **value)
{
	const char *p;
	
	*headerLen = 0;
	*value = NULL;
	
	for (p = line; p < lineEnd; p++)
	{
		if (*p == ':')
		{
			*headerLen = p - line;
			*value = skipSpaces(p + 1, lineEnd);
			return line;
		}
	}
	
	return NULL;
}

static int isHeader(const char *header, size_t headerLen, const char *cmp)
{
	if (headerLen == strlen(cmp) && strncasecmp(header, cmp, headerLen) == 0)
		return 1;
	
	return 0;
}

static const char *trimRight(const char *line, const char *lineEnd)
{
	while (line < lineEnd)
	{
		lineEnd--;
		if (*lineEnd != ' ' && *lineEnd != '\t')
		{
			lineEnd++;
			break;
		}
	}
	
	return lineEnd;
}

static char *dupValue(const char *value, const char *lineEnd)
{
	lineEnd = trimRight(value, lineEnd);
	
	if (value < lineEnd && *value == '\"' && *(lineEnd - 1) == '\"')
	{
		value++;
		lineEnd--;
	}
	
	return dupLine(value, lineEnd);
}

static int parseHeaderLine(SddpPacket *packet, const char *line, const char *lineEnd)
{
	const char *header, *value;
	size_t headerLen;
	
	header = parseHeaderValue(line, lineEnd, &headerLen, &value);
	if (!header)
		return 0;
	if (!value)
	{
		value = "";
		lineEnd = value;
	}
	
	if (isHeader(header, headerLen, "From"))
		packet->from = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Host"))
		packet->host = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Tran"))
		packet->tran = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Max-Age"))
		packet->max_age = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Timeout"))
		packet->timeout = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Primary-Proxy"))
		packet->primary_proxy = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Proxies"))
		packet->proxies = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Manufacturer"))
		packet->manufacturer = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Model"))
		packet->model = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Driver"))
		packet->driver = dupValue(value, lineEnd);
	else if (isHeader(header, headerLen, "Type"))
		packet->type = dupValue(value, lineEnd);
	return 1;
}

static char *dupVersion(const char *protocol, const char *protocolPrefix, const char *lineEnd)
{
	const char *tokenEnd;
	size_t prefixLen = strlen(protocolPrefix);
	
	assert(strncmp(protocol, protocolPrefix, prefixLen) == 0);
	
	tokenEnd = skipToken(protocol + prefixLen, lineEnd);
	return dupToken(protocol + prefixLen, tokenEnd);
}

static SddpPacket *parseFirstLine(const char *line, const char *lineEnd)
{
	const char *protocol, *status, *reason, *method, *argument;
	SddpPacket *packet = NULL;
	
	protocol = findToken(line, lineEnd, NULL);
	if (!protocol)
		return NULL;
	if (strncmp(protocol, "SDDP/", 5) == 0 && strlen(protocol) > 5)
	{
		status = findToken(line, lineEnd, protocol);
		if (!status)
			return NULL;
		reason = findToken(line, lineEnd, status);
		if (!reason)
			return NULL;
		
		packet = allocPacket(SddpPacketResponse);
		if (packet)
		{
			packet->h.response.status_code = dupToken(status, lineEnd);
			packet->h.response.reason = dupLine(reason, lineEnd);
			packet->h.response.version = dupVersion(protocol, "SDDP/", status);
		}
	}
	else
	{
		method = protocol;
		argument = findToken(line, lineEnd, method);
		if (!argument)
			return NULL;
		protocol = findToken(line, lineEnd, argument);
		if (!protocol)
			return NULL;
		if (strncmp(protocol, "SDDP/", 5) != 0 || strlen(protocol) <= 5)
			return NULL;
		
		packet = allocPacket(SddpPacketRequest);
		if (packet)
		{
			packet->h.request.method = dupToken(method, lineEnd);
			packet->h.request.argument = dupToken(argument, lineEnd);
			packet->h.request.version = dupVersion(protocol, "SDDP/", lineEnd);
		}
	}
	
	return packet;
}

SddpPacket *ParseSDDPPacket(const char *packet)
{
	SddpPacket *p = NULL;
	const char *lineEnd, *nextLine;
	const char *line = packet;
	int i;
	
	for (i = 0; line; i++)
	{
		lineEnd = strstr(line, "\r\n");
		if (lineEnd)
			nextLine = lineEnd + 2;
		else
		{
			lineEnd = line + strlen(line);
			nextLine = NULL;
		}
		
		if (lineEnd == line)
		{
			if (p && nextLine != NULL)
			{
				assert(i > 0);
				p->body = dupLine(nextLine, nextLine + strlen(nextLine));
			}
			
			break;
		}
		
		if (i == 0)
		{
			p = parseFirstLine(line, lineEnd);
			if (!p)
				break;
		}
		else
		{
			assert(p);
			if (!parseHeaderLine(p, line, lineEnd))
			{
				FreeSDDPPacket(p);
				return NULL;
			}
		}
		
		line = nextLine;
	}
	
	return p;
}

void FreeSDDPPacket(SddpPacket *packet)
{
	switch (packet->packet_type)
	{
		case SddpPacketRequest:
			freePacketHeader(&packet->h.request.method);
			freePacketHeader(&packet->h.request.argument);
			freePacketHeader(&packet->h.request.version);
			break;
		case SddpPacketResponse:
			freePacketHeader(&packet->h.response.status_code);
			freePacketHeader(&packet->h.response.reason);
			freePacketHeader(&packet->h.response.version);
			break;
		default:
			break;
	}
	
	packet->packet_type = SddpPacketUnknown;
	freePacketHeader(&packet->from);
	freePacketHeader(&packet->host);
	freePacketHeader(&packet->tran);
	freePacketHeader(&packet->max_age);
	freePacketHeader(&packet->timeout);
	freePacketHeader(&packet->primary_proxy);
	freePacketHeader(&packet->proxies);
	freePacketHeader(&packet->manufacturer);
	freePacketHeader(&packet->model);
	freePacketHeader(&packet->driver);
	freePacketHeader(&packet->type);
	freePacketHeader(&packet->body);
	free(packet);
}

static int hasValue(const char *val)
{
	return val != NULL && *val != '\0';
}

static void writeBuffer(size_t *written, char *buffer, size_t bufferSize, const char *str)
{
	*written += strlen(str);
	if (buffer && bufferSize > *written)
		strcat(buffer, str);
}

static void writeHeader(size_t *written, char *buffer, size_t bufferSize, const char *header, const char *value, int forceQuotes)
{
	if (value)
	{
		writeBuffer(written, buffer, bufferSize, header);
		if (forceQuotes)
			writeBuffer(written, buffer, bufferSize, ": \"");
		else
			writeBuffer(written, buffer, bufferSize, ": ");
		writeBuffer(written, buffer, bufferSize, value);
		if (forceQuotes)
			writeBuffer(written, buffer, bufferSize, "\"\r\n");
		else
			writeBuffer(written, buffer, bufferSize, "\r\n");
	}
}

void BuildSDDPResponse(SddpPacket *packet, char *version, char *statusCode, char *reason)
{
	memset(packet, 0, sizeof(*packet));
	packet->packet_type = SddpPacketResponse;
	packet->h.response.status_code = statusCode;
	packet->h.response.reason = reason;
	packet->h.response.version = version;
}

void BuildSDDPRequest(SddpPacket *packet, char *version, char *method, char *argument)
{
	memset(packet, 0, sizeof(*packet));
	packet->packet_type = SddpPacketRequest;
	packet->h.request.method = method;
	packet->h.request.argument = argument;
	packet->h.request.version = version;
}

size_t WriteSDDPPacket(const SddpPacket *packet, char *buffer, size_t bufferSize)
{
	size_t written = 1;
	
	if (buffer && bufferSize > 0)
		buffer[0] = '\0';
	
	switch (packet->packet_type)
	{
		case SddpPacketRequest:
			if (!hasValue(packet->h.request.method) ||
			    !hasValue(packet->h.request.argument) ||
			    !hasValue(packet->h.request.version))
			    return 0;
			
			// METHOD Argument SDDP/Version
			writeBuffer(&written, buffer, bufferSize, packet->h.request.method);
			writeBuffer(&written, buffer, bufferSize, " ");
			writeBuffer(&written, buffer, bufferSize, packet->h.request.argument);
			writeBuffer(&written, buffer, bufferSize, " SDDP/");
			writeBuffer(&written, buffer, bufferSize, packet->h.request.version);
			writeBuffer(&written, buffer, bufferSize, "\r\n");
			break;
		case SddpPacketResponse:
			if (!hasValue(packet->h.response.status_code) ||
			    !hasValue(packet->h.response.reason) ||
			    !hasValue(packet->h.response.version))
			    return 0;
			
			// SDDP/Version StatusCode Reason
			writeBuffer(&written, buffer, bufferSize, "SDDP/");
			writeBuffer(&written, buffer, bufferSize, packet->h.response.version);
			writeBuffer(&written, buffer, bufferSize, " ");
			writeBuffer(&written, buffer, bufferSize, packet->h.response.status_code);
			writeBuffer(&written, buffer, bufferSize, " ");
			writeBuffer(&written, buffer, bufferSize, packet->h.response.reason);
			writeBuffer(&written, buffer, bufferSize, "\r\n");
			break;
		default:
			return 0;
	}
	
	writeHeader(&written, buffer, bufferSize, "From", packet->from, 1);
	writeHeader(&written, buffer, bufferSize, "Host", packet->host, 1);
	writeHeader(&written, buffer, bufferSize, "Tran", packet->tran, 0);
	writeHeader(&written, buffer, bufferSize, "Max-Age", packet->max_age, 0);
	writeHeader(&written, buffer, bufferSize, "Type", packet->type, 1);
	writeHeader(&written, buffer, bufferSize, "Timeout", packet->timeout, 0);
	writeHeader(&written, buffer, bufferSize, "Primary-Proxy", packet->primary_proxy, 1);
	writeHeader(&written, buffer, bufferSize, "Proxies", packet->proxies, 1);
	writeHeader(&written, buffer, bufferSize, "Manufacturer", packet->manufacturer, 1);
	writeHeader(&written, buffer, bufferSize, "Model", packet->model, 1);
	writeHeader(&written, buffer, bufferSize, "Driver", packet->driver, 1);
	
	if (packet->body != NULL)
	{
		writeBuffer(&written, buffer, bufferSize, "\r\n");
		writeBuffer(&written, buffer, bufferSize, packet->body);
	}
	
	return written;
}

