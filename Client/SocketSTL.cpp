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

#include "SocketSTL.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace
{
#if defined(_WIN32)
	using SockLen = int;
	int LastSocketError() noexcept { return ::WSAGetLastError(); }
#else
	using SockLen = socklen_t;
	int LastSocketError() noexcept { return errno; }
#endif

	void FormatAddress(const sockaddr_storage& sockAddr, std::string& sAddress, UINT& nPort)
	{
		char szAddress[INET6_ADDRSTRLEN]{};
		if (sockAddr.ss_family == AF_INET)
		{
			const auto* pAddr = reinterpret_cast<const sockaddr_in*>(&sockAddr);
			if (inet_ntop(AF_INET, &pAddr->sin_addr, szAddress, sizeof(szAddress)) == nullptr)
				throw CSocketException(LastSocketError());
			nPort = ntohs(pAddr->sin_port);
		}
		else if (sockAddr.ss_family == AF_INET6)
		{
			const auto* pAddr = reinterpret_cast<const sockaddr_in6*>(&sockAddr);
			if (inet_ntop(AF_INET6, &pAddr->sin6_addr, szAddress, sizeof(szAddress)) == nullptr)
				throw CSocketException(LastSocketError());
			nPort = ntohs(pAddr->sin6_port);
		}
		else
			throw CSocketException(AF_UNSPEC);
		sAddress = szAddress;
	}
}

CSocketException::CSocketException(int nError) noexcept : m_nError{ nError }
{
}

bool CSocketException::GetErrorMessage(std::string& strError, unsigned int* pnHelpContext) const
{
    if (pnHelpContext != nullptr)
        *pnHelpContext = 0;

#if defined(_WIN32)
    LPSTR pMessageBuffer = nullptr;
    const DWORD nLength = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(m_nError),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&pMessageBuffer),
        0,
        nullptr);

    if (nLength == 0 || pMessageBuffer == nullptr) {
        strError.clear();
        return false;
    }

    strError.assign(pMessageBuffer, nLength);
    ::LocalFree(pMessageBuffer);

    while (!strError.empty() && (strError.back() == '\r' || strError.back() == '\n'))
        strError.pop_back();

    return true;
#elif defined(__linux__)
    char szMessage[256]{};

#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    const char* pszMessage = ::strerror_r(m_nError, szMessage, sizeof(szMessage));
    if (pszMessage == nullptr) {
        strError.clear();
        return false;
    }

    strError = pszMessage;
#else
    if (::strerror_r(m_nError, szMessage, sizeof(szMessage)) != 0) {
        strError.clear();
        return false;
    }

    strError = szMessage;
#endif

    return !strError.empty();
#else
#error "CSocketException::GetErrorMessage is supported only on Windows and Linux."
#endif
}

CSocketSTL::CSocketSTL() noexcept : m_hSocket{ INVALID_SOCKET }
{
}

CSocketSTL::~CSocketSTL() noexcept
{
    if (m_hSocket != INVALID_SOCKET)
        Close();
}

void CSocketSTL::Close() noexcept
{
    if (m_hSocket != INVALID_SOCKET)
    {
#if defined(_WIN32)
        closesocket(m_hSocket);
#else
        close(m_hSocket);
#endif
        m_hSocket = INVALID_SOCKET;
    }
}

void CSocketSTL::Attach(SOCKET hSocket) noexcept
{
    if (m_hSocket != INVALID_SOCKET)
        Close();
    m_hSocket = hSocket;
}

SOCKET CSocketSTL::Detach() noexcept
{
    const SOCKET socket{ m_hSocket };
    m_hSocket = INVALID_SOCKET;
    return socket;
}

void CSocketSTL::GetPeerName(String& sPeerAddress, UINT& nPeerPort, int /*nAddressToStringFlags*/) const
{
	sockaddr_storage sockAddr{};
	int nSockAddrLen{ static_cast<int>(sizeof(sockAddr)) };
	GetPeerName(reinterpret_cast<SOCKADDR*>(&sockAddr), &nSockAddrLen);
	std::string sAddress;
	FormatAddress(sockAddr, sAddress, nPeerPort);
	sPeerAddress.assign(sAddress.begin(), sAddress.end());
}

void CSocketSTL::GetSockName(String& sSocketAddress, UINT& nSocketPort, int /*nAddressToStringFlags*/) const
{
	sockaddr_storage sockAddr{};
	int nSockAddrLen{ static_cast<int>(sizeof(sockAddr)) };
	GetSockName(reinterpret_cast<SOCKADDR*>(&sockAddr), &nSockAddrLen);
	std::string sAddress;
	FormatAddress(sockAddr, sAddress, nSocketPort);
	sSocketAddress.assign(sAddress.begin(), sAddress.end());
}

void CSocketSTL::GetPeerName(SOCKADDR* pSockAddr, int* pSockAddrLen) const
{
	SockLen nLen{ static_cast<SockLen>(*pSockAddrLen) };
	if (getpeername(m_hSocket, pSockAddr, &nLen) != 0)
		throw CSocketException(LastSocketError());
	*pSockAddrLen = static_cast<int>(nLen);
}

void CSocketSTL::GetSockName(SOCKADDR* pSockAddr, int* pSockAddrLen) const
{
	SockLen nLen{ static_cast<SockLen>(*pSockAddrLen) };
	if (getsockname(m_hSocket, pSockAddr, &nLen) != 0)
		throw CSocketException(LastSocketError());
	*pSockAddrLen = static_cast<int>(nLen);
}

void CSocketSTL::SetSockOpt(int nOptionName, const void* pOptionValue, int nOptionLen, int nLevel) const
{
	if (setsockopt(m_hSocket, nLevel, nOptionName, static_cast<const char*>(pOptionValue), static_cast<SockLen>(nOptionLen)) != 0)
		throw CSocketException(LastSocketError());
}

void CSocketSTL::GetSockOpt(int nOptionName, void* pOptionValue, int* pOptionLen, int nLevel) const
{
	SockLen nLen{ static_cast<SockLen>(*pOptionLen) };
	if (getsockopt(m_hSocket, nLevel, nOptionName, static_cast<char*>(pOptionValue), &nLen) != 0)
		throw CSocketException(LastSocketError());
	*pOptionLen = static_cast<int>(nLen);
}
