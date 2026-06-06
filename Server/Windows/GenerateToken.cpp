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
#include "SHA256.h"

int main()
{
	int retVal = 0;

	std::wstring strUserID = GetUserID();
	_tprintf(_T("User: %s\n"), strUserID.c_str());

	std::wstring strMachineID = GetMachineID();
	_tprintf(_T("Machine: %s\n"), strMachineID.c_str());

	std::wstring strDateTime = GetDateTime();
	_tprintf(_T("DateTime: %s\n"), strDateTime.c_str());

	std::string strRawData = wstring_to_utf8(strUserID + _T(" : ") + strMachineID + _T(" : ") + strDateTime);
	SHA256 sha256;
	sha256.update(strRawData);
	std::wstring strToken = utf8_to_wstring(SHA256::toString(sha256.digest()));
	_tprintf(_T("Token: %s\n"), strToken.c_str());

	return retVal;
}
