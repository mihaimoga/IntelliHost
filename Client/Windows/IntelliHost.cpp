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
	std::wstring strToken;
	std::wstring strMachineID = INTELLIHOST_SERVER;

	/* std::cout << "Starting application with " << argc << " arguments:" << std::endl;
	for (int i = 0; i < argc; ++i) {
		std::cout << "Argument " << (i + 1) << ": " << argv[i] << std::endl;
	} */

	bool bShowHelp = false;
	if (argc != 8)
	{
		_tprintf(_T("ERROR: Invalid number of arguments!\n"));
		bShowHelp = true;
	}
	else
	{
		if ((_stricmp(argv[1], "download") != 0) &&
			(_stricmp(argv[1], "upload") != 0) &&
			(_stricmp(argv[1], "delete") != 0))
		{
			_tprintf(_T("ERROR: Invalid command (shold be either Download, Upload, or Delete)!\n"));
			bShowHelp = true;
		}
		else
		{
			strToken = utf8_to_wstring(argv[5]);
			strMachineID = utf8_to_wstring(argv[7]);
		}
	}

	if (bShowHelp)
	{
		_tprintf(_T("Usage: IntelliHost.exe Download|Upload|Delete SOURCE-FILE DESTINATION-FOLDER --token TOKEN --machine IPv4:PORT\n"));
		retVal = -1;
	}
	else
	{
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
				std::filesystem::path fullpath = std::filesystem::absolute(argv[2]);
				if (_stricmp(argv[1], "download") == 0)
				{
					_tprintf(_T("Downloading %s from %s...\n"), fullpath.c_str(), utf8_to_wstring(argv[3]).c_str());
					_tprintf(_T("Token: %s, Machine: %s\n"), strToken.c_str(), strMachineID.c_str());
					retVal = DownloadFile(fullpath, utf8_to_wstring(argv[3]), strToken, strMachineID) ? 0 : -1;
				}
				else
				{
					if (_stricmp(argv[1], "upload") == 0)
					{
						_tprintf(_T("Uploading %s to %s...\n"), fullpath.c_str(), utf8_to_wstring(argv[3]).c_str());
						_tprintf(_T("Token: %s, Machine: %s\n"), strToken.c_str(), strMachineID.c_str());
						retVal = UploadFile(fullpath, utf8_to_wstring(argv[3]), strToken, strMachineID) ? 0 : -1;
					}
					else
					{
						_tprintf(_T("Deleting %s at %s...\n"), fullpath.c_str(), utf8_to_wstring(argv[3]).c_str());
						_tprintf(_T("Token: %s, Machine: %s\n"), strToken.c_str(), strMachineID.c_str());
						retVal = DeleteFile(fullpath, utf8_to_wstring(argv[3]), strToken, strMachineID) ? 0 : -1;
					}
				}
				WSACleanup();
			}
		}
	}

	return retVal;
}
