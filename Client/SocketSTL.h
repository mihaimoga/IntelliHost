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

#ifndef SOCKETSTL_H
#define SOCKETSTL_H

#include <string>
#include <exception>
#include <system_error>

class CSocketException : public std::exception
{
public:
	// Constructors / Destructors
	CSocketException(int nError) noexcept;

	// Methods
	bool GetErrorMessage(std::string& strError, unsigned int* pnHelpContext = nullptr) const;

	// Data members
	int _nError;
};

#ifndef SOCKET
typedef int SOCKET;
#endif

#ifndef SOCKADDR
typedef struct sockaddr SOCKADDR;
#endif

#ifndef UINT
typedef unsigned int UINT;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

/**
 * Thin cross-platform (Winsock/POSIX) wrapper around a socket handle.
 * The class owns the handle, is non-copyable and non-movable, and reports
 * every failure by throwing CSocketException.
 */
class CSocketSTL
{
public:
	// Constructors / Destructors

	/** Creates an object that holds no socket (INVALID_SOCKET). */
	CSocketSTL() noexcept;
	CSocketSTL(const CSocketSTL&) = delete;
	CSocketSTL(CSocketSTL&&) = delete;
	/** Closes the socket if it is still open. */
	virtual ~CSocketSTL() noexcept;
	/** Closes the socket; safe to call repeatedly. */
	void Close() noexcept;

	// Typedefs
#ifdef _UNICODE
	using String = std::wstring;
#else
	using String = std::string;
#endif //#ifdef _UNICODE

#ifndef INVALID_SOCKET
	const int INVALID_SOCKET = -1;
#endif

#ifndef SOCKET_ERROR
	const int SOCKET_ERROR = -1;
#endif

	// Attributes

	/**
	 * Takes ownership of an existing handle, closing any socket already held.
	 * @param hSocket handle to adopt
	 */
	void Attach(SOCKET hSocket) noexcept;

	/**
	 * Releases ownership without closing the socket.
	 * @return the handle that was held (INVALID_SOCKET if none)
	 */
	SOCKET Detach() noexcept;

	/**
	 * Gets the address and port of the remote peer as text.
	 * @param sPeerAddress receives the printable IP address
	 * @param nPeerPort receives the port (host byte order)
	 * @param nAddressToStringFlags unused, kept for MFC compatibility
	 * @throws CSocketException if the call fails or the address family is unsupported
	 */
	void GetPeerName(String& sPeerAddress, UINT& nPeerPort, int nAddressToStringFlags = 0) const;

	/**
	 * Gets the raw address of the remote peer.
	 * @param pSockAddr buffer receiving the address
	 * @param pSockAddrLen in: buffer size, out: address size
	 * @throws CSocketException on failure
	 */
	void GetPeerName(SOCKADDR* pSockAddr, int* pSockAddrLen) const;

	/**
	 * Gets the local address and port of the socket as text.
	 * @param sSocketAddress receives the printable IP address
	 * @param nSocketPort receives the port (host byte order)
	 * @param nAddressToStringFlags unused, kept for MFC compatibility
	 * @throws CSocketException if the call fails or the address family is unsupported
	 */
	void GetSockName(String& sSocketAddress, UINT& nSocketPort, int nAddressToStringFlags = 0) const;

	/**
	 * Gets the raw local address of the socket.
	 * @param pSockAddr buffer receiving the address
	 * @param pSockAddrLen in: buffer size, out: address size
	 * @throws CSocketException on failure
	 */
	void GetSockName(SOCKADDR* pSockAddr, int* pSockAddrLen) const;

	/**
	 * Sets a socket option.
	 * @param nOptionName option identifier (e.g. SO_REUSEADDR)
	 * @param pOptionValue pointer to the new value
	 * @param nOptionLen size of the value in bytes
	 * @param nLevel protocol level (e.g. SOL_SOCKET)
	 * @throws CSocketException on failure
	 */
	void SetSockOpt(int nOptionName, const void* pOptionValue, int nOptionLen, int nLevel) const;

	/**
	 * Gets a socket option.
	 * @param nOptionName option identifier
	 * @param pOptionValue buffer receiving the value
	 * @param pOptionLen in: buffer size, out: value size
	 * @param nLevel protocol level (e.g. SOL_SOCKET)
	 * @throws CSocketException on failure
	 */
	void GetSockOpt(int nOptionName, void* pOptionValue, int* pOptionLen, int nLevel) const;

	/** @return true if the object currently holds a valid socket handle */
	[[nodiscard]] bool IsCreated() const noexcept;

	/**
	 * Waits for the socket to become readable.
	 * @param dwTimeout maximum wait in milliseconds
	 * @return true if data (or a pending connection) is available, false on timeout
	 * @throws CSocketException if polling fails or the handle is not a socket
	 */
	[[nodiscard]] bool IsReadible(DWORD dwTimeout) const;

	/**
	 * Waits for the socket to become writable.
	 * @param dwTimeout maximum wait in milliseconds
	 * @return true if a send will not block, false on timeout
	 * @throws CSocketException if polling fails or the handle is not a socket
	 */
	[[nodiscard]] bool IsWritable(DWORD dwTimeout) const;

	/**
	 * Sets the local IP address used by CreateAndBind().
	 * @param sBindAddress address to bind to; empty selects the wildcard address
	 */
	void SetBindAddress(const String& sBindAddress) { _sBindAddress = sBindAddress; };

	/** @return the local IP address used by CreateAndBind() */
	[[nodiscard]] String GetBindAddress() const { return _sBindAddress; };

	// Methods
	CSocketSTL& operator=(const CSocketSTL&) = delete;
	CSocketSTL& operator=(CSocketSTL&&) = delete;

	/**
	 * Creates a socket using common defaults.
	 * @param bUDP true for a datagram (UDP) socket, false for a stream (TCP) socket
	 * @param bIPv6 true for IPv6, false for IPv4
	 * @throws CSocketException on failure
	 */
	void Create(bool bUDP = false, bool bIPv6 = false);

	/**
	 * Creates a socket with explicit parameters.
	 * @param nSocketType socket type (e.g. SOCK_STREAM)
	 * @param nProtocolType protocol, 0 for the default of the type
	 * @param nAddressFormat address family (e.g. AF_INET)
	 * @throws CSocketException on failure
	 */
	void Create(int nSocketType, int nProtocolType, int nAddressFormat);

	/**
	 * Connects to a remote address, waiting for completion if the connect is asynchronous.
	 * @param pSockAddr remote address
	 * @param nSockAddrLen size of the address in bytes
	 * @throws CSocketException on invalid arguments or connection failure
	 */
	void Connect(const SOCKADDR *pSockAddr, int nSockAddrLen) const;

	/**
	 * Accepts a pending connection.
	 * @param objConnectedSocket receives (and takes ownership of) the new connection
	 * @param pSockAddr optional buffer receiving the peer address
	 * @param pSockAddrLen in: buffer size, out: address size (used only with pSockAddr)
	 * @throws CSocketException on failure
	 */
	void Accept(CSocketSTL& objConnectedSocket, SOCKADDR* pSockAddr = nullptr, int* pSockAddrLen = nullptr) const;

	/**
	 * Creates a socket and binds it to a port, on the address set by SetBindAddress()
	 * or on the wildcard address of the default family.
	 * @param nSocketPort port to bind (0-65535)
	 * @param nSocketType socket type (e.g. SOCK_STREAM)
	 * @param nDefaultAddressFormat AF_INET or AF_INET6, used when no bind address is set
	 * @throws CSocketException if the port is invalid, resolution fails, or no address could be bound
	 */
	void CreateAndBind(UINT nSocketPort, int nSocketType, int nDefaultAddressFormat);

	/**
	 * Binds the socket to a local address.
	 * @param pSockAddr local address
	 * @param nSockAddrLen size of the address in bytes
	 * @throws CSocketException on invalid arguments or failure
	 */
	void Bind(const SOCKADDR* pSockAddr, int nSockAddrLen) const;

	/**
	 * Device control; FIONBIO toggles non-blocking mode on both platforms.
	 * @param lCommand control code
	 * @param pArgument in/out argument of the command
	 * @throws CSocketException if pArgument is null or the call fails
	 */
	void IOCtl(long lCommand, DWORD* pArgument) const;

	/**
	 * Puts the socket in listening state.
	 * @param nConnectionBacklog queue length; a non-positive value selects SOMAXCONN
	 * @throws CSocketException on failure
	 */
	void Listen(int nConnectionBacklog) const;

	/**
	 * Disables sends and/or receives.
	 * @param nHow 0 = receive, 1 = send, 2 = both
	 * @throws CSocketException if nHow is out of range or the call fails
	 */
	void ShutDown(int nHow) const;

	/**
	 * Receives data from a connected socket.
	 * @param pBuffer destination buffer
	 * @param nBufLen buffer size in bytes
	 * @param nFlags recv() flags
	 * @return number of bytes received (0 when the peer closed the connection)
	 * @throws CSocketException on invalid arguments or failure
	 */
	int Receive(void* pBuffer, int nBufLen, int nFlags = 0) const;

	/**
	 * Receives a datagram and returns the sender's raw address.
	 * @param pBuffer destination buffer
	 * @param nBufLen buffer size in bytes
	 * @param pSockAddr optional buffer receiving the sender address
	 * @param pSockAddrLen in: buffer size, out: address size
	 * @param nFlags recvfrom() flags
	 * @return number of bytes received
	 * @throws CSocketException on invalid arguments or failure
	 */
	int ReceiveFrom(void* pBuffer, int nBufLen, SOCKADDR* pSockAddr,  int* pSockAddrLen, int nFlags = 0) const;

	/**
	 * Receives a datagram and returns the sender's address as text.
	 * @param pBuffer destination buffer
	 * @param nBufLen buffer size in bytes
	 * @param strSocketAddress receives the sender's printable IP address
	 * @param nSocketPort receives the sender's port
	 * @param nReceiveFromFlags recvfrom() flags
	 * @param nAddressToStringFlags unused, kept for MFC compatibility
	 * @return number of bytes received
	 * @throws CSocketException on invalid arguments or failure
	 */
	int ReceiveFrom(void* pBuffer, int nBufLen, String& strSocketAddress, UINT& nSocketPort, int nReceiveFromFlags = 0, int nAddressToStringFlags = 0) const;

	/**
	 * Sends data on a connected socket (no SIGPIPE on Linux).
	 * @param pBuffer data to send
	 * @param nBufLen size of the data in bytes
	 * @param nFlags send() flags
	 * @return number of bytes sent
	 * @throws CSocketException on failure
	 */
	int Send(const void* pBuffer, int nBufLen, int nFlags = 0) const;

	/**
	 * Sends a datagram to a raw address.
	 * @param pBuffer data to send
	 * @param nBufLen size of the data in bytes
	 * @param pSockAddr destination address
	 * @param nSockAddrLen size of the address in bytes
	 * @param nFlags sendto() flags
	 * @return number of bytes sent
	 * @throws CSocketException on invalid arguments or failure
	 */
	int SendTo(const void* pBuffer, int nBufLen, const SOCKADDR* pSockAddr, int nSockAddrLen, int nFlags = 0) const;

	/**
	 * Sends a datagram to a host name or IP; each resolved address is tried until one succeeds.
	 * @param pBuffer data to send
	 * @param nBufLen size of the data in bytes
	 * @param nHostPort destination port (0-65535)
	 * @param strHostAddress destination host name or IP address
	 * @param nFlags sendto() flags
	 * @return number of bytes sent
	 * @throws CSocketException if the port is invalid, resolution fails, or every address fails
	 */
	int SendTo(const void* pBuffer, int nBufLen, UINT nHostPort, const String& strHostAddress, int nFlags = 0) const;

protected:
	// Member variables
	SOCKET _hSocket; ///< owned socket handle, INVALID_SOCKET when none
	String _sBindAddress; ///< optional local address for CreateAndBind()
};

#endif // SOCKETSTL_H
