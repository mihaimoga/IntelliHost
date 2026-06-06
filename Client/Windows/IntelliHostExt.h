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

#ifndef __INTELLIHOST_EXT__
#define __INTELLIHOST_EXT__

#include "SocMFC.h"

/**
 * @brief Calculates the Longitudinal Redundancy Check (LRC) for a buffer.
 *        Used for simple data integrity verification in protocol packets.
 * @param buffer Pointer to the buffer.
 * @param length Number of bytes to process.
 * @return The computed LRC value.
 */
inline unsigned char calcLRC(const unsigned char* buffer, const int length)
{
	unsigned char nLRC = 0;
	for (int i = 0; i < length; nLRC = nLRC ^ buffer[i], i++);
	return nLRC;
}

/**
 * @brief Calculates the LRC for a vector of bytes.
 * @param buffer Vector of bytes.
 * @return The computed LRC value.
 */
inline unsigned char calcLRC(const std::vector<unsigned char>& buffer)
{
	unsigned char nLRC = 0;
	for (auto i = buffer.begin(); i != buffer.end(); ++i)
		nLRC = nLRC ^ *i;
	return nLRC;
}

std::wstring utf8_to_wstring(const std::string& str);
std::string wstring_to_utf8(const std::wstring& str);

const std::wstring GetUserID();
const std::wstring GetMachineID();

/**
 * @brief Reads a buffer from the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK).
 * @param pApplicationSocket The socket to read from.
 * @param pBuffer Buffer to store received data.
 * @param nLength [in/out] Length of buffer and received data.
 * @param ReceiveENQ Whether to expect an ENQ handshake.
 * @param ReceiveEOT Whether to expect an EOT handshake.
 * @return true on successful read and protocol validation, false otherwise.
 */
bool ReadBuffer(CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT);

/**
 * @brief Writes a buffer to the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK).
 * @param pApplicationSocket The socket to write to.
 * @param pBuffer Buffer containing data to send.
 * @param nLength Length of data to send.
 * @param SendENQ Whether to send an ENQ handshake.
 * @param SendEOT Whether to send an EOT handshake.
 * @return true on successful write and protocol validation, false otherwise.
 */
bool WriteBuffer(CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT);

bool DownloadFile(const std::wstring& strSourceFile, const std::wstring& strDestinationPath, const std::wstring& strToken, const std::wstring& strMachineID);

bool UploadFile(const std::wstring& strSourceFile, const std::wstring& strDestinationPath, const std::wstring& strToken, const std::wstring& strMachineID);

bool DeleteFile(const std::wstring& strSourceFile, const std::wstring& strDestinationPath, const std::wstring& strToken, const std::wstring& strMachineID);

#endif
