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
#include <windows.h>
#include <winsock.h>
#else
#include <cstring>
#include <unistd.h>
#endif

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
