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

#include <memory>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <cstring>
#include <climits>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
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

	// Waits up to dwTimeout ms for the requested event; error/hang-up also count as ready
	// so that the following recv/send reports the actual condition.
	bool PollSocket(SOCKET hSocket, short nEvents, const DWORD dwTimeout)
	{
		const int nTimeout{ dwTimeout > static_cast<DWORD>(INT_MAX) ? INT_MAX : static_cast<int>(dwTimeout) };
#if defined(_WIN32)
		WSAPOLLFD pfd{};
		pfd.fd = hSocket;
		pfd.events = nEvents;
		const int nStatus{ ::WSAPoll(&pfd, 1, nTimeout) };
		if (nStatus == SOCKET_ERROR)
			throw CSocketException(LastSocketError());
#else
		pollfd pfd{};
		pfd.fd = hSocket;
		pfd.events = nEvents;
		int nStatus{};
		do
		{
			nStatus = ::poll(&pfd, 1, nTimeout);
		} while (nStatus < 0 && errno == EINTR);
		if (nStatus < 0)
			throw CSocketException(LastSocketError());
#endif
		if (nStatus == 0)
			return false;
		if ((pfd.revents & POLLNVAL) != 0)
			throw CSocketException(
#if defined(_WIN32)
				WSAENOTSOCK
#else
				EBADF
#endif
			);
		return true;
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

namespace
{
#if defined(_WIN32)
	// Initializes Winsock once per process and cleans up at exit.
	struct WinsockInit
	{
		WinsockInit() noexcept { WSADATA wsaData{}; ::WSAStartup(MAKEWORD(2, 2), &wsaData); }
		~WinsockInit() { ::WSACleanup(); }
	};
#endif

	void EnsureSocketsInitialized() noexcept
	{
#if defined(_WIN32)
		static const WinsockInit s_Init;
#endif
	}
}

CSocketSTL::CSocketSTL() noexcept : m_hSocket{ INVALID_SOCKET }
{
	EnsureSocketsInitialized();
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

bool CSocketSTL::IsCreated() const noexcept
{
	return (m_hSocket != INVALID_SOCKET);
}

bool CSocketSTL::IsReadible(const DWORD dwTimeout) const
{
	return PollSocket(m_hSocket, POLLIN, dwTimeout);
}

bool CSocketSTL::IsWritable(const DWORD dwTimeout) const
{
	return PollSocket(m_hSocket, POLLOUT, dwTimeout);
}


void CSocketSTL::Create(bool bUDP, bool bIPv6)
{
	Create(bUDP ? SOCK_DGRAM : SOCK_STREAM, 0, bIPv6 ? AF_INET6 : PF_INET);
}

void CSocketSTL::Create(const int nSocketType, const int nProtocolType, const int nAddressFormat)
{
	m_hSocket = ::socket(nAddressFormat, nSocketType, nProtocolType);
	if (m_hSocket == INVALID_SOCKET)
		throw CSocketException(LastSocketError());
}

void CSocketSTL::Connect(const SOCKADDR* pSockAddr, const int nSockAddrLen) const
{
	if (pSockAddr == nullptr || nSockAddrLen <= 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	if (::connect(m_hSocket, pSockAddr, nSockAddrLen) == SOCKET_ERROR)
	{
		// A non-blocking socket reports WSAEWOULDBLOCK while the connection is in progress:
		// wait for writability, then check the pending socket error.
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			throw CSocketException(LastSocketError());
		if (!PollSocket(m_hSocket, POLLOUT, INFINITE))
			throw CSocketException(WSAETIMEDOUT);
		int nError{ 0 };
		int nLen{ static_cast<int>(sizeof(nError)) };
		if (getsockopt(m_hSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&nError), &nLen) != 0)
			throw CSocketException(LastSocketError());
		if (nError != 0)
			throw CSocketException(nError);
	}
#else
	if (::connect(m_hSocket, pSockAddr, static_cast<socklen_t>(nSockAddrLen)) == SOCKET_ERROR)
	{
		// EINTR: the connection continues asynchronously; EINPROGRESS: non-blocking socket.
		// Either way, wait for writability and then read the final result from SO_ERROR
		// (retrying connect() after EINTR would fail with EALREADY/EISCONN).
		if (errno != EINTR && errno != EINPROGRESS)
			throw CSocketException(LastSocketError());
		if (!PollSocket(m_hSocket, POLLOUT, static_cast<DWORD>(-1)))
			throw CSocketException(ETIMEDOUT);
		int nError{ 0 };
		socklen_t nLen{ static_cast<socklen_t>(sizeof(nError)) };
		if (getsockopt(m_hSocket, SOL_SOCKET, SO_ERROR, &nError, &nLen) != 0)
			throw CSocketException(LastSocketError());
		if (nError != 0)
			throw CSocketException(nError);
	}
#endif
}

void CSocketSTL::Accept(CSocketSTL& connectedSocket, SOCKADDR* pSockAddr, int* pSockAddrLen) const
{
	// The platform accept() expects SockLen (socklen_t on Linux, int on Windows)
	SockLen nLen{ (pSockAddrLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0) };
	SockLen* const pLen{ (pSockAddr != nullptr && pSockAddrLen != nullptr) ? &nLen : nullptr };
	SOCKET socket{ INVALID_SOCKET };

#if defined(_WIN32)
	socket = accept(m_hSocket, pLen != nullptr ? pSockAddr : nullptr, pLen);
	if (socket == INVALID_SOCKET)
		throw CSocketException(LastSocketError());
#else
	// Restart the call when interrupted by a signal
	do
	{
		nLen = (pSockAddrLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0);
		socket = ::accept(m_hSocket, pLen != nullptr ? pSockAddr : nullptr, pLen);
	} while (socket == INVALID_SOCKET && errno == EINTR);
	if (socket == INVALID_SOCKET)
		throw CSocketException(LastSocketError());
#endif

	if (pLen != nullptr)
		*pSockAddrLen = static_cast<int>(nLen);

	// Wrap the new socket up into a C++ instance
	connectedSocket.Attach(socket);
}

void CSocketSTL::CreateAndBind(const UINT nSocketPort, const int nSocketType, const int nDefaultAddressFormat)
{
	if (nSocketPort > 65535)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

	// Bind to a specific IP address when one was configured, otherwise to the wildcard address
	// of the requested family (AF_INET6 or AF_INET).
	std::string sBindAddress;
	for (const auto ch : m_sBindAddress)
		sBindAddress.push_back(static_cast<char>(ch));
	const std::string sPort{ std::to_string(nSocketPort) };

	addrinfo hints{};
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = nSocketType;
	if (sBindAddress.empty())
	{
		if (nDefaultAddressFormat != AF_INET && nDefaultAddressFormat != AF_INET6)
			return;
		hints.ai_family = nDefaultAddressFormat;
	}
	else
		hints.ai_family = AF_UNSPEC;

	addrinfo* pResult{ nullptr };
	const int nError{ ::getaddrinfo(sBindAddress.empty() ? nullptr : sBindAddress.c_str(), sPort.c_str(), &hints, &pResult) };
	if (nError != 0)
		throw CSocketException(nError);
	const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> lookup{ pResult, &::freeaddrinfo };

	// Try each candidate address until one can be created and bound
	int nLastError{ 0 };
	for (const addrinfo* pAddress = pResult; pAddress != nullptr; pAddress = pAddress->ai_next)
	{
		try
		{
			Create(nSocketType, 0, pAddress->ai_family);
		}
		catch (const CSocketException& e)
		{
			nLastError = e.m_nError;
			continue;
		}

		if (::bind(m_hSocket, pAddress->ai_addr, static_cast<SockLen>(pAddress->ai_addrlen)) == 0)
			return;

		nLastError = LastSocketError();
		Close();
	}
	throw CSocketException(nLastError);
}

void CSocketSTL::Bind(const SOCKADDR* pSockAddr, const int nSockAddrLen) const
{
	if (pSockAddr == nullptr || nSockAddrLen <= 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEFAULT
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	if (::bind(m_hSocket, pSockAddr, nSockAddrLen) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	if (::bind(m_hSocket, pSockAddr, static_cast<socklen_t>(nSockAddrLen)) == -1)
		throw CSocketException(LastSocketError());
#endif
}

void CSocketSTL::IOCtl(long lCommand, DWORD* pArgument) const
{
	if (pArgument == nullptr)
		throw CSocketException(
#if defined(_WIN32)
			WSAEFAULT
#else
			EFAULT
#endif
		);
#if defined(_WIN32)
	u_long ulArgument = static_cast<u_long>(*pArgument);
	if (::ioctlsocket(m_hSocket, lCommand, &ulArgument) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
	*pArgument = static_cast<DWORD>(ulArgument);
#else
	if (lCommand == FIONBIO)
	{
		// Winsock semantics: non-zero argument enables non-blocking mode
		const int nFlags = ::fcntl(m_hSocket, F_GETFL, 0);
		if (nFlags == -1)
			throw CSocketException(LastSocketError());
		const int nNewFlags = (*pArgument != 0) ? (nFlags | O_NONBLOCK) : (nFlags & ~O_NONBLOCK);
		if (nNewFlags != nFlags && ::fcntl(m_hSocket, F_SETFL, nNewFlags) == -1)
			throw CSocketException(LastSocketError());
		return;
	}
	// Linux ioctl requests expect an int, not a 64-bit unsigned long
	int nArgument = static_cast<int>(*pArgument);
	if (::ioctl(m_hSocket, static_cast<unsigned long>(lCommand), &nArgument) == -1)
		throw CSocketException(LastSocketError());
	*pArgument = static_cast<DWORD>(nArgument);
#endif
}

void CSocketSTL::Listen(const int nConnectionBacklog) const
{
#if defined(_WIN32)
	// Negative backlog is invalid; SOMAXCONN lets Winsock pick a sensible maximum
	const int nBacklog{ nConnectionBacklog > 0 ? nConnectionBacklog : SOMAXCONN };
	if (::listen(m_hSocket, nBacklog) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	const int nBacklog{ nConnectionBacklog > 0 ? nConnectionBacklog : SOMAXCONN };
	int nResult;
	do
	{
		nResult = ::listen(m_hSocket, nBacklog);
	} while (nResult == -1 && errno == EINTR);
	if (nResult == -1)
		throw CSocketException(LastSocketError());
#endif
}

void CSocketSTL::ShutDown(const int nHow) const
{
	// 0 = receive, 1 = send, 2 = both (SD_* on Windows, SHUT_* on Linux share these values)
	if (nHow < 0 || nHow > 2)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	const int nMode{ nHow == 0 ? SD_RECEIVE : (nHow == 1 ? SD_SEND : SD_BOTH) };
	if (::shutdown(m_hSocket, nMode) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	const int nMode{ nHow == 0 ? SHUT_RD : (nHow == 1 ? SHUT_WR : SHUT_RDWR) };
	int nResult;
	do
	{
		nResult = ::shutdown(m_hSocket, nMode);
	} while (nResult == -1 && errno == EINTR);
	if (nResult == -1)
		throw CSocketException(LastSocketError());
#endif
}

int CSocketSTL::Receive(void* pBuf, const int nBufLen, const int nFlags) const
{
	if (pBuf == nullptr || nBufLen < 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	const int nReceived{ ::recv(m_hSocket, static_cast<char*>(pBuf), nBufLen, nFlags) };
	if (nReceived == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	ssize_t nResult;
	do
	{
		nResult = ::recv(m_hSocket, pBuf, static_cast<size_t>(nBufLen), nFlags);
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	const int nReceived{ static_cast<int>(nResult) };
#endif

	return nReceived;
}

int CSocketSTL::ReceiveFrom(void* pBuffer, int nBufLen, SOCKADDR* pSockAddr, int* pSockAddrLen, int nFlags) const
{
	if (pBuffer == nullptr || nBufLen < 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

	SockLen nLen{ (pSockAddr != nullptr && pSockAddrLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0) };
	SockLen* const pLen{ (pSockAddr != nullptr && pSockAddrLen != nullptr) ? &nLen : nullptr };
	SOCKADDR* const pFrom{ pLen != nullptr ? pSockAddr : nullptr };

#if defined(_WIN32)
	const int nReceived{ ::recvfrom(m_hSocket, static_cast<char*>(pBuf), nBufLen, nFlags, pFrom, pLen) };
	if (nReceived == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	ssize_t nResult;
	do
	{
		nLen = (pLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0);
		nResult = ::recvfrom(m_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags, pFrom, pLen);
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	const int nReceived{ static_cast<int>(nResult) };
#endif

	if (pLen != nullptr)
		*pSockAddrLen = static_cast<int>(nLen);
	return nReceived;
}

int CSocketSTL::ReceiveFrom(void* pBuffer, const int nBufLen, String& strSocketAddress, UINT& nSocketPort, const int nReceiveFromFlags, int /*nAddressToStringFlags*/) const
{
	sockaddr_storage sockAddr{};
	int nSockAddrLen{ static_cast<int>(sizeof(sockAddr)) };
	const int nResult{ ReceiveFrom(pBuffer, nBufLen, reinterpret_cast<SOCKADDR*>(&sockAddr), &nSockAddrLen, nReceiveFromFlags) };

	std::string sAddress;
	FormatAddress(sockAddr, sAddress, nSocketPort);
	strSocketAddress.assign(sAddress.begin(), sAddress.end());
	return nResult;
}

int CSocketSTL::Send(const void* pBuffer, int nBufLen, int nFlags) const
{
#if defined(_WIN32)
	const int nSent{ ::send(m_hSocket, static_cast<const char*>(pBuffer), nBufLen, nFlags) };
	if (nSent == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
	return nSent;
#else
#if defined(MSG_NOSIGNAL)
	nFlags |= MSG_NOSIGNAL; // report EPIPE instead of raising SIGPIPE
#endif
	ssize_t nResult;
	do
	{
		nResult = ::send(m_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags);
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	return static_cast<int>(nResult);
#endif
}

int CSocketSTL::SendTo(const void* pBuffer, int nBufLen, const SOCKADDR* pSockAddr, int nSockAddrLen, int nFlags) const
{
	if (pBuffer == nullptr || nBufLen < 0 || pSockAddr == nullptr || nSockAddrLen <= 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	const int nSent{ ::sendto(m_hSocket, static_cast<const char*>(pBuffer), nBufLen, nFlags, pSockAddr, nSockAddrLen) };
	if (nSent == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
	return nSent;
#else
#if defined(MSG_NOSIGNAL)
	nFlags |= MSG_NOSIGNAL; // report EPIPE instead of raising SIGPIPE
#endif
	ssize_t nResult;
	do
	{
		nResult = ::sendto(m_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags, pSockAddr, static_cast<SockLen>(nSockAddrLen));
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	return static_cast<int>(nResult);
#endif
}

int CSocketSTL::SendTo(const void* pBuffer, int nBufLen, UINT nHostPort, const String& strHostAddress, int nFlags) const
{
	if (nHostPort > 65535)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

	std::string sHost;
	for (const auto ch : strHostAddress)
		sHost.push_back(static_cast<char>(ch));
	const std::string sPort{ std::to_string(nHostPort) };

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	addrinfo* pResult{ nullptr };
	const int nError{ ::getaddrinfo(sHost.empty() ? nullptr : sHost.c_str(), sPort.c_str(), &hints, &pResult) };
	if (nError != 0)
		throw CSocketException(nError);
	const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> lookup{ pResult, &::freeaddrinfo };

	// Try each resolved address until one accepts the datagram
	int nLastError{ 0 };
	for (const addrinfo* pAddress = pResult; pAddress != nullptr; pAddress = pAddress->ai_next)
	{
		try
		{
			return SendTo(pBuffer, nBufLen, pAddress->ai_addr, static_cast<int>(pAddress->ai_addrlen), nFlags);
		}
		catch (const CSocketException& e)
		{
			nLastError = e.m_nError;
		}
	}
	throw CSocketException(nLastError);
}
