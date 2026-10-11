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
    std::string host;
    std::string port;
};

struct CommandLine {
    std::string command;
    std::string input_file;
    std::string output_folder;
    std::string token;
    std::string machine;
};

void print_usage(const char* program) {
    std::cerr << "Usage:\n"
              << "  " << program << " --help\n"
              << "  " << program
              << " <--download|--upload|--delete> --input-file FILE --output-folder FOLDER"
                 " [--token TOKEN] [--machine HOST:PORT]\n"
              << "  TOKEN_ID environment variable is used when --token is not specified.\n"
              << "  IntelliHost.xml is used when --machine is not specified."
              << std::endl;
}

bool is_command(std::string_view command) {
    return command == "--download" || command == "--upload" || command == "--delete";
}

bool parse_command_line(int argc, char* argv[], CommandLine& command_line, std::string& error) {
    if (argc < 2) {
        error = "an action is required";
        return false;
    }

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (is_command(argument)) {
            if (!command_line.command.empty()) {
                error = "only one action may be specified";
                return false;
            }
            command_line.command = argument;
            continue;
        }

        std::string* destination = nullptr;
        if (argument == "--input-file") {
            destination = &command_line.input_file;
        } else if (argument == "--output-folder") {
            destination = &command_line.output_folder;
        } else if (argument == "--token") {
            destination = &command_line.token;
        } else if (argument == "--machine") {
            destination = &command_line.machine;
        } else {
            error = "unknown parameter '" + std::string(argument) + "'";
            return false;
        }

        if (++index == argc || argv[index][0] == '\0' || std::string_view(argv[index]).starts_with("--")) {
            error = "parameter '" + std::string(argument) + "' requires a value";
            return false;
        }
        if (!destination->empty()) {
            error = "parameter '" + std::string(argument) + "' may only be specified once";
            return false;
        }
        *destination = argv[index];
    }

    if (command_line.token.empty()) {
        if (const char* token = std::getenv("TOKEN_ID"); token != nullptr) {
            command_line.token = token;
        }
    }

    if (command_line.command.empty()) {
        error = "an action is required";
    } else if (command_line.input_file.empty()) {
        error = "--input-file is required";
    } else if (command_line.output_folder.empty()) {
        error = "--output-folder is required";
    } else if (command_line.token.empty()) {
        error = "--token or TOKEN_ID environment variable is required";
    }

    return error.empty();
}

bool load_machine_from_configuration(CommandLine& command_line, std::string& error) {
    tinyxml2::XMLDocument document;
    if (document.LoadFile("IntelliHost.xml") != tinyxml2::XML_SUCCESS) {
        error = "unable to load IntelliHost.xml: " + std::string(document.ErrorStr());
        return false;
    }

    const tinyxml2::XMLElement* root = document.FirstChildElement("IntelliHost");
    if (root == nullptr) {
        error = "IntelliHost.xml is missing the IntelliHost element";
        return false;
    }

    const tinyxml2::XMLElement* server_ip = root->FirstChildElement("ServerIP");
    const tinyxml2::XMLElement* server_port = root->FirstChildElement("ServerPort");
    if (server_ip == nullptr || server_ip->GetText() == nullptr || server_port == nullptr ||
        server_port->GetText() == nullptr) {
        error = "IntelliHost.xml must contain non-empty ServerIP and ServerPort elements";
        return false;
    }

    command_line.machine = std::string(server_ip->GetText()) + ':' + server_port->GetText();
    return true;
}

bool parse_endpoint(std::string_view value, Endpoint& endpoint) {
    if (value.empty()) {
        return false;
    }

    const auto separator = value.rfind(':');
    if (separator == std::string_view::npos || separator == 0 || separator == value.size() - 1) {
        return false;
    }

    endpoint.host = value.substr(0, separator);
    endpoint.port = value.substr(separator + 1);
    return endpoint.port.find_first_not_of("0123456789") == std::string::npos;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    CommandLine command_line;
    std::string error;
    if (!parse_command_line(argc, argv, command_line, error)) {
        std::cerr << "error: " << error << std::endl;
        print_usage(argv[0]);
        return -1;
    }

    if (command_line.machine.empty() && !load_machine_from_configuration(command_line, error)) {
        std::cerr << "error: " << error << std::endl;
        return -1;
    }

    Endpoint endpoint;
    if (!parse_endpoint(command_line.machine, endpoint)) {
        std::cerr << "error: machine must be in HOST:PORT form" << std::endl;
        return -1;
    }

    return 0;
}
