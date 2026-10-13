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

    //Attributes
    void Attach(SOCKET hSocket) noexcept;
    SOCKET Detach() noexcept;
    void GetPeerName(String& sPeerAddress, UINT& nPeerPort, int nAddressToStringFlags = 0) const;
    void GetPeerName(SOCKADDR* pSockaddr, int* pSockAddrLen) const;
    void GetSockName(String& sSocketAddress, UINT& nSocketPort, int nAddressToStringFlags = 0) const;
    void GetSockName(SOCKADDR* pSockAddr, int* pSockAddrLen) const;
    void SetSockOpt(int nOptionName, const void* pOptionValue, int nOptionLen, int nLevel) const;
    void GetSockOpt(int nOptionName, void* pOptionValue, int* pOptionLen, int nLevel) const;
/*    _NODISCARD bool IsCreated() const noexcept;
    _NODISCARD bool IsReadible(_In_ DWORD dwTimeout);
    _NODISCARD bool IsWritable(_In_ DWORD dwTimeout);
    void SetBindAddress(_In_ const String& sBindAddress) { m_sBindAddress = sBindAddress; };
    _NODISCARD String GetBindAddress() const { return m_sBindAddress; };*/

protected:
    // Member variables
    SOCKET m_hSocket;
    String m_sBindAddress;
};

#endif // SOCKETSTL_H
