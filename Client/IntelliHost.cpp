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

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define SECURITY_WIN32
#include <windows.h>
#include <secext.h>
#pragma comment(lib, "Secur32.lib")
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "SHA256.h"
#include "SocketSTL.h"
#include "tinyxml2.h"

enum Version
{
	MARJOR_VERSION = 0,
	MINOR_VERSION = 0,
	REVISION = 1
};

namespace
{

constexpr std::string_view strDigits = "0123456789";
enum BufferSize
{
	nNameBufferSize = 0x100,
	nPasswdBufferSize = 0x400
};

enum ArgumentCount
{
	nMinArgCount = 2,
	nFirstArgIndex = 1
};

struct Endpoint
{
	std::string _strHost;
	std::string _strPort;
};

struct CommandLine
{
	std::string _strCommand;
	std::string _strInputFile;
	std::string _strOutputFolder;
	std::string _strToken;
	std::string _strMachine;
};

/// @brief Prints the command line usage text to standard error.
///
/// @param pszProgram The program name (typically argv[0]) shown in the usage lines.
void vPrintUsage(const char* pszProgram)
{
	std::cerr << "Usage:\n"
			  << "  " << pszProgram << " --help\n"
			  << "  " << pszProgram << " --version\n"
			  << "  " << pszProgram << " --generate-token\n"
			  << "  " << pszProgram
			  << " <--download|--upload|--delete> --input-file FILE --output-folder FOLDER"
				 " [--token TOKEN] [--machine HOST:PORT]\n"
			  << "  TOKEN_ID environment variable is used when --token is not specified.\n"
			  << "  IntelliHost.xml is used when --machine is not specified."
			  << std::endl;
}

/// @brief Checks whether an argument is one of the supported actions.
///
/// @param strCommand The command line argument to test.
/// @return true if it is --generate-token, --download, --upload or --delete.
bool bIsCommand(const std::string_view strCommand)
{
	return strCommand == "--generate-token" || strCommand == "--download" || strCommand == "--upload" ||
		   strCommand == "--delete";
}

/// @brief Parses and validates the command line arguments.
///
/// Falls back to the TOKEN_ID environment variable when --token is absent, and
/// checks that all parameters required by the chosen action are present.
///
/// @param nArgc The number of arguments.
/// @param ppszArgv The argument vector.
/// @param[out] cmdLine Receives the parsed command line values.
/// @param[out] strError Receives a description of the problem on failure.
/// @return true if the command line is valid, false otherwise.
bool bParseCommandLine(const int nArgc, char* ppszArgv[], CommandLine& cmdLine, std::string& strError)
{
	if (nArgc < nMinArgCount)
	{
		strError = "an action is required";
		return false;
	}

	for (int nIndex = nFirstArgIndex; nIndex < nArgc; ++nIndex)
	{
		const std::string_view strArgument{ppszArgv[nIndex]};
		if (bIsCommand(strArgument))
		{
			if (!cmdLine._strCommand.empty())
			{
				strError = "only one action may be specified";
				return false;
			}
			cmdLine._strCommand = strArgument;
			continue;
		}

		std::string* pstrDestination = nullptr;
		if (strArgument == "--input-file")
		{
			pstrDestination = &cmdLine._strInputFile;
		}
		else if (strArgument == "--output-folder")
		{
			pstrDestination = &cmdLine._strOutputFolder;
		}
		else if (strArgument == "--token")
		{
			pstrDestination = &cmdLine._strToken;
		}
		else if (strArgument == "--machine")
		{
			pstrDestination = &cmdLine._strMachine;
		}
		else
		{
			strError = "unknown parameter '" + std::string(strArgument) + "'";
			return false;
		}

		if (++nIndex == nArgc || ppszArgv[nIndex][0] == '\0' || std::string_view(ppszArgv[nIndex]).starts_with("--"))
		{
			strError = "parameter '" + std::string(strArgument) + "' requires a value";
			return false;
		}
		if (!pstrDestination->empty())
		{
			strError = "parameter '" + std::string(strArgument) + "' may only be specified once";
			return false;
		}
		*pstrDestination = ppszArgv[nIndex];
	}

	if (cmdLine._strToken.empty())
	{
		if (const char* pszToken = std::getenv("TOKEN_ID"); pszToken != nullptr)
		{
			cmdLine._strToken = pszToken;
		}
	}

	if (cmdLine._strCommand.empty())
	{
		strError = "an action is required";
	}
	else if (cmdLine._strInputFile.empty() && (cmdLine._strCommand != "--generate-token"))
	{
		strError = "--input-file is required";
	}
	else if (cmdLine._strOutputFolder.empty() && (cmdLine._strCommand != "--generate-token"))
	{
		strError = "--output-folder is required";
	}
	else if (cmdLine._strToken.empty() && (cmdLine._strCommand != "--generate-token"))
	{
		strError = "--token or TOKEN_ID environment variable is required";
	}

	return strError.empty();
}

/// @brief Loads the server address from IntelliHost.xml.
///
/// Reads the ServerIP and ServerPort elements under the IntelliHost root and
/// stores them as HOST:PORT in the command line's machine field.
///
/// @param[in,out] cmdLine Receives the machine value.
/// @param[out] strError Receives a description of the problem on failure.
/// @return true on success, false if the file is missing or malformed.
bool bLoadMachineFromConfiguration(CommandLine& cmdLine, std::string& strError)
{
	tinyxml2::XMLDocument xmlDocument;
	if (xmlDocument.LoadFile("IntelliHost.xml") != tinyxml2::XML_SUCCESS)
	{
		strError = "unable to load IntelliHost.xml: " + std::string(xmlDocument.ErrorStr());
		return false;
	}

	const tinyxml2::XMLElement* pXmlRoot = xmlDocument.FirstChildElement("IntelliHost");
	if (pXmlRoot == nullptr)
	{
		strError = "IntelliHost.xml is missing the IntelliHost element";
		return false;
	}

	const tinyxml2::XMLElement* pXmlServerIp = pXmlRoot->FirstChildElement("ServerIP");
	const tinyxml2::XMLElement* pXmlServerPort = pXmlRoot->FirstChildElement("ServerPort");
	if (pXmlServerIp == nullptr || pXmlServerIp->GetText() == nullptr || pXmlServerPort == nullptr ||
		pXmlServerPort->GetText() == nullptr)
	{
		strError = "IntelliHost.xml must contain non-empty ServerIP and ServerPort elements";
		return false;
	}

	cmdLine._strMachine = std::string(pXmlServerIp->GetText()) + ':' + pXmlServerPort->GetText();
	return true;
}

/// @brief Splits a HOST:PORT string into its host and port parts.
///
/// @param strValue The text to parse; the last ':' separates host from port.
/// @param[out] endpointParsed Receives the host and port.
/// @return true if both parts are non-empty and the port is numeric.
bool bParseEndpoint(std::string_view strValue, Endpoint& endpointParsed)
{
	if (strValue.empty())
	{
		return false;
	}

	const auto nSeparator = strValue.rfind(':');
	if (nSeparator == std::string_view::npos || nSeparator == 0 || nSeparator == strValue.size() - 1)
	{
		return false;
	}

	endpointParsed._strHost = strValue.substr(0, nSeparator);
	endpointParsed._strPort = strValue.substr(nSeparator + 1);
	return endpointParsed._strPort.find_first_not_of(strDigits) == std::string::npos;
}

/// @brief Retrieves the name of the user running the current process.
///
/// On Windows the user principal name is preferred, falling back to the plain
/// account name. On Linux the effective user is looked up in the password
/// database, falling back to the USER environment variable.
///
/// @return The user name encoded as UTF-8, or an empty string on failure.
std::string strGetUserID()
{
#ifdef _WIN32
	wchar_t szUserName[nNameBufferSize] = { 0, };
	DWORD dwLength{nNameBufferSize};
	// Try the user principal name first (e.g. user@domain)
	if (!GetUserNameExW(NameUserPrincipal, szUserName, &dwLength))
	{
		// Fall back to the plain account name; reset the buffer size first
		dwLength = nNameBufferSize;
		if (!GetUserNameW(szUserName, &dwLength))
		{
			return std::string();
		}
		// GetUserNameW counts the terminating null
		if (dwLength > 0)
		{
			--dwLength;
		}
	}
	if (dwLength == 0)
	{
		return std::string();
	}
	// First call computes the required UTF-8 buffer size
	const int nSize{WideCharToMultiByte(CP_UTF8, 0, szUserName, static_cast<int>(dwLength), nullptr, 0, nullptr, nullptr)};
	if (nSize <= 0)
	{
		return std::string();
	}
	// Second call performs the actual UTF-16 to UTF-8 conversion
	std::string strUserName(static_cast<size_t>(nSize), '\0');
	WideCharToMultiByte(CP_UTF8, 0, szUserName, static_cast<int>(dwLength), strUserName.data(), nSize, nullptr, nullptr);
	return strUserName;
#else
	// Linux locale encodings are normally UTF-8, so the bytes are returned as-is.
	// Size the buffer as recommended by the system, or use a default size
	std::vector<char> vecBuffer(sysconf(_SC_GETPW_R_SIZE_MAX) > 0 ? static_cast<size_t>(sysconf(_SC_GETPW_R_SIZE_MAX)) : static_cast<size_t>(nPasswdBufferSize));
	struct passwd pwdEntry;
	struct passwd* ppwdResult = nullptr;
	if (getpwuid_r(geteuid(), &pwdEntry, vecBuffer.data(), vecBuffer.size(), &ppwdResult) == 0 && ppwdResult != nullptr &&
		ppwdResult->pw_name != nullptr)
	{
		return std::string(ppwdResult->pw_name);
	}
	// Last resort: use the USER environment variable
	const char* pszUser = std::getenv("USER");
	return pszUser != nullptr ? std::string(pszUser) : std::string();
#endif
}

/// @brief Retrieves the name of the machine (host) running the current process.
///
/// On Windows the fully qualified DNS name is preferred, falling back to the
/// NetBIOS computer name. On Linux the kernel host name is used.
///
/// @return The machine name encoded as UTF-8, or an empty string on failure.
std::string strGetMachineID()
{
#ifdef _WIN32
	wchar_t szComputerName[nNameBufferSize] = { 0, };
	DWORD dwLength{nNameBufferSize};
	// Try the fully qualified DNS name first
	if (!GetComputerNameExW(ComputerNamePhysicalDnsFullyQualified, szComputerName, &dwLength))
	{
		// Fall back to the NetBIOS name; reset the buffer size first
		dwLength = nNameBufferSize;
		if (!GetComputerNameW(szComputerName, &dwLength))
		{
			return std::string();
		}
	}
	if (dwLength == 0)
	{
		return std::string();
	}
	// First call computes the required UTF-8 buffer size
	const int nSize{WideCharToMultiByte(CP_UTF8, 0, szComputerName, static_cast<int>(dwLength), nullptr, 0, nullptr, nullptr)};
	if (nSize <= 0)
	{
		return std::string();
	}
	// Second call performs the actual UTF-16 to UTF-8 conversion
	std::string strComputerName(static_cast<size_t>(nSize), '\0');
	WideCharToMultiByte(CP_UTF8, 0, szComputerName, static_cast<int>(dwLength), strComputerName.data(), nSize, nullptr, nullptr);
	return strComputerName;
#else
	// Linux host names are raw bytes, normally UTF-8, so they are returned as-is.
	char szHostName[nNameBufferSize] = { 0, };
	// Reserve the last byte so the name is always null-terminated
	if (gethostname(szHostName, sizeof(szHostName) - 1) != 0)
	{
		return std::string();
	}
	return std::string(szHostName);
#endif
}

/// @brief Retrieves the current UTC date and time.
///
/// @return The timestamp formatted as "YYYY-MM-DDTHH:MM:SSZ", or an empty string on failure.
std::string strGetDateTime()
{
	char szDateTime[32] = { 0, };
#ifdef _WIN32
	SYSTEMTIME stNow;
	GetSystemTime(&stNow);
	const int nLength = snprintf(szDateTime, sizeof(szDateTime), "%04u-%02u-%02uT%02u:%02u:%02uZ",
		stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour, stNow.wMinute, stNow.wSecond);
	if (nLength <= 0)
	{
		return std::string();
	}
#else
	const time_t tmNow = time(nullptr);
	struct tm tmUtc;
	if (tmNow == static_cast<time_t>(-1) || gmtime_r(&tmNow, &tmUtc) == nullptr ||
		strftime(szDateTime, sizeof(szDateTime), "%Y-%m-%dT%H:%M:%SZ", &tmUtc) == 0)
	{
		return std::string();
	}
#endif
	return std::string(szDateTime);
}

} // namespace

/// @brief Program entry point.
///
/// Handles --help and --version, parses the command line, resolves the server
/// endpoint (from --machine or IntelliHost.xml) and prints the user and machine IDs.
///
/// @param nArgc The number of arguments.
/// @param ppszArgv The argument vector.
/// @return 0 on success, -1 on error.
int main(const int nArgc, char* ppszArgv[])
{
	if (nArgc == nMinArgCount && std::string_view(ppszArgv[1]) == "--help")
	{
		vPrintUsage(ppszArgv[0]);
		return 0;
	}

	if (nArgc == nMinArgCount && std::string_view(ppszArgv[1]) == "--version")
	{
		std::cout << ppszArgv[0] << "v" << MARJOR_VERSION << "." << MINOR_VERSION << "." << REVISION << std::endl;
		return 0;
	}

	CommandLine cmdLine;
	std::string strError;
	if (!bParseCommandLine(nArgc, ppszArgv, cmdLine, strError))
	{
		std::cerr << "error: " << strError << std::endl;
		vPrintUsage(ppszArgv[0]);
		return -1;
	}

	if (cmdLine._strMachine.empty() && !bLoadMachineFromConfiguration(cmdLine, strError))
	{
		std::cerr << "error: " << strError << std::endl;
		return -1;
	}

	Endpoint endpointMachine;
	if (!bParseEndpoint(cmdLine._strMachine, endpointMachine))
	{
		std::cerr << "error: machine must be in HOST:PORT form" << std::endl;
		return -1;
	}

	if (nArgc == nMinArgCount && std::string_view(ppszArgv[1]) == "--generate-token")
	{
		const std::string strUserID = strGetUserID();
		std::cout << "UserID: " << strUserID << std::endl;
		const std::string strMachineID = strGetMachineID();
		std::cout << "MachineID: " << strMachineID << std::endl;
		const std::string strDateTime = strGetDateTime();
		std::cout << "DateTime: " << strDateTime << std::endl;

		if (!strUserID.empty() && !strMachineID.empty() && !strDateTime.empty())
		{
			std::string strRawData = strUserID + (" : ") + strMachineID + (" : ") + strDateTime;
			SHA256 sha256;
			sha256.update(strRawData);
			const std::string strToken = SHA256::toString(sha256.digest());
			std::cout << "TokenID: " << strToken << std::endl;
		}
	}

	return 0;
}
