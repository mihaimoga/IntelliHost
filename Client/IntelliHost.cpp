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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <tinyxml2.h>

namespace {

struct Endpoint {
    std::string strHost;
    std::string strPort;
};

struct CommandLine {
    std::string strCommand;
    std::string strInputFile;
    std::string strOutputFolder;
    std::string strToken;
    std::string strMachine;
};

void vPrintUsage(const char* pszProgram) {
    std::cerr << "Usage:\n"
              << "  " << pszProgram << " --help\n"
              << "  " << pszProgram << " --generate-token\n"
              << "  " << pszProgram
              << " <--download|--upload|--delete> --input-file FILE --output-folder FOLDER"
                 " [--token TOKEN] [--machine HOST:PORT]\n"
              << "  TOKEN_ID environment variable is used when --token is not specified.\n"
              << "  IntelliHost.xml is used when --machine is not specified."
              << std::endl;
}

bool bIsCommand(const std::string_view strCommand) {
    return strCommand == "--generate-token" || strCommand == "--download" || strCommand == "--upload" ||
           strCommand == "--delete";
}

bool bParseCommandLine(const int nArgc, char* ppszArgv[], CommandLine& cmdLine, std::string& strError) {
    if (nArgc < 2) {
        strError = "an action is required";
        return false;
    }

    for (int nIndex = 1; nIndex < nArgc; ++nIndex) {
        const std::string_view strArgument(ppszArgv[nIndex]);
        if (bIsCommand(strArgument)) {
            if (!cmdLine.strCommand.empty()) {
                strError = "only one action may be specified";
                return false;
            }
            cmdLine.strCommand = strArgument;
            continue;
        }

        std::string* pstrDestination = nullptr;
        if (strArgument == "--input-file") {
            pstrDestination = &cmdLine.strInputFile;
        } else if (strArgument == "--output-folder") {
            pstrDestination = &cmdLine.strOutputFolder;
        } else if (strArgument == "--token") {
            pstrDestination = &cmdLine.strToken;
        } else if (strArgument == "--machine") {
            pstrDestination = &cmdLine.strMachine;
        } else {
            strError = "unknown parameter '" + std::string(strArgument) + "'";
            return false;
        }

        if (++nIndex == nArgc || ppszArgv[nIndex][0] == '\0' || std::string_view(ppszArgv[nIndex]).starts_with("--")) {
            strError = "parameter '" + std::string(strArgument) + "' requires a value";
            return false;
        }
        if (!pstrDestination->empty()) {
            strError = "parameter '" + std::string(strArgument) + "' may only be specified once";
            return false;
        }
        *pstrDestination = ppszArgv[nIndex];
    }

    if (cmdLine.strToken.empty()) {
        if (const char* pszToken = std::getenv("TOKEN_ID"); pszToken != nullptr) {
            cmdLine.strToken = pszToken;
        }
    }

    if (cmdLine.strCommand.empty()) {
        strError = "an action is required";
    } else if (cmdLine.strInputFile.empty() && (cmdLine.strCommand != "--generate-token")) {
        strError = "--input-file is required";
    } else if (cmdLine.strOutputFolder.empty() && (cmdLine.strCommand != "--generate-token")) {
        strError = "--output-folder is required";
    } else if (cmdLine.strToken.empty() && (cmdLine.strCommand != "--generate-token")) {
        strError = "--token or TOKEN_ID environment variable is required";
    }

    return strError.empty();
}

bool bLoadMachineFromConfiguration(CommandLine& cmdLine, std::string& strError) {
    tinyxml2::XMLDocument xmlDocument;
    if (xmlDocument.LoadFile("IntelliHost.xml") != tinyxml2::XML_SUCCESS) {
        strError = "unable to load IntelliHost.xml: " + std::string(xmlDocument.ErrorStr());
        return false;
    }

    const tinyxml2::XMLElement* pXmlRoot = xmlDocument.FirstChildElement("IntelliHost");
    if (pXmlRoot == nullptr) {
        strError = "IntelliHost.xml is missing the IntelliHost element";
        return false;
    }

    const tinyxml2::XMLElement* pXmlServerIp = pXmlRoot->FirstChildElement("ServerIP");
    const tinyxml2::XMLElement* pXmlServerPort = pXmlRoot->FirstChildElement("ServerPort");
    if (pXmlServerIp == nullptr || pXmlServerIp->GetText() == nullptr || pXmlServerPort == nullptr ||
        pXmlServerPort->GetText() == nullptr) {
        strError = "IntelliHost.xml must contain non-empty ServerIP and ServerPort elements";
        return false;
    }

    cmdLine.strMachine = std::string(pXmlServerIp->GetText()) + ':' + pXmlServerPort->GetText();
    return true;
}

bool bParseEndpoint(std::string_view strValue, Endpoint& endpointParsed) {
    if (strValue.empty()) {
        return false;
    }

    const auto nSeparator = strValue.rfind(':');
    if (nSeparator == std::string_view::npos || nSeparator == 0 || nSeparator == strValue.size() - 1) {
        return false;
    }

    endpointParsed.strHost = strValue.substr(0, nSeparator);
    endpointParsed.strPort = strValue.substr(nSeparator + 1);
    return endpointParsed.strPort.find_first_not_of("0123456789") == std::string::npos;
}

}  // namespace

int main(const int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        vPrintUsage(argv[0]);
        return 0;
    }

    CommandLine cmdLine;
    std::string strError;
    if (!bParseCommandLine(argc, argv, cmdLine, strError)) {
        std::cerr << "error: " << strError << std::endl;
        vPrintUsage(argv[0]);
        return -1;
    }

    if (cmdLine.strMachine.empty() && !bLoadMachineFromConfiguration(cmdLine, strError)) {
        std::cerr << "error: " << strError << std::endl;
        return -1;
    }

    Endpoint endpointMachine;
    if (!bParseEndpoint(cmdLine.strMachine, endpointMachine)) {
        std::cerr << "error: machine must be in HOST:PORT form" << std::endl;
        return -1;
    }

    return 0;
}
