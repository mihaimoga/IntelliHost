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
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // keep windows.h from pulling in winsock.h (SocketSTL.h uses winsock2.h)
#endif
#ifndef NOMINMAX
#define NOMINMAX // keep windows.h from defining min/max macros
#endif
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

#define STX 0x02  // Start of Text - marks the beginning of a data packet
#define ETX 0x03  // End of Text - marks the end of a data packet
#define EOT 0x04  // End of Transmission - signals end of communication
#define ENQ 0x05  // Enquiry - requests acknowledgment from receiver
#define ACK 0x06  // Acknowledgment - confirms successful receipt
#define NAK 0x15  // Negative Acknowledgment - indicates transmission error

#ifndef _WIN32
#define ZeroMemory(p, n) memset((p), 0, (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
#endif

namespace {
	/// @brief Application version numbers.
	enum Version
	{
		MAJOR_VERSION = 0,
		MINOR_VERSION = 0,
		REVISION = 1
	};

	/// @brief Characters accepted in a numeric port.
	constexpr std::string_view strDigits = "0123456789";
	/// @brief Buffer sizes used when querying user and host names.
	enum BufferSize
	{
		nNameBufferSize = 0x100,
		nPasswdBufferSize = 0x400
	};

	/// @brief Argument count and index constants for command line parsing.
	enum ArgumentCount
	{
		nMinArgCount = 2,
		nFirstArgIndex = 1
	};

	/// @brief A server address split into host and port.
	struct Endpoint
	{
		std::string _strHost;
		std::string _strPort;
	};

	/// @brief Values parsed from the command line.
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
	void PrintApplicationUsage(const char* pszProgram)
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
	bool IsCommand(const std::string_view strCommand)
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
	bool ParseCommandLine(const int nArgc, char* ppszArgv[], CommandLine& cmdLine, std::string& strError)
	{
		// At least the program name and one action are expected
		if (nArgc < nMinArgCount)
		{
			strError = "an action is required";
			return false;
		}

		for (int nIndex = nFirstArgIndex; nIndex < nArgc; ++nIndex)
		{
			const std::string_view strArgument{ppszArgv[nIndex]};
			if (IsCommand(strArgument))
			{
				// Only one action is allowed per invocation
				if (!cmdLine._strCommand.empty())
				{
					strError = "only one action may be specified";
					return false;
				}
				cmdLine._strCommand = strArgument;
				continue;
			}

			// Map the option to the field that will receive its value
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

			// Advance to the value; it must exist, be non-empty and not be another option
			if (++nIndex == nArgc || ppszArgv[nIndex][0] == '\0' || std::string_view(ppszArgv[nIndex]).starts_with("--"))
			{
				strError = "parameter '" + std::string(strArgument) + "' requires a value";
				return false;
			}
			// Reject duplicated options
			if (!pstrDestination->empty())
			{
				strError = "parameter '" + std::string(strArgument) + "' may only be specified once";
				return false;
			}
			*pstrDestination = ppszArgv[nIndex];
		}

		// Fall back to the TOKEN_ID environment variable when --token is absent
		if (cmdLine._strToken.empty())
		{
			if (const char* pszToken = std::getenv("TOKEN_ID"); pszToken != nullptr)
			{
				cmdLine._strToken = pszToken;
			}
		}

		// Validate required parameters; --generate-token needs none of them
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
	bool LoadMachineFromConfiguration(CommandLine& cmdLine, std::string& strError)
	{
		// The configuration file is read from the current working directory
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

		// Both elements must exist and have text content
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
	bool ParseEndpoint(std::string_view strValue, Endpoint& endpointParsed)
	{
		if (strValue.empty())
		{
			return false;
		}

		// Use the last ':' so hosts containing colons (e.g. IPv6) keep their prefix;
		// both host and port must be non-empty
		const auto nSeparator = strValue.rfind(':');
		if (nSeparator == std::string_view::npos || nSeparator == 0 || nSeparator == strValue.size() - 1)
		{
			return false;
		}

		endpointParsed._strHost = strValue.substr(0, nSeparator);
		endpointParsed._strPort = strValue.substr(nSeparator + 1);
		// The port is valid only if it contains digits exclusively
		return endpointParsed._strPort.find_first_not_of(strDigits) == std::string::npos;
	}

	/// @brief Retrieves the name of the user running the current process.
	///
	/// On Windows the user principal name is preferred, falling back to the plain
	/// account name. On Linux the effective user is looked up in the password
	/// database, falling back to the USER environment variable.
	///
	/// @return The user name encoded as UTF-8, or an empty string on failure.
	std::string GetUserID()
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
	std::string GetMachineID()
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
	std::string GetDateTime()
	{
		char szDateTime[32] = { 0, };
#ifdef _WIN32
		// GetSystemTime returns UTC
		SYSTEMTIME stNow;
		GetSystemTime(&stNow);
		const int nLength = snprintf(szDateTime, sizeof(szDateTime), "%04u-%02u-%02uT%02u:%02u:%02uZ",
			stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour, stNow.wMinute, stNow.wSecond);
		if (nLength <= 0)
		{
			return std::string();
		}
#else
		// Convert the current time to UTC using the thread-safe gmtime_r
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

	/// @brief Size in bytes of the buffers used to exchange framed packets.
	const int MAX_BUFFER = 0x10000;
	/// @brief Computes the Longitudinal Redundancy Check (XOR of all bytes).
	///
	/// @param buffer The data to checksum.
	/// @param length The number of bytes in buffer.
	/// @return The XOR of every byte in buffer.
	inline unsigned char calcLRC(const unsigned char* buffer, const int length)
	{
		// XOR every byte into the running checksum
		unsigned char nLRC = 0;
		for (int i = 0; i < length; nLRC = nLRC ^ buffer[i], i++);
		return nLRC;
	}

	/// @brief Receives one framed packet using the ENQ/ACK/NAK/EOT handshake.
	///
	/// Packet layout: STX, 2-byte length, payload, ETX, LRC. The packet is acknowledged
	/// with ACK when the LRC matches, otherwise NAK is sent and up to 3 attempts are made.
	///
	/// @param pApplicationSocket The connected socket to read from.
	/// @param pBuffer Receives the packet; must hold at least MAX_BUFFER bytes.
	/// @param[in,out] nLength Receives the number of bytes read.
	/// @param ReceiveENQ true to wait for ENQ first and answer it with ACK.
	/// @param ReceiveEOT true to wait for EOT after the packet.
	/// @return true if a valid packet was received and acknowledged, false otherwise.
	bool ReadBuffer(CSocketSTL& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT)
	{
		// Verdict sent back to the peer; stays ACK unless a packet fails the LRC check
		char nReturn = ACK;

		try
		{
			// nIndex: bytes in the last read, nCount: number of attempts made
			int nIndex = 0;
			int nCount = 0;
			// Handshake: wait for the sender's ENQ (its last byte) and answer with ACK
			if (ReceiveENQ)
			{
				nLength = MAX_BUFFER;
				if (pApplicationSocket.IsReadible(1000) &&
					((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
					(ENQ == pBuffer[nLength - 1]))
				{
					// TRACE(_T("ENQ Received\n"));
					unsigned char chACK = ACK;
					const int nSize = pApplicationSocket.Send(&chACK, sizeof(chACK));
					if (nSize != 1)
						return false;
					// TRACE(_T("ACK Sent\n"));
				}
				else
					return false;
			}
			// Read the packet; the last byte is the LRC, checked over the payload that follows the 3-byte header
			nLength = 0;
			do {
				// Keep reading while data arrives within 1 second; the LRC covers the payload only,
				// i.e. everything between the 3-byte header and the ETX/LRC trailer
				while (pApplicationSocket.IsReadible(1000) &&
					((nIndex = pApplicationSocket.Receive(pBuffer + nIndex, MAX_BUFFER - nLength)) > 0))
				{
					nLength += nIndex;
					// TRACE(_T("Buffer Received %s\n"), dumpHEX(pBuffer, nLength).c_str());
					nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
				}
				// Report the verdict; on NAK the sender retransmits and the loop reads again (max 3 attempts)
				const int nSize = pApplicationSocket.Send(&nReturn, sizeof(nReturn));
				if (nSize != 1)
					return false;
				// TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			} while ((ACK != nReturn) && (++nCount < 3));
			// Optional closing handshake: the sender must finish with a single EOT byte
			if (ReceiveEOT)
			{
				unsigned char chEOT = ACK;
				if (pApplicationSocket.IsReadible(1000) &&
					((nLength = pApplicationSocket.Receive(&chEOT, sizeof(chEOT))) > 0) &&
					(EOT == chEOT))
				{
					// TRACE(_T("EOT Received\n"));
				}
				else
					return false;
			}
		}
		// Socket failures are reported to the caller as a plain false
		catch (CSocketException& ex)
		{
			std::string strError;
			ex.GetErrorMessage(strError);
			std::cerr << "ReadBuffer error: " << strError << std::endl;
			return false;
		}
		// TRACE(_T("ReadBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
		return (ACK == nReturn);
	}

	/// @brief Sends one framed packet using the ENQ/ACK/NAK/EOT handshake.
	///
	/// Wraps the payload as STX, 2-byte length, payload, ETX, LRC and resends it until
	/// the peer answers ACK, up to 3 retries.
	///
	/// @param pApplicationSocket The connected socket to write to.
	/// @param pBuffer The payload to send.
	/// @param nLength The payload size in bytes; must not exceed MAX_BUFFER - 5.
	/// @param SendENQ true to send ENQ first and wait for ACK.
	/// @param SendEOT true to send EOT after the packet.
	/// @return true if the peer acknowledged the packet; false on NAK after all retries,
	/// a missing ACK, a short send/receive or a socket error.
	bool WriteBuffer(CSocketSTL& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
	{
		// Last answer from the peer (ACK or NAK) and the outgoing packet buffer
		unsigned char nReturn = ACK;
		unsigned char pPacket[MAX_BUFFER] = { 0, };

		try
		{
			// nCount: retransmissions so far
			int nCount = 0;
			// ASSERT(nLength <= (sizeof(pPacket) - 5));
			// Handshake: announce the packet with ENQ and wait for the receiver's ACK
			if (SendENQ && pApplicationSocket.IsWritable(1000))
			{
				unsigned char chENQ = ENQ;
				// TRACE(_T("ENQ Sent\n"));
				const int nSize = pApplicationSocket.Send(&chENQ, sizeof(chENQ));
				if (nSize != 1)
					return false;
				ZeroMemory(pPacket, sizeof(pPacket));
				nCount = MAX_BUFFER;
				if (((nCount = pApplicationSocket.Receive(pPacket, nCount)) > 0) &&
					(ACK == pPacket[nCount - 1]))
				{
					// TRACE(_T("ACK Received\n"));
					nCount = 0;
				}
				else
					return false;
			}
			// Build the packet: STX, length (big-endian), payload, ETX, LRC
			ZeroMemory(pPacket, sizeof(pPacket));
			CopyMemory(&pPacket[3], pBuffer, nLength);
			pPacket[0] = STX;
			pPacket[1] = static_cast<unsigned char>(nLength / 0x100);
			pPacket[2] = static_cast<unsigned char>(nLength % 0x100);
			pPacket[3 + nLength] = ETX;
			pPacket[4 + nLength] = calcLRC(pBuffer, nLength);
			// Retransmit until the peer answers ACK
			do {
				if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
				{
					// TRACE(_T("Buffer Sent %s\n"), dumpHEX(pPacket, (5 + nLength)).c_str());
					const int nSize = pApplicationSocket.Receive(&nReturn, sizeof(nReturn));
					if (nSize != 1)
						return false;
					// TRACE(_T("%s Received\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
				}
			} while ((nReturn != ACK) && (++nCount <= 3));
			// Optional closing handshake: tell the receiver the transmission is over
			if (SendEOT && pApplicationSocket.IsWritable(1000))
			{
				unsigned char chEOT = EOT;
				// TRACE(_T("EOT Sent\n"));
				const int nSize = pApplicationSocket.Send(&chEOT, sizeof(chEOT));
				if (nSize != 1)
					return false;
			}
		}
		// Socket failures are reported to the caller as a plain false
		catch (CSocketException& ex)
		{
			std::string strError;
			ex.GetErrorMessage(strError);
			std::cerr << "WriteBuffer error: " << strError << std::endl;
			return false;
		}
		// TRACE(_T("WriteBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
		return (ACK == nReturn);
	}
} // namespace

/// @brief Program entry point.
///
/// Handles --help and --version, parses the command line, resolves the server
/// endpoint (from --machine or IntelliHost.xml), generates a token on request
/// (--generate-token) and requests the token from the server: after the
/// "IntelliHost" protocol handshake it sends the "GenerateToken" command with the
/// user ID, machine ID and UTC time, then prints the TokenID the server returns.
///
/// @param nArgc The number of arguments.
/// @param ppszArgv The argument vector.
/// @return 0 on success, -1 on error.
int main(const int nArgc, char* ppszArgv[])
{
	// --help is only valid as the sole argument
	if (nArgc == nMinArgCount && std::string_view(ppszArgv[1]) == "--help")
	{
		PrintApplicationUsage(ppszArgv[0]);
		return EXIT_SUCCESS;
	}

	// --version is only valid as the sole argument
	if (nArgc == nMinArgCount && std::string_view(ppszArgv[1]) == "--version")
	{
		std::cout << ppszArgv[0] << "v" << MAJOR_VERSION << "." << MINOR_VERSION << "." << REVISION << std::endl;
		return EXIT_SUCCESS;
	}

	// Parse and validate the command line
	CommandLine cmdLine;
	std::string strError;
	if (!ParseCommandLine(nArgc, ppszArgv, cmdLine, strError))
	{
		std::cerr << "error: " << strError << std::endl;
		PrintApplicationUsage(ppszArgv[0]);
		return EXIT_FAILURE;
	}

	// Use the server from IntelliHost.xml when --machine was not given
	if (cmdLine._strMachine.empty() && !LoadMachineFromConfiguration(cmdLine, strError))
	{
		std::cerr << "error: " << strError << std::endl;
		return EXIT_FAILURE;
	}

	// Validate the server address
	Endpoint endpointMachine;
	if (!ParseEndpoint(cmdLine._strMachine, endpointMachine))
	{
		std::cerr << "error: machine must be in HOST:PORT form" << std::endl;
		return EXIT_FAILURE;
	}

	// Token generation: send "user, machine, UTC time" to the server, which returns the token
	if (nArgc == nMinArgCount && std::string_view(ppszArgv[1]) == "--generate-token")
	{
		const std::string& strUserID = GetUserID();
		std::cout << "UserID: " << strUserID << std::endl;
		const std::string& strMachineID = GetMachineID();
		std::cout << "MachineID: " << strMachineID << std::endl;
		const std::string& strDateTime = GetDateTime();
		std::cout << "DateTime: " << strDateTime << std::endl;

		if (!strUserID.empty() && !strMachineID.empty() && !strDateTime.empty())
		{
			// Open a new TCP connection to the server and run the token request; each
			// step is acknowledged by the server, and the sequence stops at the first failure
			try
			{
				CSocketSTL objSocket;
				objSocket.CreateAndConnect(endpointMachine._strHost, static_cast<UINT>(std::stoul(endpointMachine._strPort)), SOCK_STREAM);
				std::cout << "Connected to " << endpointMachine._strHost << ":" << endpointMachine._strPort << "..." << std::endl;

				// The terminating null is sent too, so the server can read it as a C string
				const std::string strProtocol = "IntelliHost";
				int nLength = static_cast<int>(strProtocol.length()) + 1;
				if (WriteBuffer(objSocket, reinterpret_cast<const unsigned char *>(strProtocol.c_str()), nLength, true, false))
				{
					// Reply buffer, reused for every packet read from the server
					unsigned char pBuffer[MAX_BUFFER] = { 0, };
					nLength = sizeof(pBuffer);
					ZeroMemory(pBuffer, sizeof(pBuffer));
					if (ReadBuffer(objSocket, pBuffer, nLength, true, false))
					{
						// The payload starts after the STX and 2-byte length header
						// The server answers "OK" to a known protocol, otherwise an error message
						const std::string strProtocolStatus = reinterpret_cast<char *>(&pBuffer[3]);
						if (strProtocolStatus == "OK")
						{
							std::cout << "Protocol acknowledged!" << std::endl;

							// Ask the server to generate a token
							const std::string strCommand = "GenerateToken";
							nLength = static_cast<int>(strCommand.length()) + 1;
							// Send the command, then read the server's verdict on it
							if (WriteBuffer(objSocket, reinterpret_cast<const unsigned char *>(strCommand.c_str()), nLength, true, false))
							{
								nLength = sizeof(pBuffer);
								ZeroMemory(pBuffer, sizeof(pBuffer));
								if (ReadBuffer(objSocket, pBuffer, nLength, true, false))
								{
									// "OK" means the command is accepted and the server expects the token components next
									const std::string strCommandStatus = reinterpret_cast<char *>(&pBuffer[3]);
									if (strCommandStatus == "OK")
									{
										std::cout << "Command sent!" << std::endl;

										// Send the token components, one packet each
										nLength = static_cast<int>(strUserID.length()) + 1;
										if (WriteBuffer(objSocket, reinterpret_cast<const unsigned char *>(strUserID.c_str()), nLength, true, false))
										{
											nLength = static_cast<int>(strMachineID.length()) + 1;
											if (WriteBuffer(objSocket, reinterpret_cast<const unsigned char *>(strMachineID.c_str()), nLength, true, false))
											{
												nLength = static_cast<int>(strDateTime.length()) + 1;
												if (WriteBuffer(objSocket, reinterpret_cast<const unsigned char *>(strDateTime.c_str()), nLength, true, false))
												{
													// Read the TokenID packet; the server closes the exchange with EOT
													nLength = sizeof(pBuffer);
													ZeroMemory(pBuffer, sizeof(pBuffer));
													if (ReadBuffer(objSocket, pBuffer, nLength, true, true))
													{
														// The server answers with the computed token
														const std::string strTokenID = reinterpret_cast<char *>(&pBuffer[3]);
														std::cout << "TokenID: " << strTokenID << std::endl;
													}
												}
											}
										}
									}
									// Anything but "OK" is the server's error text (e.g. unknown command): show it and fail
									else
									{
										objSocket.Close();
										std::cerr << strCommandStatus << std::endl;
										return EXIT_FAILURE;
									}
								}
							}
						}
						// The server rejected the protocol identifier: show its error text and fail
						else
						{
							objSocket.Close();
							std::cerr << strProtocolStatus << std::endl;
							return EXIT_FAILURE;
						}
					}
				}
				// Normal end of the exchange (also reached when a step failed without raising an exception)
				objSocket.Close();
			}
			// Socket failures (resolution, refused connection, ...) carry a system error message
			catch (const CSocketException& ex)
			{
				// Reuse the strError declared earlier in main
				ex.GetErrorMessage(strError);
				std::cerr << "Client Error: " << strError << std::endl;
				return EXIT_FAILURE;
			}
			// std::stoul throws when the port is not numeric or out of range
			catch (const std::exception&)
			{
				std::cerr << "error: invalid port in " << cmdLine._strMachine << std::endl;
				return EXIT_FAILURE;
			}
		}
		return EXIT_SUCCESS;
	}
	return EXIT_SUCCESS;
}
