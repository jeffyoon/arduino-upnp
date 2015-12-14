/*
  Parsing.cpp - HTTP request parsing.

  Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.
  Copyright (c) 2015 Danny Backx. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
  Modified November 2015 by Danny Backx (cut code we don't need for UPnP XML messages).
*/

#include <Arduino.h>
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "UPnP/WebServer.h"
#include "UPnP/Headers.h"

#undef DEBUG_OUTPUT
// #define DEBUG_OUTPUT Serial

extern char *upnp_headers[];

bool WebServer::_parseRequest(WiFiClient& client) {
  // Read the first line of HTTP request
  String req = client.readStringUntil('\r');
  client.readStringUntil('\n');

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
#ifdef DEBUG_OUTPUT
    DEBUG_OUTPUT.print("Invalid request: ");
    DEBUG_OUTPUT.println(req);
#endif
    return false;
  }

  String methodStr = req.substring(0, addr_start);
  _method = methodStr.c_str();
  String url = req.substring(addr_start + 1, addr_end);
  String searchStr = "";
  int hasSearch = url.indexOf('?');
  if (hasSearch != -1){
    searchStr = url.substring(hasSearch + 1);
    url = url.substring(0, hasSearch);
  }
  _currentUri = url;

  HTTPMethod method = HTTP_GET;
  for (int i=HTTP_ANY; i<HTTP_END_METHODS; i++)
    if (methodStr == http_method_strings[i])
      method = (HTTPMethod)i;
  _currentMethod = method;

#ifdef DEBUG_OUTPUT
  DEBUG_OUTPUT.print("method: ");
  DEBUG_OUTPUT.print(methodStr);
  DEBUG_OUTPUT.print(" url: ");
  DEBUG_OUTPUT.print(url);
  DEBUG_OUTPUT.print(" search: ");
  DEBUG_OUTPUT.println(searchStr);
#endif

  // below is needed only when POST type request
  if (method == HTTP_POST || method == HTTP_PUT || method == HTTP_PATCH || method == HTTP_DELETE) {
    String boundaryStr;
    String headerName;
    String headerValue;
    uint32_t contentLength = 0;

    // parse headers
    while(1) {
      req = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (req == "")
        break;	// no more headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1)
        break;

      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 2);
	  
#ifdef DEBUG_OUTPUT
      DEBUG_OUTPUT.print("headerName: ");
      DEBUG_OUTPUT.println(headerName);
      DEBUG_OUTPUT.print("headerValue: ");
      DEBUG_OUTPUT.println(headerValue);
#endif
	  
      if (headerName == "Content-Length"){
        contentLength = headerValue.toInt();
      } else if (headerName == "Host"){
        _hostHeader = headerValue;
      }
    }

    if (searchStr != "")
      searchStr += '&';

    // some clients send headers first and data after (like we do), so give them a chance
    int tries = 100;//100ms max wait
    while (!client.available() && tries--) delay(1);
    size_t plainLen = client.available();
    char *plainBuf = (char*)malloc(plainLen+1);
    if (this->plainBuf)
      free(this->plainBuf);
    this->plainBuf = plainBuf;	// FIXME Danny
    this->plainLen = plainLen;	// FIXME Danny
    client.readBytes(plainBuf, plainLen);
    plainBuf[plainLen] = '\0';
#ifdef DEBUG_OUTPUT
    DEBUG_OUTPUT.print("Plain: ");
    DEBUG_OUTPUT.println(plainBuf);
#endif

#if 1
    searchStr += plainBuf;
#else
    if (plainBuf[0] == '{' || plainBuf[0] == '[' || strstr(plainBuf, "=") == NULL) {
      // plain post json or other data
      searchStr += "plain=";
      searchStr += plainBuf;
    } else {
      searchStr += plainBuf;
    }
#endif
    /* End HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE */
  } else if (method == HTTP_SUBSCRIBE) {
    String headerName;
    String headerValue;
 
    while(1) {
      req = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (req == "")
        break;	// no more headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1)
        break;

      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 2);

      for (int i=UPNP_METHOD_NONE; i<UPNP_END_METHODS; i++)
        if (headerName.equalsIgnoreCase(upnp_header_strings[i])) {
	  int len = headerValue.length();
	  upnp_headers[i] = (char *)malloc(len+1);
	  strcpy(upnp_headers[i], headerValue.c_str());
#ifdef DEBUG_OUTPUT
	  DEBUG_OUTPUT.printf("HEADER [%s] {%s}\n", upnp_header_strings[i], upnp_headers[i]);
#endif
	  break;
	}

      if (headerName == "Host"){
        _hostHeader = headerValue;
      }
    }
    /* HTTP_SUBSCRIBE */
  } else {
    /* HTTP_GET, HTTP_OPTIONS */
    String headerName;
    String headerValue;

    // Parse headers
    while(1) {
      req = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (req == "")
        break;	// no more headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1)
        break;

      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 2);
	  
#ifdef DEBUG_OUTPUT
      DEBUG_OUTPUT.println("==========");
      DEBUG_OUTPUT.print("headerName: ");
      DEBUG_OUTPUT.println(headerName);
      DEBUG_OUTPUT.print("headerValue: ");
      DEBUG_OUTPUT.println(headerValue);
#endif
	  
	  if (headerName == "Host"){
        _hostHeader = headerValue;
      }
    }
  }
  client.flush();

#ifdef DEBUG_OUTPUT
  DEBUG_OUTPUT.print("Request: ");
  DEBUG_OUTPUT.println(url);
  DEBUG_OUTPUT.print(" Arguments: ");
  DEBUG_OUTPUT.println(searchStr);
#endif

  return true;
}
