/*
 * IntelliHost Linux client.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

struct Endpoint {
    std::string host;
    std::string port;
};

void print_usage(const char* program) {
    std::cerr << "Usage: " << program
              << " <download|upload|delete> SOURCE-FILE DESTINATION-FOLDER "
                 "--token TOKEN --machine HOST:PORT" << std::endl;
}

bool is_command(std::string_view command) {
    return command == "download" || command == "upload" || command == "delete";
}

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
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

class Socket {
public:
    Socket() = default;
    explicit Socket(int descriptor) : descriptor_(descriptor) {}

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : descriptor_(other.descriptor_) {
        other.descriptor_ = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            descriptor_ = other.descriptor_;
            other.descriptor_ = -1;
        }
        return *this;
    }

    ~Socket() {
        close();
    }

    [[nodiscard]] bool valid() const {
        return descriptor_ >= 0;
    }

    [[nodiscard]] int descriptor() const {
        return descriptor_;
    }

private:
    void close() {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
            descriptor_ = -1;
        }
    }

    int descriptor_ = -1;
};

Socket connect_to_server(const Endpoint& endpoint, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const int lookup_status = ::getaddrinfo(endpoint.host.c_str(), endpoint.port.c_str(), &hints, &result);
    if (lookup_status != 0) {
        error = ::gai_strerror(lookup_status);
        return {};
    }

    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(result, ::freeaddrinfo);
    for (const addrinfo* address = addresses.get(); address != nullptr; address = address->ai_next) {
        Socket socket(::socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (!socket.valid()) {
            error = std::strerror(errno);
            continue;
        }

        const timeval timeout{10, 0};
        if (::setsockopt(socket.descriptor(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
            ::setsockopt(socket.descriptor(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
            error = std::strerror(errno);
            continue;
        }

        if (::connect(socket.descriptor(), address->ai_addr, address->ai_addrlen) == 0) {
            return socket;
        }
        error = std::strerror(errno);
    }

    return {};
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    if (argc != 8 || std::string_view(argv[4]) != "--token" || std::string_view(argv[6]) != "--machine") {
        print_usage(argv[0]);
        return 2;
    }

    const std::string command = to_lower(argv[1]);
    if (!is_command(command)) {
        std::cerr << "error: unsupported command '" << argv[1] << "'" << std::endl;
        print_usage(argv[0]);
        return 2;
    }

    Endpoint endpoint;
    if (!parse_endpoint(argv[7], endpoint)) {
        std::cerr << "error: --machine must be in HOST:PORT form" << std::endl;
        return 2;
    }

    std::string error;
    Socket socket = connect_to_server(endpoint, error);
    if (!socket.valid()) {
        std::cerr << "error: cannot connect to " << endpoint.host << ':' << endpoint.port << ": " << error << std::endl;
        return 1;
    }

    std::cout << "Connected to " << endpoint.host << ':' << endpoint.port << " for " << command << '.' << std::endl;
    return 0;
}
