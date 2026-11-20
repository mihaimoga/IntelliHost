# IntelliHost

IntelliHost is a simpler, flexible, lower-cost SharePoint alternative. It consists of a multi-threaded TCP **server** and a command-line **client** written in C++ (C++26), portable between Linux and Windows.

> **Status:** work in progress. Currently the client can request a token from the server; `--download`, `--upload` and `--delete` are parsed and validated but not implemented yet.

## Layout

| Path | Description |
|------|-------------|
| `Server/` | `IntelliHostServer`: accepts connections, one thread per client |
| `Client/` | `IntelliHostClient` command-line tool and `IntelliTest` socket unit tests |
| `IntelliHost.sql` | MySQL schema (`token`, `filename`, `filedata`) |

Both programs build on Linux and Windows and share `SocketSTL` (cross-platform socket wrapper), `tinyxml2` (configuration) and `SHA256` from `Client/`.

## Building

Requires CMake 3.25+ and a C++26-capable compiler (GCC or Clang on Linux, MSVC on Windows).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build      # runs the IntelliTest socket tests
```

On Windows the targets link `ws2_32` and `secur32` automatically. The CMake files request `cxx_std_26`, so use a recent compiler (and CMake 3.30+ for MSVC).

A CLion project is included, with Debug and Release CMake profiles (`.idea/cmake.xml`).

## Configuration

Both programs read `IntelliHost.xml` from the **current working directory**. CMake copies each file next to its binary (`IntelliHostClient` and `IntelliHostServer`) when the project is configured. Edit the copy in the build directory, or the source file and reconfigure.

**Server** (`Server/IntelliHost.xml`): every element is required.

```xml
<IntelliHost>
    <ServerPort>8080</ServerPort>
    <HostName>localhost</HostName>
    <HostPort>3306</HostPort>
    <Database>MySQL_database</Database>
    <Username>MySQL_username</Username>
    <Password>MySQL_password</Password>
</IntelliHost>
```

`ServerPort` is the port the server listens on; the other elements describe the MySQL database. The server prints the loaded values, including the password, at startup, so avoid sharing its console output.

**Client** (`Client/IntelliHost.xml`):

```xml
<IntelliHost>
    <ServerIP>127.0.0.1</ServerIP>
    <ServerPort>8080</ServerPort>
</IntelliHost>
```

## Usage

Start the server, then run the client:

```sh
./IntelliHostServer

./IntelliHostClient --help
./IntelliHostClient --version
./IntelliHostClient --generate-token
./IntelliHostClient <--download|--upload|--delete> --input-file FILE --output-folder FOLDER \
                    [--token TOKEN] [--machine HOST:PORT]
```

- `--generate-token` prints the user ID, machine ID and UTC time, connects to the server and prints the `TokenID` it returns.
- `--machine HOST:PORT` overrides the server address from `IntelliHost.xml`.
- `--token` overrides the `TOKEN_ID` environment variable.
- `--download`, `--upload` and `--delete` require `--input-file`, `--output-folder` and a token.

## Protocol

The server listens on `ServerPort` (IPv4, TCP) and serves each accepted connection on its own detached thread. Reads wait up to 1 second for data and a failed packet is retried up to 3 times.

Every message is a framed packet:

```
STX | length (2 bytes, big-endian) | payload | ETX | LRC
```

`LRC` is the XOR of the payload bytes. The sender starts with `ENQ` and waits for `ACK`; the receiver answers each packet with `ACK`, or `NAK` when the LRC does not match (up to 3 attempts). Payload strings include their terminating null.

Token generation conversation:

| Step | Client sends | Server replies |
|------|--------------|----------------|
| 1 | `IntelliHost` (protocol identifier) | `OK` |
| 2 | `GenerateToken` | `OK` |
| 3 | user ID, machine ID, UTC date and time (three packets) | |
| 4 | | `TokenID` |

If the server does not recognise the protocol identifier or the command, it replies with an error message (`Error: Unknown protocol identifier` or `Error: Unknown command`) instead of `OK` and ends the conversation. The client prints that message to stderr and exits with a failure status. The server sends the token with a trailing `EOT`, which the client waits for to end the exchange. The token is a fixed-length, 64-character lowercase hexadecimal string (256 bits): an HMAC-SHA256 over the length-prefixed `UserID`, `MachineID` and `DateTime`, keyed with 256 random bits from the operating system's random source. Because the key is random and fresh for every request, the token cannot be predicted or recomputed from its (guessable) inputs; it fits the `VARCHAR(64)` `token` column in `IntelliHost.sql`.

### Diagnostics

The server logs client connections and disconnections to stdout. Failures go to stderr, prefixed `Accept Error:` (a connection could not be accepted; the server keeps running), `Server Error:` (a socket failure; fatal when binding/listening, otherwise it ends only that client's session) or `Error: invalid ServerPort:` (bad port in `IntelliHost.xml`). The client reports connection problems as `Client Error:`.

## Database

`IntelliHost.sql` creates the MySQL tables `token`, `filename` and `filedata`. The server reads the connection settings but does not use the database yet.

## Contributing and security

See [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) and [SECURITY.md](SECURITY.md).

## License

GNU General Public License v3.0 or later. See [LICENSE](LICENSE).
