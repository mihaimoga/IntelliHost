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

#endif // SOCKETSTL_H
