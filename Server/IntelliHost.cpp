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

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
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

namespace {
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
}

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

    return EXIT_SUCCESS;
}
