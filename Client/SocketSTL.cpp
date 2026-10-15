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

	/**
	 * Converts a sockaddr_storage (IPv4/IPv6) to a printable address and port.
	 *
	 * @param stSockAddr the socket address to convert.
	 * @param sAddress receives the textual IP address.
	 * @param nPort    receives the port in host byte order.
	 * @throws CSocketException on conversion failure or unsupported family.
	 */
	void FormatAddress(const sockaddr_storage& stSockAddr, std::string& sAddress, UINT& nPort)
	{
		char szAddress[INET6_ADDRSTRLEN]{}; // large enough for either family
		if (stSockAddr.ss_family == AF_INET)
		{
			const auto* pAddr = reinterpret_cast<const sockaddr_in*>(&stSockAddr);
			if (inet_ntop(AF_INET, &pAddr->sin_addr, szAddress, sizeof(szAddress)) == nullptr)
				throw CSocketException(LastSocketError());
			nPort = ntohs(pAddr->sin_port);
		}
		else if (stSockAddr.ss_family == AF_INET6)
		{
			const auto* pAddr = reinterpret_cast<const sockaddr_in6*>(&stSockAddr);
			if (inet_ntop(AF_INET6, &pAddr->sin6_addr, szAddress, sizeof(szAddress)) == nullptr)
				throw CSocketException(LastSocketError());
			nPort = ntohs(pAddr->sin6_port);
		}
		else
			throw CSocketException(AF_UNSPEC); // unsupported address family
		sAddress = szAddress;
	}

	/**
	 * Waits for the requested event; error/hang-up also count as ready so that
	 * the following recv/send reports the actual condition.
	 *
	 * @param hSocket  socket to poll.
	 * @param nEvents  event mask (POLLIN / POLLOUT).
	 * @param dwTimeout maximum wait in milliseconds (clamped to INT_MAX).
	 * @return true if ready, false on timeout.
	 * @throws CSocketException if polling fails or the handle is not a socket.
	 */
	bool PollSocket(SOCKET hSocket, short nEvents, const DWORD dwTimeout)
	{
		// poll() takes an int, so clamp large timeouts
		const int nTimeout{ dwTimeout > static_cast<DWORD>(INT_MAX) ? INT_MAX : static_cast<int>(dwTimeout) };
#if defined(_WIN32)
		WSAPOLLFD stPollFd{};
		stPollFd.fd = hSocket;
		stPollFd.events = nEvents;
		const int nStatus{ ::WSAPoll(&stPollFd, 1, nTimeout) };
		if (nStatus == SOCKET_ERROR)
			throw CSocketException(LastSocketError());
#else
		pollfd stPollFd{};
		stPollFd.fd = hSocket;
		stPollFd.events = nEvents;
		int nStatus{};
		do
		{
			nStatus = ::poll(&stPollFd, 1, nTimeout);
		} while (nStatus < 0 && errno == EINTR);
		if (nStatus < 0)
			throw CSocketException(LastSocketError());
#endif
		if (nStatus == 0)
			return false; // timed out
		if ((stPollFd.revents & POLLNVAL) != 0) // handle is not an open socket
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

/**
 * Constructs the exception from a platform error code.
 *
 * @param nError errno, WSAGetLastError() or getaddrinfo() result.
 */
CSocketException::CSocketException(int nError) noexcept : _nError{ nError }
{
}

/**
 * Retrieves the system's text description of the stored error code.
 *
 * @param strError      receives the message (cleared on failure).
 * @param pnHelpContext optional; always set to 0 (no help context).
 * @return true if a message was produced, false if none is available.
 */
bool CSocketException::GetErrorMessage(std::string& strError, unsigned int* pnHelpContext) const
{
	if (pnHelpContext != nullptr)
		*pnHelpContext = 0;

#if defined(_WIN32)
	LPSTR pMessageBuffer{ nullptr }; // allocated by FormatMessageA, freed with LocalFree
	const DWORD nLength{ ::FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		static_cast<DWORD>(_nError),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&pMessageBuffer),
		0,
		nullptr) };

	if (nLength == 0 || pMessageBuffer == nullptr)
	{
		strError.clear();
		return false;
	}

	strError.assign(pMessageBuffer, nLength);
	::LocalFree(pMessageBuffer);

	// Strip the trailing CR/LF that FormatMessage appends
	while (!strError.empty() && (strError.back() == '\r' || strError.back() == '\n'))
		strError.pop_back();

	return true;
#elif defined(__linux__)
	char szMessage[256]{};

#if defined(__GLIBC__) && defined(_GNU_SOURCE)
	// GNU strerror_r returns a char* (which may not point into szMessage)
	const char* pszMessage{ ::strerror_r(_nError, szMessage, sizeof(szMessage)) };
	if (pszMessage == nullptr)
	{
		strError.clear();
		return false;
	}

	strError = pszMessage;
#else
	// XSI strerror_r returns 0 on success
	if (::strerror_r(_nError, szMessage, sizeof(szMessage)) != 0)
	{
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
		WinsockInit() noexcept { WSADATA stWsaData{}; ::WSAStartup(MAKEWORD(2, 2), &stWsaData); }
		~WinsockInit() { ::WSACleanup(); }
	};
#endif

	/**
	 * Triggers one-time Winsock startup (no-op on Linux).
	 */
	void EnsureSocketsInitialized() noexcept
	{
#if defined(_WIN32)
		static const WinsockInit s_Init;
#endif
	}
}

/**
 * Default constructor. Holds no socket and ensures the socket subsystem
 * (Winsock on Windows) is initialized.
 */
CSocketSTL::CSocketSTL() noexcept : _hSocket{ INVALID_SOCKET }
{
	EnsureSocketsInitialized();
}

/**
 * Destructor. Closes the owned socket, if any, so the handle never leaks.
 */
CSocketSTL::~CSocketSTL() noexcept
{
	if (_hSocket != INVALID_SOCKET)
		Close();
}

/**
 * Closes the socket (closesocket / close). Safe to call repeatedly.
 */
void CSocketSTL::Close() noexcept
{
	// Reset the handle afterwards so repeated calls are harmless
	if (_hSocket != INVALID_SOCKET)
	{
#if defined(_WIN32)
		closesocket(_hSocket);
#else
		close(_hSocket);
#endif
		_hSocket = INVALID_SOCKET;
	}
}

/**
 * Takes ownership of an existing socket handle, closing any current one.
 *
 * @param hSocket the socket handle to adopt.
 */
void CSocketSTL::Attach(SOCKET hSocket) noexcept
{
	// Release any socket we already own before taking the new one
	if (_hSocket != INVALID_SOCKET)
		Close();
	_hSocket = hSocket;
}

/**
 * Releases ownership without closing the socket.
 *
 * @return the handle; the caller becomes responsible for closing it.
 */
SOCKET CSocketSTL::Detach() noexcept
{
	// Hand the handle to the caller without closing it
	const SOCKET hNewSocket{ _hSocket };
	_hSocket = INVALID_SOCKET;
	return hNewSocket;
}

/**
 * Retrieves the remote endpoint as printable address and port.
 *
 * @param sPeerAddress receives the peer IP address (IPv4 or IPv6).
 * @param nPeerPort    receives the peer port in host byte order.
 * @throws CSocketException if the lookup fails or the address family is unsupported.
 */
void CSocketSTL::GetPeerName(String& sPeerAddress, UINT& nPeerPort, int /*nAddressToStringFlags*/) const
{
	// sockaddr_storage can hold both IPv4 and IPv6 addresses
	sockaddr_storage stSockAddr{};
	int nSockAddrLen{ static_cast<int>(sizeof(stSockAddr)) };
	GetPeerName(reinterpret_cast<SOCKADDR*>(&stSockAddr), &nSockAddrLen);
	std::string sAddress;
	FormatAddress(stSockAddr, sAddress, nPeerPort);
	// Widen to String (wstring when _UNICODE)
	sPeerAddress.assign(sAddress.begin(), sAddress.end());
}

/**
 * Retrieves the local endpoint as printable address and port.
 *
 * @param sSocketAddress receives the local IP address (IPv4 or IPv6).
 * @param nSocketPort    receives the local port in host byte order.
 * @throws CSocketException if the lookup fails or the address family is unsupported.
 */
void CSocketSTL::GetSockName(String& sSocketAddress, UINT& nSocketPort, int /*nAddressToStringFlags*/) const
{
	sockaddr_storage stSockAddr{};
	int nSockAddrLen{ static_cast<int>(sizeof(stSockAddr)) };
	GetSockName(reinterpret_cast<SOCKADDR*>(&stSockAddr), &nSockAddrLen);
	std::string sAddress;
	FormatAddress(stSockAddr, sAddress, nSocketPort);
	sSocketAddress.assign(sAddress.begin(), sAddress.end());
}

/**
 * Raw getpeername() wrapper.
 *
 * @param pSockAddr    receives the peer address.
 * @param pSockAddrLen in: buffer size; out: bytes written.
 * @throws CSocketException on failure.
 */
void CSocketSTL::GetPeerName(SOCKADDR* pSockAddr, int* pSockAddrLen) const
{
	// Use the platform's length type, then copy the result back as int
	SockLen nLen{ static_cast<SockLen>(*pSockAddrLen) };
	if (getpeername(_hSocket, pSockAddr, &nLen) != 0)
		throw CSocketException(LastSocketError());
	*pSockAddrLen = static_cast<int>(nLen);
}

/**
 * Raw getsockname() wrapper.
 *
 * @param pSockAddr    receives the local address.
 * @param pSockAddrLen in: buffer size; out: bytes written.
 * @throws CSocketException on failure.
 */
void CSocketSTL::GetSockName(SOCKADDR* pSockAddr, int* pSockAddrLen) const
{
	SockLen nLen{ static_cast<SockLen>(*pSockAddrLen) };
	if (getsockname(_hSocket, pSockAddr, &nLen) != 0)
		throw CSocketException(LastSocketError());
	*pSockAddrLen = static_cast<int>(nLen);
}

/**
 * Sets a socket option (setsockopt).
 *
 * @param nOptionName  option identifier.
 * @param pOptionValue pointer to the option value.
 * @param nOptionLen   size of the value in bytes.
 * @param nLevel       protocol level (e.g. SOL_SOCKET).
 * @throws CSocketException on failure.
 */
void CSocketSTL::SetSockOpt(int nOptionName, const void* pOptionValue, int nOptionLen, int nLevel) const
{
	if (setsockopt(_hSocket, nLevel, nOptionName, static_cast<const char*>(pOptionValue), static_cast<SockLen>(nOptionLen)) != 0)
		throw CSocketException(LastSocketError());
}

/**
 * Reads a socket option (getsockopt).
 *
 * @param nOptionName  option identifier.
 * @param pOptionValue receives the option value.
 * @param pOptionLen   in: buffer size; out: bytes written.
 * @param nLevel       protocol level (e.g. SOL_SOCKET).
 * @throws CSocketException on failure.
 */
void CSocketSTL::GetSockOpt(int nOptionName, void* pOptionValue, int* pOptionLen, int nLevel) const
{
	SockLen nLen{ static_cast<SockLen>(*pOptionLen) }; // in: buffer size, out: bytes written
	if (getsockopt(_hSocket, nLevel, nOptionName, static_cast<char*>(pOptionValue), &nLen) != 0)
		throw CSocketException(LastSocketError());
	*pOptionLen = static_cast<int>(nLen);
}

/**
 * @return true when a valid socket handle is held.
 */
bool CSocketSTL::IsCreated() const noexcept
{
	return (_hSocket != INVALID_SOCKET);
}

/**
 * Waits for incoming data or a pending connection.
 *
 * @param dwTimeout maximum wait in milliseconds.
 * @return true if readable (or error/hang-up), false on timeout.
 * @throws CSocketException if polling fails.
 */
bool CSocketSTL::IsReadible(const DWORD dwTimeout) const
{
	return PollSocket(_hSocket, POLLIN, dwTimeout); // POLLIN: data (or a pending connection) is available
}

/**
 * Waits until a send will not block.
 *
 * @param dwTimeout maximum wait in milliseconds.
 * @return true if writable (or error/hang-up), false on timeout.
 * @throws CSocketException if polling fails.
 */
bool CSocketSTL::IsWritable(const DWORD dwTimeout) const
{
	return PollSocket(_hSocket, POLLOUT, dwTimeout); // POLLOUT: a send will not block
}


/**
 * Creates a socket, choosing the type and family from flags.
 *
 * @param bUDP  true for a datagram (UDP) socket, false for a stream (TCP) socket.
 * @param bIPv6 true for IPv6, false for IPv4.
 * @throws CSocketException if socket creation fails.
 */
void CSocketSTL::Create(bool bUDP, bool bIPv6)
{
	// Datagram for UDP, stream for TCP; protocol 0 picks the default for the type
	Create(bUDP ? SOCK_DGRAM : SOCK_STREAM, 0, bIPv6 ? AF_INET6 : PF_INET);
}

/**
 * Creates the underlying socket. A previously held handle is not closed.
 *
 * @param nSocketType    e.g. SOCK_STREAM or SOCK_DGRAM.
 * @param nProtocolType  protocol number (0 = default for the type).
 * @param nAddressFormat address family (e.g. AF_INET, AF_INET6).
 * @throws CSocketException if socket creation fails.
 */
void CSocketSTL::Create(const int nSocketType, const int nProtocolType, const int nAddressFormat)
{
	// Note: overwrites _hSocket without closing a previous handle
	_hSocket = ::socket(nAddressFormat, nSocketType, nProtocolType);
	if (_hSocket == INVALID_SOCKET)
		throw CSocketException(LastSocketError());
}

/**
 * Connects to a remote address, waiting for completion when the connect is
 * non-blocking or interrupted.
 *
 * @param pSockAddr    remote address.
 * @param nSockAddrLen size of the address in bytes.
 * @throws CSocketException on invalid arguments, timeout or connection failure.
 */
void CSocketSTL::Connect(const SOCKADDR* pSockAddr, const int nSockAddrLen) const
{
	// Reject a missing or empty address
	if (pSockAddr == nullptr || nSockAddrLen <= 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	if (::connect(_hSocket, pSockAddr, nSockAddrLen) == SOCKET_ERROR)
	{
		// A non-blocking socket reports WSAEWOULDBLOCK while the connection is in progress:
		// wait for writability, then check the pending socket error.
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			throw CSocketException(LastSocketError());
		if (!PollSocket(_hSocket, POLLOUT, INFINITE))
			throw CSocketException(WSAETIMEDOUT);
		int nError{ 0 };
		int nLen{ static_cast<int>(sizeof(nError)) };
		if (getsockopt(_hSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&nError), &nLen) != 0)
			throw CSocketException(LastSocketError());
		if (nError != 0)
			throw CSocketException(nError);
	}
#else
	if (::connect(_hSocket, pSockAddr, static_cast<socklen_t>(nSockAddrLen)) == SOCKET_ERROR)
	{
		// EINTR: the connection continues asynchronously; EINPROGRESS: non-blocking socket.
		// Either way, wait for writability and then read the final result from SO_ERROR
		// (retrying connect() after EINTR would fail with EALREADY/EISCONN).
		if (errno != EINTR && errno != EINPROGRESS)
			throw CSocketException(LastSocketError());
		if (!PollSocket(_hSocket, POLLOUT, static_cast<DWORD>(-1)))
			throw CSocketException(ETIMEDOUT);
		int nError{ 0 };
		socklen_t nLen{ static_cast<socklen_t>(sizeof(nError)) };
		if (getsockopt(_hSocket, SOL_SOCKET, SO_ERROR, &nError, &nLen) != 0)
			throw CSocketException(LastSocketError());
		if (nError != 0)
			throw CSocketException(nError);
	}
#endif
}

/**
 * Accepts an incoming connection.
 *
 * @param objConnectedSocket receives (takes ownership of) the accepted socket.
 * @param pSockAddr       optional; receives the peer address.
 * @param pSockAddrLen    optional; in: buffer size, out: bytes written.
 * @throws CSocketException if accept fails.
 */
void CSocketSTL::Accept(CSocketSTL& objConnectedSocket, SOCKADDR* pSockAddr, int* pSockAddrLen) const
{
	// The platform accept() expects SockLen (socklen_t on Linux, int on Windows)
	SockLen nLen{ (pSockAddrLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0) };
	SockLen* const pLen{ (pSockAddr != nullptr && pSockAddrLen != nullptr) ? &nLen : nullptr };
	SOCKET hNewSocket{ INVALID_SOCKET };

#if defined(_WIN32)
	hNewSocket = accept(_hSocket, pLen != nullptr ? pSockAddr : nullptr, pLen);
	if (hNewSocket == INVALID_SOCKET)
		throw CSocketException(LastSocketError());
#else
	// Restart the call when interrupted by a signal
	do
	{
		nLen = (pSockAddrLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0);
		hNewSocket = ::accept(_hSocket, pLen != nullptr ? pSockAddr : nullptr, pLen);
	} while (hNewSocket == INVALID_SOCKET && errno == EINTR);
	if (hNewSocket == INVALID_SOCKET)
		throw CSocketException(LastSocketError());
#endif

	if (pLen != nullptr)
		*pSockAddrLen = static_cast<int>(nLen);

	// Wrap the new socket up into a C++ instance
	objConnectedSocket.Attach(hNewSocket);
}

/**
 * Creates a socket and binds it to the configured bind address (or the
 * wildcard address), using the first resolved candidate that works.
 *
 * @param nSocketPort           port to bind (0-65535).
 * @param nSocketType           e.g. SOCK_STREAM or SOCK_DGRAM.
 * @param nDefaultAddressFormat AF_INET or AF_INET6 when no bind address is set;
 *                              any other value does nothing.
 * @throws CSocketException on invalid port, resolution, creation or bind failure.
 */
void CSocketSTL::CreateAndBind(const UINT nSocketPort, const int nSocketType, const int nDefaultAddressFormat)
{
	// Ports are 16-bit
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
	for (const auto ch : _sBindAddress)
		sBindAddress.push_back(static_cast<char>(ch));
	const std::string sPort{ std::to_string(nSocketPort) };

	// AI_PASSIVE: resolve to a wildcard address suitable for bind() when no host is given
	addrinfo stHints{};
	stHints.ai_flags = AI_PASSIVE;
	stHints.ai_socktype = nSocketType;
	if (sBindAddress.empty())
	{
		if (nDefaultAddressFormat != AF_INET && nDefaultAddressFormat != AF_INET6)
			return; // unsupported family: nothing to create (no error raised)
		stHints.ai_family = nDefaultAddressFormat;
	}
	else
		stHints.ai_family = AF_UNSPEC; // let the bind address decide the family

	addrinfo* pResult{ nullptr };
	const int nError{ ::getaddrinfo(sBindAddress.empty() ? nullptr : sBindAddress.c_str(), sPort.c_str(), &stHints, &pResult) };
	if (nError != 0)
		throw CSocketException(nError);
	// RAII: frees the getaddrinfo result on every exit path
	const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> pLookup{ pResult, &::freeaddrinfo };

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
			nLastError = e._nError;
			continue;
		}

		if (::bind(_hSocket, pAddress->ai_addr, static_cast<SockLen>(pAddress->ai_addrlen)) == 0)
			return;

		nLastError = LastSocketError();
		Close(); // discard the unbound socket and try the next address
	}
	throw CSocketException(nLastError); // no candidate address worked
}

/**
 * Binds the socket to a local address.
 *
 * @param pSockAddr    local address.
 * @param nSockAddrLen size of the address in bytes.
 * @throws CSocketException on invalid arguments or bind failure.
 */
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
	if (::bind(_hSocket, pSockAddr, nSockAddrLen) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	if (::bind(_hSocket, pSockAddr, static_cast<socklen_t>(nSockAddrLen)) == -1)
		throw CSocketException(LastSocketError());
#endif
}

/**
 * Winsock-style ioctl. On Linux, FIONBIO is mapped to fcntl() O_NONBLOCK.
 *
 * @param lCommand  control command (e.g. FIONBIO).
 * @param pArgument in/out command argument; must not be null.
 * @throws CSocketException on invalid argument or failure.
 */
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
	// ioctlsocket takes a u_long in/out argument
	u_long ulArgument = static_cast<u_long>(*pArgument);
	if (::ioctlsocket(_hSocket, lCommand, &ulArgument) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
	*pArgument = static_cast<DWORD>(ulArgument);
#else
	if (lCommand == FIONBIO)
	{
		// Winsock semantics: non-zero argument enables non-blocking mode
		const int nFlags = ::fcntl(_hSocket, F_GETFL, 0);
		if (nFlags == -1)
			throw CSocketException(LastSocketError());
		const int nNewFlags = (*pArgument != 0) ? (nFlags | O_NONBLOCK) : (nFlags & ~O_NONBLOCK);
		if (nNewFlags != nFlags && ::fcntl(_hSocket, F_SETFL, nNewFlags) == -1)
			throw CSocketException(LastSocketError());
		return;
	}
	// Linux ioctl requests expect an int, not a 64-bit unsigned long
	int nArgument = static_cast<int>(*pArgument);
	if (::ioctl(_hSocket, static_cast<unsigned long>(lCommand), &nArgument) == -1)
		throw CSocketException(LastSocketError());
	*pArgument = static_cast<DWORD>(nArgument);
#endif
}

/**
 * Puts the socket in listening state.
 *
 * @param nConnectionBacklog queue length; non-positive uses SOMAXCONN.
 * @throws CSocketException on failure.
 */
void CSocketSTL::Listen(const int nConnectionBacklog) const
{
#if defined(_WIN32)
	// Negative backlog is invalid; SOMAXCONN lets Winsock pick a sensible maximum
	const int nBacklog{ nConnectionBacklog > 0 ? nConnectionBacklog : SOMAXCONN };
	if (::listen(_hSocket, nBacklog) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	const int nBacklog{ nConnectionBacklog > 0 ? nConnectionBacklog : SOMAXCONN };
	int nResult;
	do
	{
		nResult = ::listen(_hSocket, nBacklog);
	} while (nResult == -1 && errno == EINTR);
	if (nResult == -1)
		throw CSocketException(LastSocketError());
#endif
}

/**
 * Disables receiving, sending or both on the connection.
 *
 * @param nHow 0 = receive, 1 = send, 2 = both.
 * @throws CSocketException on invalid value or failure.
 */
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
	if (::shutdown(_hSocket, nMode) == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	const int nMode{ nHow == 0 ? SHUT_RD : (nHow == 1 ? SHUT_WR : SHUT_RDWR) };
	int nResult;
	do
	{
		nResult = ::shutdown(_hSocket, nMode);
	} while (nResult == -1 && errno == EINTR);
	if (nResult == -1)
		throw CSocketException(LastSocketError());
#endif
}

/**
 * Receives data from a connected socket.
 *
 * @param pBuffer    destination buffer.
 * @param nBufLen buffer capacity in bytes.
 * @param nFlags  recv() flags.
 * @return number of bytes received (0 = peer closed the connection).
 * @throws CSocketException on invalid arguments or failure.
 */
int CSocketSTL::Receive(void* pBuffer, const int nBufLen, const int nFlags) const
{
	// Validate arguments
	if (pBuffer == nullptr || nBufLen < 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

#if defined(_WIN32)
	const int nReceived{ ::recv(_hSocket, static_cast<char*>(pBuffer), nBufLen, nFlags) };
	if (nReceived == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	ssize_t nResult; // retry when interrupted by a signal
	do
	{
		nResult = ::recv(_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags);
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	const int nReceived{ static_cast<int>(nResult) };
#endif

	return nReceived;
}

/**
 * Receives a datagram and optionally reports the sender's raw address.
 *
 * @param pBuffer      destination buffer.
 * @param nBufLen      buffer capacity in bytes.
 * @param pSockAddr    optional; receives the sender address.
 * @param pSockAddrLen optional; in: buffer size, out: bytes written.
 * @param nFlags       recvfrom() flags.
 * @return number of bytes received.
 * @throws CSocketException on invalid arguments or failure.
 */
int CSocketSTL::ReceiveFrom(void* pBuffer, const int nBufLen, SOCKADDR* pSockAddr, int* pSockAddrLen, int nFlags) const
{
	if (pBuffer == nullptr || nBufLen < 0)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

	// The source address is only requested when both the buffer and its length are supplied
	SockLen nLen{ (pSockAddr != nullptr && pSockAddrLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0) };
	SockLen* const pLen{ (pSockAddr != nullptr && pSockAddrLen != nullptr) ? &nLen : nullptr };
	SOCKADDR* const pFrom{ pLen != nullptr ? pSockAddr : nullptr };

#if defined(_WIN32)
	const int nReceived{ ::recvfrom(_hSocket, static_cast<char*>(pBuf), nBufLen, nFlags, pFrom, pLen) };
	if (nReceived == SOCKET_ERROR)
		throw CSocketException(LastSocketError());
#else
	ssize_t nResult;
	do
	{
		nLen = (pLen != nullptr) ? static_cast<SockLen>(*pSockAddrLen) : static_cast<SockLen>(0);
		nResult = ::recvfrom(_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags, pFrom, pLen);
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	const int nReceived{ static_cast<int>(nResult) };
#endif

	if (pLen != nullptr)
		*pSockAddrLen = static_cast<int>(nLen);
	return nReceived;
}

/**
 * Receives a datagram and reports the sender as printable address and port.
 *
 * @param pBuffer             destination buffer.
 * @param nBufLen             buffer capacity in bytes.
 * @param strSocketAddress    receives the sender IP address.
 * @param nSocketPort         receives the sender port (host byte order).
 * @param nReceiveFromFlags   recvfrom() flags.
 * @param nAddressToStringFlags unused (kept for API compatibility).
 * @return number of bytes received.
 * @throws CSocketException on failure.
 */
int CSocketSTL::ReceiveFrom(void* pBuffer, const int nBufLen, String& strSocketAddress, UINT& nSocketPort, const int nReceiveFromFlags, int /*nAddressToStringFlags*/) const
{
	sockaddr_storage stSockAddr{};
	int nSockAddrLen{ static_cast<int>(sizeof(stSockAddr)) };
	// Receive into a generic address buffer, then format it as text
	const int nResult{ ReceiveFrom(pBuffer, nBufLen, reinterpret_cast<SOCKADDR*>(&stSockAddr), &nSockAddrLen, nReceiveFromFlags) };

	std::string sAddress;
	FormatAddress(stSockAddr, sAddress, nSocketPort);
	strSocketAddress.assign(sAddress.begin(), sAddress.end());
	return nResult;
}

/**
 * Sends data on a connected socket. On Linux, SIGPIPE is suppressed
 * (MSG_NOSIGNAL) and EPIPE is reported instead.
 *
 * @param pBuffer data to send.
 * @param nBufLen number of bytes to send.
 * @param nFlags  send() flags.
 * @return number of bytes actually sent.
 * @throws CSocketException on failure.
 */
int CSocketSTL::Send(const void* pBuffer, const int nBufLen, int nFlags) const
{
#if defined(_WIN32)
	const int nSent{ ::send(_hSocket, static_cast<const char*>(pBuffer), nBufLen, nFlags) };
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
		nResult = ::send(_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags);
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	return static_cast<int>(nResult);
#endif
}

/**
 * Sends a datagram to a raw socket address.
 *
 * @param pBuffer      data to send.
 * @param nBufLen      number of bytes to send.
 * @param pSockAddr    destination address.
 * @param nSockAddrLen size of the address in bytes.
 * @param nFlags       sendto() flags.
 * @return number of bytes actually sent.
 * @throws CSocketException on invalid arguments or failure.
 */
int CSocketSTL::SendTo(const void* pBuffer, const int nBufLen, const SOCKADDR* pSockAddr, int nSockAddrLen, int nFlags) const
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
	const int nSent{ ::sendto(_hSocket, static_cast<const char*>(pBuffer), nBufLen, nFlags, pSockAddr, nSockAddrLen) };
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
		nResult = ::sendto(_hSocket, pBuffer, static_cast<size_t>(nBufLen), nFlags, pSockAddr, static_cast<SockLen>(nSockAddrLen));
	} while (nResult < 0 && errno == EINTR);
	if (nResult < 0)
		throw CSocketException(LastSocketError());
	return static_cast<int>(nResult);
#endif
}

/**
 * Resolves a host name/IP and sends a datagram to the first address that works.
 *
 * @param pBuffer        data to send.
 * @param nBufLen        number of bytes to send.
 * @param nHostPort      destination port (0-65535).
 * @param strHostAddress destination host name or IP address.
 * @param nFlags         sendto() flags.
 * @return number of bytes actually sent.
 * @throws CSocketException on invalid port, resolution failure or send failure.
 */
int CSocketSTL::SendTo(const void* pBuffer, const int nBufLen, const UINT nHostPort, const String& strHostAddress, const int nFlags) const
{
	if (nHostPort > 65535)
		throw CSocketException(
#if defined(_WIN32)
			WSAEINVAL
#else
			EINVAL
#endif
		);

	// Narrow the host string for getaddrinfo
	std::string sHost;
	for (const auto ch : strHostAddress)
		sHost.push_back(static_cast<char>(ch));
	const std::string sPort{ std::to_string(nHostPort) };

	addrinfo stHints{};
	stHints.ai_family = AF_UNSPEC;
	stHints.ai_socktype = SOCK_DGRAM; // SendTo is datagram-oriented

	addrinfo* pResult{ nullptr };
	const int nError{ ::getaddrinfo(sHost.empty() ? nullptr : sHost.c_str(), sPort.c_str(), &stHints, &pResult) };
	if (nError != 0)
		throw CSocketException(nError);
	const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> pLookup{ pResult, &::freeaddrinfo };

	// Try each resolved address until one accepts the datagram
	int nLastError{ 0 };
	for (const addrinfo* pAddress = pResult; pAddress != nullptr; pAddress = pAddress->ai_next)
	{
		try
		{
			// Delegate to the raw-address overload; first success wins
			return SendTo(pBuffer, nBufLen, pAddress->ai_addr, static_cast<int>(pAddress->ai_addrlen), nFlags);
		}
		catch (const CSocketException& e)
		{
			nLastError = e._nError;
		}
	}
	throw CSocketException(nLastError);
}
