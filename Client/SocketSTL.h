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
    int m_nError;
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

class CSocketSTL
{
public:
    // Constructors / Destructors
    CSocketSTL() noexcept;
    CSocketSTL(const CSocketSTL&) = delete;
    CSocketSTL(CSocketSTL&&) = delete;
    virtual ~CSocketSTL() noexcept;
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
    void Attach(SOCKET hSocket) noexcept;
    SOCKET Detach() noexcept;
    void GetPeerName(String& sPeerAddress, UINT& nPeerPort, int nAddressToStringFlags = 0) const;
    void GetPeerName(SOCKADDR* pSockaddr, int* pSockAddrLen) const;
    void GetSockName(String& sSocketAddress, UINT& nSocketPort, int nAddressToStringFlags = 0) const;
    void GetSockName(SOCKADDR* pSockAddr, int* pSockAddrLen) const;
    void SetSockOpt(int nOptionName, const void* pOptionValue, int nOptionLen, int nLevel) const;
    void GetSockOpt(int nOptionName, void* pOptionValue, int* pOptionLen, int nLevel) const;
    [[nodiscard]] bool IsCreated() const noexcept;
    [[nodiscard]] bool IsReadible(DWORD dwTimeout) const;
    [[nodiscard]] bool IsWritable(DWORD dwTimeout) const;
    void SetBindAddress(const String& sBindAddress) { m_sBindAddress = sBindAddress; };
    [[nodiscard]] String GetBindAddress() const { return m_sBindAddress; };

	// Methods
	CSocketSTL& operator=(const CSocketSTL&) = delete;
	CSocketSTL& operator=(CSocketSTL&&) = delete;
	void Create(bool bUDP = false, bool bIPv6 = false);
	void Create(int nSocketType, int nProtocolType, int nAddressFormat);
    void Connect(const SOCKADDR *pSockAddr, int nSockAddrLen) const;
    void Accept(CSocketSTL& connectedSocket, SOCKADDR* pSockAddr = nullptr, int* pSockAddrLen = nullptr) const;
	void CreateAndBind(UINT nSocketPort, int nSocketType, int nDefaultAddressFormat);
	void Bind(const SOCKADDR* pSockAddr, int nSockAddrLen) const;
	void IOCtl(long lCommand, DWORD* pArgument);
	void Listen(int nConnectionBacklog) const;
	void ShutDown(int nHow) const;
	/*	void CreateAndConnect(_In_z_ LPCTSTR pszHostAddress, _In_ UINT nHostPort, _In_ int nSocketType = SOCK_STREAM, _In_ int nFamily = AF_UNSPEC, _In_ int nProtocolType = 0);
	void CreateAndConnect(_In_z_ LPCTSTR pszHostAddress, _In_z_ LPCTSTR pszPortOrServiceName, _In_ int nSocketType = SOCK_STREAM, _In_ int nFamily = AF_UNSPEC, _In_ int nProtocolType = 0);
	void Connect(_In_reads_bytes_(nSockAddrLen) const SOCKADDR* pSockAddr, _In_ int nSockAddrLen, _In_ DWORD dwTimeout, _In_ bool bResetToBlockingMode = true);
	void CreateAndConnect(_In_z_ LPCTSTR pszHostAddress, _In_ UINT nHostPort, _In_ DWORD dwTimeout, _In_ bool bResetToBlockingMode, _In_ int nSocketType = SOCK_STREAM);
	void CreateAndConnect(_In_z_ LPCTSTR pszHostAddress, _In_z_ LPCTSTR pszPortOrServiceName, _In_ DWORD dwTimeout, _In_ bool bResetToBlockingMode, _In_ int nSocketType = SOCK_STREAM);
	int Receive(_Out_writes_bytes_to_(nBufLen, return) __out_data_source(NETWORK) void* pBuf, _In_ int nBufLen, _In_ int nFlags = 0);
	int ReceiveFrom(_Out_writes_bytes_to_(nBufLen, return) __out_data_source(NETWORK) void* pBuf, _In_ int nBufLen, _Out_writes_bytes_to_opt_(*pSockAddrLen, *pSockAddrLen) SOCKADDR* pSockAddr, _Inout_opt_ int* pSockAddrLen, _In_ int nFlags = 0);
	int ReceiveFrom(_Out_writes_bytes_to_(nBufLen, return) __out_data_source(NETWORK) void* pBuf, _In_ int nBufLen, _Inout_ String& sSocketAddress, _Out_ UINT& nSocketPort, _In_ int nReceiveFromFlags = 0, _In_ int nAddressToStringFlags = 0);
	int Send(_In_reads_bytes_(nBufLen) const void* pBuffer, _In_ int nBufLen, _In_ int nFlags = 0);
	int SendTo(_In_reads_bytes_(nBufLen) const void* pBuf, _In_ int nBufLen, _In_reads_bytes_(nSockAddrLen) const SOCKADDR* pSockAddr, _In_ int nSockAddrLen, _In_ int nFlags = 0);
	int SendTo(_In_reads_bytes_(nBufLen) const void* pBuf, _In_ int nBufLen, _In_ UINT nHostPort, _In_z_ LPCTSTR pszHostAddress = nullptr, _In_ int nFlags = 0);*/

protected:
    // Member variables
    SOCKET m_hSocket;
    String m_sBindAddress;
};

#endif // SOCKETSTL_H
