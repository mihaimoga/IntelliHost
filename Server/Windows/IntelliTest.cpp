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

#include "stdafx.h"
#include "IntelliHostExt.h"

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
	int retVal = 0;

	/* std::cout << "Starting application with " << argc << " arguments:" << std::endl;
	for (int i = 0; i < argc; ++i) {
		std::cout << "Argument " << (i + 1) << ": " << argv[i] << std::endl;
	} */

	WSADATA wsaData{};
	if ((retVal = WSAStartup(MAKEWORD(2, 2), &wsaData)) == 0)
	{
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			_tprintf(_T("Could not find a usable version of Winsock.dll\n"));
			retVal = -1;
		}
		else
		{
			StartProcessing();
			WSACleanup();
		}

	}

	return retVal;
}
