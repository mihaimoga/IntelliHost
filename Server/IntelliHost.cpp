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

    /// @brief Number of client connections currently being served.
    std::atomic<int> g_nActiveClients{0};

    /// @brief Serves a single client connection.
    ///
    /// Runs on its own thread and owns the socket, which is closed when the client
    /// disconnects or a socket error occurs.
    ///
    /// @param objSocket The accepted connection; ownership is transferred to this function.
    void HandleClient(const std::unique_ptr<CSocketSTL>& objSocket)
    {
        ++g_nActiveClients;
        CSocketSTL::String strPeerAddress;
        UINT nPeerPort = 0;
        try
        {
            objSocket->GetPeerName(strPeerAddress, nPeerPort);
            std::cout << "Client connected: " << std::string(strPeerAddress.begin(), strPeerAddress.end()) << ":" << nPeerPort << std::endl;

            char pszBuffer[4096];
            int nReceived = 0;
            // Receive returns 0 when the client closes the connection
            while ((nReceived = objSocket->Receive(pszBuffer, sizeof(pszBuffer))) > 0)
            {
                // TODO: parse the request and reply to the client
            }
        }
        catch (const CSocketException& ex)
        {
            std::string strError;
            ex.GetErrorMessage(strError);
            std::cerr << "client error: " << strError << std::endl;
        }
        objSocket->Close();
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
