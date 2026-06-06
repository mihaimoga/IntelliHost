/* Copyright (C) 2026 Stefan-Mihai MOGA
This file is part of IntelliHost application developed by Stefan-Mihai MOGA.
IntelliHost is a simpler, flexible, lower-cost SharePoint alternative.

IntelliHost is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliHost is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliHost. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#pragma once

#ifndef STRICT
#define STRICT
#endif

#ifndef WINVER
#define WINVER 0x0601
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0601
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define VC_EXTRALEAN //Exclude rarely-used stuff from Windows headers
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS //some CString constructors will be explicit
#define _AFX_ALL_WARNINGS //turns off MFC's hiding of some common and often safely ignored warning messages
#define _ATL_NO_AUTOMATIC_NAMESPACE

#ifndef _SECURE_ATL
#define _SECURE_ATL 1 //Use the Secure C Runtime in ATL
#endif

#include <afxwin.h>
#include <afxext.h>
#include <atlsocket.h>

#include <iostream>
#include <filesystem>
#include <stdio.h>
#include <string>

#define CWSOCKET_MFC_EXTENSIONS
#define INTELLIHOST_SERVER _T("127.0.0.1:8081")

// Protocol control characters for communication handshake
#define STX 0x02  // Start of Text - marks the beginning of a data packet
#define ETX 0x03  // End of Text - marks the end of a data packet
#define EOT 0x04  // End of Transmission - signals end of communication
#define ENQ 0x05  // Enquiry - requests acknowledgment from receiver
#define ACK 0x06  // Acknowledgment - confirms successful receipt
#define NAK 0x15  // Negative Acknowledgment - indicates transmission error
