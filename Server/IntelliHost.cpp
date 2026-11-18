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

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // keep windows.h from pulling in winsock.h (SocketSTL.h uses winsock2.h)
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

#include "../Client/SHA256.h"
#include "../Client/SocketSTL.h"
#include "../Client/tinyxml2.h"

#define STX 0x02  // Start of Text - marks the beginning of a data packet
#define ETX 0x03  // End of Text - marks the end of a data packet
#define EOT 0x04  // End of Transmission - signals end of communication
#define ENQ 0x05  // Enquiry - requests acknowledgment from receiver
#define ACK 0x06  // Acknowledgment - confirms successful receipt
#define NAK 0x15  // Negative Acknowledgment - indicates transmission error

#ifndef _WIN32
#define ZeroMemory(p, n) memset((p), 0, (n))
#endif

#ifndef _WIN32
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
#endif

namespace {
    /// @brief Loads the server settings from IntelliHost.xml.
    ///
    /// The file is read from the current working directory. Every element under the
    /// IntelliHost root must exist and be non-empty, and each value is echoed to stdout.
    ///
    /// @param[out] strError Receives a description of the problem on failure.
    /// @param[out] strServerPort Port the server listens on.
    /// @param[out] strHostName Name of the host.
    /// @param[out] strHostPort Port of the host.
    /// @param[out] strDatabase Database name.
    /// @param[out] strUsername Database user name.
    /// @param[out] strPassword Database password.
    /// @return true on success, false if the file is missing or malformed.
    bool LoadMachineFromConfiguration(std::string& strError, std::string& strServerPort, std::string& strHostName, std::string& strHostPort, std::string& strDatabase, std::string& strUsername, std::string& strPassword)
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

        // ServerPort element must exist and have text content
        const tinyxml2::XMLElement* pXmlServerPort = pXmlRoot->FirstChildElement("ServerPort");
        if (pXmlServerPort == nullptr || pXmlServerPort->GetText() == nullptr)
        {
            strError = "IntelliHost.xml must contain non-empty ServerPort element";
            return false;
        }
        strServerPort = std::string(pXmlServerPort->GetText());
        std::cout << "Server port: " << strServerPort << std::endl;

        // HostName element must exist and have text content
        const tinyxml2::XMLElement* pXmlHostName = pXmlRoot->FirstChildElement("HostName");
        if (pXmlHostName == nullptr || pXmlHostName->GetText() == nullptr)
        {
            strError = "IntelliHost.xml must contain non-empty HostName element";
            return false;
        }
        strHostName = std::string(pXmlHostName->GetText());
        std::cout << "HostName: " << strHostName << std::endl;

        // HostPort element must exist and have text content
        const tinyxml2::XMLElement* pXmlHostPort = pXmlRoot->FirstChildElement("HostPort");
        if (pXmlHostPort == nullptr || pXmlHostPort->GetText() == nullptr)
        {
            strError = "IntelliHost.xml must contain non-empty HostPort element";
            return false;
        }
        strHostPort = std::string(pXmlHostPort->GetText());
        std::cout << "HostPort: " << strHostPort << std::endl;

        // ServerPort element must exist and have text content
        const tinyxml2::XMLElement* pXmlDatabase = pXmlRoot->FirstChildElement("Database");
        if (pXmlDatabase == nullptr || pXmlDatabase->GetText() == nullptr)
        {
            strError = "IntelliHost.xml must contain non-empty Database element";
            return false;
        }
        strDatabase = std::string(pXmlDatabase->GetText());
        std::cout << "Database: " << strDatabase << std::endl;

        // Username element must exist and have text content
        const tinyxml2::XMLElement* pXmlUsername = pXmlRoot->FirstChildElement("Username");
        if (pXmlUsername == nullptr || pXmlUsername->GetText() == nullptr)
        {
            strError = "IntelliHost.xml must contain non-empty Username element";
            return false;
        }
        strUsername = std::string(pXmlUsername->GetText());
        std::cout << "Username: " << strUsername << std::endl;

        // Password element must exist and have text content
        const tinyxml2::XMLElement* pXmlPassword = pXmlRoot->FirstChildElement("Password");
        if (pXmlPassword == nullptr || pXmlPassword->GetText() == nullptr)
        {
            strError = "IntelliHost.xml must contain non-empty Password element";
            return false;
        }
        strPassword = std::string(pXmlPassword->GetText());
        std::cout << "Password: " << strPassword << std::endl;

        return true;
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
		char nReturn = ACK;

		try
		{
			// nIndex: bytes in the last read, nCount: number of attempts made
			int nIndex = 0;
			int nCount = 0;
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
					// TRACE(_T("ACK Sent\n"));
				}
				else
					return false;
			}
			// Read the packet; the last byte is the LRC, checked over the payload that follows the 3-byte header
			nLength = 0;
			do {
				while (pApplicationSocket.IsReadible(1000) &&
					((nIndex = pApplicationSocket.Receive(pBuffer + nIndex, MAX_BUFFER - nLength)) > 0))
				{
					nLength += nIndex;
					// TRACE(_T("Buffer Received %s\n"), dumpHEX(pBuffer, nLength).c_str());
					nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
				}
				const int nSize = pApplicationSocket.Send(&nReturn, sizeof(nReturn));
				// TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			} while ((ACK != nReturn) && (++nCount < 3));
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
	/// @return true if the peer acknowledged the packet, false otherwise.
	bool WriteBuffer(CSocketSTL& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
	{
		unsigned char nReturn = ACK;
		unsigned char pPacket[MAX_BUFFER] = { 0, };

		try
		{
			int nCount = 0;
			// ASSERT(nLength <= (sizeof(pPacket) - 5));
			if (SendENQ && pApplicationSocket.IsWritable(1000))
			{
				unsigned char chENQ = ENQ;
				// TRACE(_T("ENQ Sent\n"));
				const int nSize = pApplicationSocket.Send(&chENQ, sizeof(chENQ));
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
			pPacket[1] = static_cast<unsigned char>((char)(nLength / 0x100));
			pPacket[2] = static_cast<unsigned char>((char)(nLength % 0x100));
			pPacket[3 + nLength] = ETX;
			pPacket[4 + nLength] = calcLRC(pBuffer, nLength);
			// Retransmit until the peer answers ACK
			do {
				if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
				{
					// TRACE(_T("Buffer Sent %s\n"), dumpHEX(pPacket, (5 + nLength)).c_str());
					const int nSize = pApplicationSocket.Receive(&nReturn, sizeof(nReturn));
					// TRACE(_T("%s Received\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
				}
			} while ((nReturn != ACK) && (++nCount <= 3));
			if (SendEOT && pApplicationSocket.IsWritable(1000))
			{
				unsigned char chEOT = EOT;
				// TRACE(_T("EOT Sent\n"));
				const int nSize = pApplicationSocket.Send(&chEOT, sizeof(chEOT));
			}
		}
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

    /// @brief Number of client connections currently being served.
    std::atomic<int> g_nActiveClients{0};

    /// @brief Serves a single client connection.
    ///
    /// Runs on its own thread and owns the socket, which is closed when the client
    /// disconnects or a socket error occurs.
    ///
    /// @param pSocket The accepted connection; ownership is transferred to this function.
    void HandleClient(std::unique_ptr<CSocketSTL> pSocket)
    {
        CSocketSTL& objSocket = *pSocket;
        ++g_nActiveClients;
        CSocketSTL::String strPeerAddress;
        UINT nPeerPort = 0;
        try
        {
            objSocket.GetPeerName(strPeerAddress, nPeerPort);
            std::cout << "Client connected: " << std::string(strPeerAddress.begin(), strPeerAddress.end()) << ":" << nPeerPort << std::endl;

        	// Receive the first packet, which must carry the "IntelliHost" protocol identifier
        	unsigned char pBuffer[MAX_BUFFER] = { 0, };
        	int nLength = sizeof(pBuffer);
        	ZeroMemory(pBuffer, sizeof(pBuffer));
        	if (ReadBuffer(objSocket, pBuffer, nLength, true, false))
        	{
        		// The payload starts after the STX and 2-byte length header
        		const std::string strCommand = (char*) &pBuffer[3];
        		if (strCommand == "IntelliHost")
        		{
        			std::cout << "Protocol acknowledged!" << std::endl;
        		}
        	}
        }
        catch (const CSocketException& ex)
        {
            std::string strError;
            ex.GetErrorMessage(strError);
            std::cerr << "server error: " << strError << std::endl;
        }
        objSocket.Close();
        std::cout << "Client disconnected: " << std::string(strPeerAddress.begin(), strPeerAddress.end()) << ":" << nPeerPort << std::endl;
        --g_nActiveClients;
    }
}

/// @brief Program entry point.
///
/// Loads IntelliHost.xml, listens on the configured ServerPort and serves every
/// accepted connection on its own detached thread. Runs until the process is killed.
///
/// @param nArgc The number of arguments (unused).
/// @param ppszArgv The argument vector (unused).
/// @return EXIT_FAILURE on a configuration or socket error.
int main(const int nArgc, char* ppszArgv[])
{
    std::string strError;
    std::string strServerPort;
    std::string strHostName;
    std::string strHostPort;
    std::string strDatabase;
    std::string strUsername;
    std::string strPassword;
    if (!LoadMachineFromConfiguration(strError, strServerPort, strHostName, strHostPort, strDatabase, strUsername, strPassword))
    {
        std::cerr << "error: " << strError << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        // std::stoul throws when the port is not numeric or out of range
        const unsigned long nServerPort = std::stoul(strServerPort);

        // Listening socket: binds to the wildcard IPv4 address on the server port
        CSocketSTL objListener;
        objListener.CreateAndBind(static_cast<UINT>(nServerPort), SOCK_STREAM, AF_INET);
        objListener.Listen(SOMAXCONN);
        std::cout << "Listening on port " << nServerPort << "..." << std::endl;

        // Each accepted connection is served by its own detached thread
        while (true)
        {
            auto objSocket = std::make_unique<CSocketSTL>();
            try
            {
                objListener.Accept(*objSocket);
            }
            // A failed accept must not stop the server
            catch (const CSocketException& ex)
            {
                std::string strAcceptError;
                ex.GetErrorMessage(strAcceptError);
                std::cerr << "accept error: " << strAcceptError << std::endl;
                continue;
            }
            std::thread(HandleClient, std::move(objSocket)).detach();
        }
    }
    catch (const CSocketException& ex)
    {
        std::string strSocketError;
        ex.GetErrorMessage(strSocketError);
        std::cerr << "error: " << strSocketError << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: invalid ServerPort: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
