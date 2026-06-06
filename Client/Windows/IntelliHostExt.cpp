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

#define SECURITY_WIN32
#include "Security.h"
#pragma comment(lib, "Secur32.lib")

std::wstring utf8_to_wstring(const std::string& string)
{
	if (string.empty())
	{
		return L"";
	}

	// Calculate the required buffer size
	const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), nullptr, 0);
	if (size_needed <= 0)
	{
		throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
	}

	// Perform the conversion
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), result.data(), size_needed);
	return result;
}

std::string wstring_to_utf8(const std::wstring& wide_string)
{
	if (wide_string.empty())
	{
		return "";
	}

	// Calculate the required buffer size
	const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0)
	{
		throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
	}

	// Perform the conversion
	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), result.data(), size_needed, nullptr, nullptr);
	return result;
}

const std::wstring GetUserID()
{
	DWORD nLength = 0x100;
	TCHAR lpszUserName[0x100] = { 0, };
	if (GetUserNameEx(NameUserPrincipal, lpszUserName, &nLength))
	{
		lpszUserName[nLength] = 0;
		TRACE(_T("UserName = %s\n"), lpszUserName);
	}
	else
	{
		nLength = 0x100;
		if (GetUserName(lpszUserName, &nLength) != 0)
		{
			lpszUserName[nLength] = 0;
			TRACE(_T("UserName = %s\n"), lpszUserName);
		}
	}
	return lpszUserName;
}

const std::wstring GetMachineID()
{
	DWORD nLength = 0x100;
	TCHAR lpszComputerName[0x100] = { 0, };
	if (GetComputerNameEx(ComputerNamePhysicalDnsFullyQualified, lpszComputerName, &nLength))
	{
		lpszComputerName[nLength] = 0;
		TRACE(_T("ComputerName = %s\n"), lpszComputerName);
	}
	else
	{
		nLength = 0x100;
		if (GetComputerName(lpszComputerName, &nLength) != 0)
		{
			lpszComputerName[nLength] = 0;
			TRACE(_T("ComputerName = %s\n"), lpszComputerName);
		}
	}
	return lpszComputerName;
}

const int MAX_BUFFER = 0x10000;

// Hexadecimal conversion helpers
const char HEX_MAP[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

/**
 * @brief Maps a nibble to its hexadecimal character
 * @param c The byte value to convert (only lower 4 bits are used)
 * @return The hexadecimal character ('0'-'9', 'A'-'F')
 */
char replace(unsigned char c)
{
	return HEX_MAP[c & 0x0f];
}

/**
 * @brief Converts a byte to a two-character hexadecimal string
 * @param c The byte value to convert
 * @return Two-character hexadecimal string representation
 */
std::string char_to_hex(unsigned char c)
{
	std::string hex;
	char left = (c >> 4);
	char right = (c & 0x0f);
	hex += replace(left);
	hex += replace(right);
	return hex;
}

/**
 * @brief Dumps a buffer as a space-separated hexadecimal string
 * @param pBuffer Pointer to the buffer to dump
 * @param nLength Length of the buffer in bytes
 * @return Wide string containing space-separated hexadecimal values
 */
std::wstring dumpHEX(unsigned char* pBuffer, const int nLength)
{
	std::string result;
	for (int nIndex = 0; nIndex < nLength; nIndex++)
	{
		result += char_to_hex(pBuffer[nIndex]);
		result += " ";
	}
	return utf8_to_wstring(result);
}

/**
 * @brief Reads a buffer from the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK)
 * @param pApplicationSocket The socket to read from
 * @param pBuffer Buffer to store received data
 * @param nLength [in/out] Length of buffer and received data
 * @param ReceiveENQ Whether to expect an ENQ handshake
 * @param ReceiveEOT Whether to expect an EOT handshake
 * @return true on successful read and protocol validation, false otherwise
 */
bool ReadBuffer(CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT)
{
	int nIndex = 0;
	int nCount = 0;
	char nReturn = ACK;

	try
	{
		if (ReceiveENQ)
		{
			nLength = MAX_BUFFER;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
				(ENQ == pBuffer[nLength - 1]))
			{
				TRACE(_T("ENQ Received\n"));
				unsigned char chACK = ACK;
				VERIFY(pApplicationSocket.Send(&chACK, sizeof(chACK)) == 1);
				TRACE(_T("ACK Sent\n"));
			}
			else
				return false;
		}
		nLength = 0;
		do {
			while (pApplicationSocket.IsReadible(1000) &&
				((nIndex = pApplicationSocket.Receive(pBuffer + nIndex, MAX_BUFFER - nLength)) > 0))
			{
				nLength += nIndex;
				TRACE(_T("Buffer Received %s\n"), dumpHEX(pBuffer, nLength).c_str());
				nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
			}
			VERIFY(pApplicationSocket.Send(&nReturn, sizeof(nReturn)) == 1);
			TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
		} while ((ACK != nReturn) && (++nCount < 3));
		if (ReceiveEOT)
		{
			unsigned char chEOT = ACK;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(&chEOT, sizeof(chEOT))) > 0) &&
				(EOT == chEOT))
			{
				TRACE(_T("EOT Received\n"));
			}
			else
				return false;
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		_tprintf(lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		return false;
	}
	TRACE(_T("ReadBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

/**
 * @brief Writes a buffer to the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK)
 * @param pApplicationSocket The socket to write to
 * @param pBuffer Buffer containing data to send
 * @param nLength Length of data to send
 * @param SendENQ Whether to send an ENQ handshake
 * @param SendEOT Whether to send an EOT handshake
 * @return true on successful write and protocol validation, false otherwise
 */
#pragma warning(suppress: 6262)
bool WriteBuffer(CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
{
	int nCount = 0;
	unsigned char nReturn = ACK;
	unsigned char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength <= (sizeof(pPacket) - 5));
		if (SendENQ && pApplicationSocket.IsWritable(1000))
		{
			unsigned char chENQ = ENQ;
			TRACE(_T("ENQ Sent\n"));
			VERIFY(pApplicationSocket.Send(&chENQ, sizeof(chENQ)) == 1);
			ZeroMemory(pPacket, sizeof(pPacket));
			nCount = MAX_BUFFER;
			if (((nCount = pApplicationSocket.Receive(pPacket, nCount)) > 0) &&
				(ACK == pPacket[nCount - 1]))
			{
				TRACE(_T("ACK Received\n"));
				nCount = 0;
			}
			else
				return false;
		}
		ZeroMemory(pPacket, sizeof(pPacket));
		CopyMemory(&pPacket[3], pBuffer, nLength);
		pPacket[0] = STX;
		pPacket[1] = (char)(nLength / 0x100);
		pPacket[2] = nLength % 0x100;
		pPacket[3 + nLength] = ETX;
		pPacket[4 + nLength] = calcLRC(pBuffer, nLength);
		do {
			if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
			{
				TRACE(_T("Buffer Sent %s\n"), dumpHEX(pPacket, (5 + nLength)).c_str());
				VERIFY(pApplicationSocket.Receive(&nReturn, sizeof(nReturn)) == 1);
				TRACE(_T("%s Received\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			}
		} while ((nReturn != ACK) && (++nCount <= 3));
		if (SendEOT && pApplicationSocket.IsWritable(1000))
		{
			unsigned char chEOT = EOT;
			TRACE(_T("EOT Sent\n"));
			VERIFY(pApplicationSocket.Send(&chEOT, sizeof(chEOT)) == 1);
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		_tprintf(lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		return false;
	}
	TRACE(_T("WriteBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

bool DownloadFile(const std::wstring& strSourceFile, const std::wstring& strDestinationPath, const std::wstring& strToken, const std::wstring& strMachineID)
{
	CWSocket pApplicationSocket;

	try
	{
		const size_t position = strMachineID.find(':');
		if (std::string::npos == position) return false;
		const std::wstring strHostname = strMachineID.substr(0, position);
		const std::wstring strPort = strMachineID.substr(position + 1);

		pApplicationSocket.CreateAndConnect(strHostname.c_str(), strPort.c_str());
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		_tprintf(lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		return false;
	}

	pApplicationSocket.Close();
	return true;
}

bool UploadFile(const std::wstring& strSourceFile, const std::wstring& strDestinationPath, const std::wstring& strToken, const std::wstring& strMachineID)
{
	return true;
}

bool DeleteFile(const std::wstring& strSourceFile, const std::wstring& strDestinationPath, const std::wstring& strToken, const std::wstring& strMachineID)
{
	return true;
}
