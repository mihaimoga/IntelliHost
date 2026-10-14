/* Copyright (C) 2026 Stefan-Mihai MOGA
This file is part of IntelliHost application developed by Stefan-Mihai MOGA.
Unit tests for CSocketSTL / CSocketException (self-contained, no external framework).

IntelliHost is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliHost is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliHost. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#include "SocketSTL.h"

#include <cerrno>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
struct TestCase { const char* name; std::function<void()> body; };

std::vector<TestCase>& Registry() { static std::vector<TestCase> r; return r; }
int g_nFailures = 0;

struct Registrar { Registrar(const char* n, std::function<void()> f) { Registry().push_back({ n, std::move(f) }); } };

#define TEST(name) \
    static void name(); \
    static Registrar reg_##name(#name, name); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) { ++g_nFailures; std::cerr << "    FAILED " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_THROWS_ERR(expr, err) \
    do { \
        bool bThrown = false; int nCode = 0; \
        try { expr; } catch (const CSocketException& e) { bThrown = true; nCode = e.m_nError; } \
        if (!bThrown || nCode != (err)) { ++g_nFailures; std::cerr << "    FAILED " << __FILE__ << ":" << __LINE__ << ": " #expr " expected error " #err " got " << (bThrown ? nCode : -1) << "\n"; } \
    } while (0)

#define CHECK_THROWS(expr) \
    do { \
        bool bThrown = false; \
        try { expr; } catch (const CSocketException&) { bThrown = true; } \
        if (!bThrown) { ++g_nFailures; std::cerr << "    FAILED " << __FILE__ << ":" << __LINE__ << ": " #expr " expected CSocketException\n"; } \
    } while (0)

sockaddr_in Loopback(unsigned short port = 0)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

SOCKADDR* AsAddr(sockaddr_in& a) { return reinterpret_cast<SOCKADDR*>(&a); }
const SOCKADDR* AsAddr(const sockaddr_in& a) { return reinterpret_cast<const SOCKADDR*>(&a); }

// Binds `s` to an ephemeral loopback port and returns that port.
unsigned short BindEphemeral(CSocketSTL& s)
{
    sockaddr_in a = Loopback();
    s.Bind(AsAddr(a), static_cast<int>(sizeof(a)));
    sockaddr_in bound{};
    int len = static_cast<int>(sizeof(bound));
    s.GetSockName(AsAddr(bound), &len);
    return ntohs(bound.sin_port);
}

// Creates a connected TCP pair on loopback.
void MakeTcpPair(CSocketSTL& listener, CSocketSTL& client, CSocketSTL& server)
{
    listener.Create();
    const unsigned short port = BindEphemeral(listener);
    listener.Listen(5);
    client.Create();
    sockaddr_in a = Loopback(port);
    client.Connect(AsAddr(a), static_cast<int>(sizeof(a)));
    listener.Accept(server);
}
}

// ---------------------------------------------------------------- CSocketException

TEST(Exception_StoresErrorCode)
{
    CSocketException e(ECONNREFUSED);
    CHECK(e.m_nError == ECONNREFUSED);
}

TEST(Exception_MessageForKnownError)
{
    CSocketException e(ECONNREFUSED);
    std::string msg;
    CHECK(e.GetErrorMessage(msg));
    CHECK(!msg.empty());
}

TEST(Exception_HelpContextIsZeroed)
{
    CSocketException e(EINVAL);
    std::string msg;
    unsigned int ctx = 123;
    CHECK(e.GetErrorMessage(msg, &ctx));
    CHECK(ctx == 0);
}

TEST(Exception_IsStdException)
{
    try { throw CSocketException(EINVAL); }
    catch (const std::exception&) { return; }
    CHECK(false);
}

// ---------------------------------------------------------------- lifetime / attributes

TEST(Socket_DefaultNotCreated)
{
    CSocketSTL s;
    CHECK(!s.IsCreated());
}

TEST(Socket_CreateTcpAndClose)
{
    CSocketSTL s;
    s.Create();
    CHECK(s.IsCreated());
    s.Close();
    CHECK(!s.IsCreated());
    s.Close(); // idempotent
    CHECK(!s.IsCreated());
}

TEST(Socket_CreateUdpIPv4TypeIsDgram)
{
    CSocketSTL s;
    s.Create(true, false);
    int type = 0, len = static_cast<int>(sizeof(type));
    s.GetSockOpt(SO_TYPE, &type, &len, SOL_SOCKET);
    CHECK(type == SOCK_DGRAM);
}

TEST(Socket_CreateTcpTypeIsStream)
{
    CSocketSTL s;
    s.Create(false, false);
    int type = 0, len = static_cast<int>(sizeof(type));
    s.GetSockOpt(SO_TYPE, &type, &len, SOL_SOCKET);
    CHECK(type == SOCK_STREAM);
}

TEST(Socket_CreateIPv6)
{
    CSocketSTL s;
    try { s.Create(false, true); }
    catch (const CSocketException& e) { if (e.m_nError == EAFNOSUPPORT) return; throw; }
    sockaddr_in6 a{};
    int len = static_cast<int>(sizeof(a));
    // unbound socket: family must still be AF_INET6
    s.GetSockName(reinterpret_cast<SOCKADDR*>(&a), &len);
    CHECK(a.sin6_family == AF_INET6);
}

TEST(Socket_CreateInvalidTypeThrows)
{
    CSocketSTL s;
    CHECK_THROWS(s.Create(-1, 0, AF_INET));
    CHECK(!s.IsCreated());
}

TEST(Socket_AttachDetach)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    CSocketSTL s;
    s.Attach(fd);
    CHECK(s.IsCreated());
    CHECK(s.Detach() == fd);
    CHECK(!s.IsCreated());
    CHECK(s.Detach() == -1);
    ::close(fd);
}

TEST(Socket_AttachClosesPrevious)
{
    const int fd1 = ::socket(AF_INET, SOCK_STREAM, 0);
    const int fd2 = ::socket(AF_INET, SOCK_STREAM, 0);
    CSocketSTL s;
    s.Attach(fd1);
    s.Attach(fd2);
    CHECK(::fcntl(fd1, F_GETFD) == -1 && errno == EBADF);
    CHECK(s.Detach() == fd2);
    ::close(fd2);
}

TEST(Socket_DestructorClosesSocket)
{
    int fd = -1;
    {
        CSocketSTL s;
        s.Create();
        fd = s.Detach();
        s.Attach(fd);
    }
    CHECK(::fcntl(fd, F_GETFD) == -1 && errno == EBADF);
}

TEST(Socket_BindAddressAccessors)
{
    CSocketSTL s;
    CHECK(s.GetBindAddress().empty());
    s.SetBindAddress("127.0.0.1");
    CHECK(s.GetBindAddress() == "127.0.0.1");
}

// ---------------------------------------------------------------- options / ioctl

TEST(Socket_SetGetSockOpt)
{
    CSocketSTL s;
    s.Create();
    int on = 1;
    s.SetSockOpt(SO_REUSEADDR, &on, static_cast<int>(sizeof(on)), SOL_SOCKET);
    int val = 0, len = static_cast<int>(sizeof(val));
    s.GetSockOpt(SO_REUSEADDR, &val, &len, SOL_SOCKET);
    CHECK(val != 0);
    CHECK(len == static_cast<int>(sizeof(val)));
}

TEST(Socket_SockOptOnInvalidSocketThrows)
{
    CSocketSTL s;
    int v = 1, len = static_cast<int>(sizeof(v));
    CHECK_THROWS_ERR(s.SetSockOpt(SO_REUSEADDR, &v, len, SOL_SOCKET), EBADF);
    CHECK_THROWS_ERR(s.GetSockOpt(SO_REUSEADDR, &v, &len, SOL_SOCKET), EBADF);
}

TEST(Socket_IOCtlNonBlockingToggle)
{
    CSocketSTL s;
    s.Create();
    DWORD on = 1;
    s.IOCtl(FIONBIO, &on);
    int fd = s.Detach();
    CHECK((::fcntl(fd, F_GETFL) & O_NONBLOCK) != 0);
    s.Attach(fd);
    DWORD off = 0;
    s.IOCtl(FIONBIO, &off);
    fd = s.Detach();
    CHECK((::fcntl(fd, F_GETFL) & O_NONBLOCK) == 0);
    s.Attach(fd);
}

TEST(Socket_IOCtlNullArgumentThrows)
{
    CSocketSTL s;
    s.Create();
    CHECK_THROWS_ERR(s.IOCtl(FIONBIO, nullptr), EFAULT);
}

TEST(Socket_IOCtlInvalidSocketThrows)
{
    CSocketSTL s;
    DWORD v = 1;
    CHECK_THROWS_ERR(s.IOCtl(FIONBIO, &v), EBADF);
}

// ---------------------------------------------------------------- bind / names

TEST(Socket_BindAndGetSockName)
{
    CSocketSTL s;
    s.Create();
    const unsigned short port = BindEphemeral(s);
    CHECK(port != 0);
    CSocketSTL::String addr;
    UINT p = 0;
    s.GetSockName(addr, p);
    CHECK(addr == "127.0.0.1");
    CHECK(p == port);
}

TEST(Socket_BindInvalidArgumentsThrow)
{
    CSocketSTL s;
    s.Create();
    sockaddr_in a = Loopback();
    CHECK_THROWS_ERR(s.Bind(nullptr, static_cast<int>(sizeof(a))), EINVAL);
    CHECK_THROWS_ERR(s.Bind(AsAddr(a), 0), EINVAL);
}

TEST(Socket_BindAddressInUseThrows)
{
    CSocketSTL a, b;
    a.Create();
    const unsigned short port = BindEphemeral(a);
    b.Create();
    sockaddr_in addr = Loopback(port);
    CHECK_THROWS_ERR(b.Bind(AsAddr(addr), static_cast<int>(sizeof(addr))), EADDRINUSE);
}

TEST(Socket_GetPeerNameUnconnectedThrows)
{
    CSocketSTL s;
    s.Create();
    CSocketSTL::String addr;
    UINT p = 0;
    CHECK_THROWS_ERR(s.GetPeerName(addr, p), ENOTCONN);
}

TEST(Socket_CreateAndBindWildcardIPv4)
{
    CSocketSTL s;
    s.CreateAndBind(0, SOCK_STREAM, AF_INET);
    CHECK(s.IsCreated());
    CSocketSTL::String addr;
    UINT p = 0;
    s.GetSockName(addr, p);
    CHECK(addr == "0.0.0.0");
    CHECK(p != 0);
}

TEST(Socket_CreateAndBindUsesBindAddress)
{
    CSocketSTL s;
    s.SetBindAddress("127.0.0.1");
    s.CreateAndBind(0, SOCK_DGRAM, AF_INET);
    CSocketSTL::String addr;
    UINT p = 0;
    s.GetSockName(addr, p);
    CHECK(addr == "127.0.0.1");
}

TEST(Socket_CreateAndBindPortTooLargeThrows)
{
    CSocketSTL s;
    CHECK_THROWS_ERR(s.CreateAndBind(65536, SOCK_STREAM, AF_INET), EINVAL);
    CHECK(!s.IsCreated());
}

TEST(Socket_CreateAndBindUnknownFamilyIsNoOp)
{
    CSocketSTL s;
    s.CreateAndBind(0, SOCK_STREAM, AF_UNSPEC);
    CHECK(!s.IsCreated());
}

TEST(Socket_CreateAndBindBadHostThrows)
{
    CSocketSTL s;
    s.SetBindAddress("999.999.999.999");
    CHECK_THROWS(s.CreateAndBind(0, SOCK_STREAM, AF_INET));
}

// ---------------------------------------------------------------- TCP

TEST(Tcp_ConnectAcceptSendReceive)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);
    CHECK(server.IsCreated());

    const char msg[] = "hello";
    CHECK(client.Send(msg, 5) == 5);
    CHECK(server.IsReadible(1000));
    char buf[16]{};
    CHECK(server.Receive(buf, sizeof(buf)) == 5);
    CHECK(std::memcmp(buf, msg, 5) == 0);

    CHECK(server.Send("ok", 2) == 2);
    CHECK(client.Receive(buf, sizeof(buf)) == 2);
}

TEST(Tcp_PeerAndSockNames)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);

    CSocketSTL::String addr;
    UINT cport = 0, sport = 0;
    client.GetSockName(addr, cport);
    CHECK(addr == "127.0.0.1");
    server.GetPeerName(addr, sport);
    CHECK(addr == "127.0.0.1");
    CHECK(sport == cport);
}

TEST(Tcp_AcceptFillsAddress)
{
    CSocketSTL listener, client;
    listener.Create();
    const unsigned short port = BindEphemeral(listener);
    listener.Listen(1);
    client.Create();
    sockaddr_in target = Loopback(port);
    client.Connect(AsAddr(target), static_cast<int>(sizeof(target)));

    CSocketSTL server;
    sockaddr_in peer{};
    int len = static_cast<int>(sizeof(peer));
    listener.Accept(server, AsAddr(peer), &len);
    CHECK(len == static_cast<int>(sizeof(peer)));
    CHECK(peer.sin_family == AF_INET);
    CHECK(peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
}

TEST(Tcp_ConnectInvalidArgumentsThrow)
{
    CSocketSTL s;
    s.Create();
    sockaddr_in a = Loopback(1);
    CHECK_THROWS_ERR(s.Connect(nullptr, static_cast<int>(sizeof(a))), EINVAL);
    CHECK_THROWS_ERR(s.Connect(AsAddr(a), 0), EINVAL);
}

TEST(Tcp_ConnectRefusedThrows)
{
    // Obtain a port that is certainly closed: bind, then close.
    unsigned short port;
    {
        CSocketSTL tmp;
        tmp.Create();
        port = BindEphemeral(tmp);
    }
    CSocketSTL s;
    s.Create();
    sockaddr_in a = Loopback(port);
    CHECK_THROWS_ERR(s.Connect(AsAddr(a), static_cast<int>(sizeof(a))), ECONNREFUSED);
}

TEST(Tcp_ListenNegativeBacklogUsesDefault)
{
    CSocketSTL s;
    s.Create();
    BindEphemeral(s);
    s.Listen(-1);
    s.Listen(0);
}

TEST(Tcp_ListenInvalidSocketThrows)
{
    CSocketSTL s;
    CHECK_THROWS_ERR(s.Listen(1), EBADF);
}

TEST(Tcp_AcceptNonBlockingNoPendingThrows)
{
    CSocketSTL s;
    s.Create();
    BindEphemeral(s);
    s.Listen(1);
    DWORD on = 1;
    s.IOCtl(FIONBIO, &on);
    CSocketSTL c;
    bool bThrown = false;
    try { s.Accept(c); } catch (const CSocketException& e) { bThrown = (e.m_nError == EAGAIN || e.m_nError == EWOULDBLOCK); }
    CHECK(bThrown);
    CHECK(!c.IsCreated());
}

TEST(Tcp_ReceiveInvalidArgumentsThrow)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);
    char buf[4];
    CHECK_THROWS_ERR(server.Receive(nullptr, 4), EINVAL);
    CHECK_THROWS_ERR(server.Receive(buf, -1), EINVAL);
}

TEST(Tcp_ReceiveReturnsZeroOnPeerClose)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);
    client.Close();
    char buf[4];
    CHECK(server.Receive(buf, sizeof(buf)) == 0);
}

TEST(Tcp_ShutDownModes)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);
    client.ShutDown(1);
    char buf[4];
    CHECK(server.Receive(buf, sizeof(buf)) == 0); // EOF after send-side shutdown
    CHECK_THROWS_ERR(client.Send("x", 1), EPIPE);
    server.ShutDown(2);
}

TEST(Tcp_ShutDownInvalidHowThrows)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);
    CHECK_THROWS_ERR(client.ShutDown(-1), EINVAL);
    CHECK_THROWS_ERR(client.ShutDown(3), EINVAL);
}

TEST(Tcp_ShutDownNotConnectedThrows)
{
    CSocketSTL s;
    s.Create();
    CHECK_THROWS_ERR(s.ShutDown(2), ENOTCONN);
}

TEST(Tcp_SendOnInvalidSocketThrows)
{
    CSocketSTL s;
    CHECK_THROWS_ERR(s.Send("x", 1), EBADF);
}

TEST(Tcp_IsReadibleTimesOutWhenIdle)
{
    CSocketSTL listener, client, server;
    MakeTcpPair(listener, client, server);
    CHECK(!server.IsReadible(10));
    CHECK(client.IsWritable(10));
}

TEST(Poll_UncreatedSocketIsNeverReady)
{
    // poll() ignores negative descriptors, so an uncreated socket simply times out.
    CSocketSTL s;
    CHECK(!s.IsReadible(0));
    CHECK(!s.IsWritable(0));
}

TEST(Poll_StaleDescriptorThrows)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ::close(fd);
    CSocketSTL s;
    s.Attach(fd);
    CHECK_THROWS_ERR((void)s.IsReadible(0), EBADF);
    CHECK_THROWS_ERR((void)s.IsWritable(0), EBADF);
    (void)s.Detach();
}

// ---------------------------------------------------------------- UDP

TEST(Udp_SendToReceiveFromRawAddress)
{
    CSocketSTL rx, tx;
    rx.Create(true);
    const unsigned short port = BindEphemeral(rx);
    tx.Create(true);

    sockaddr_in dst = Loopback(port);
    const char msg[] = "datagram";
    CHECK(tx.SendTo(msg, 8, AsAddr(dst), static_cast<int>(sizeof(dst))) == 8);
    CHECK(rx.IsReadible(1000));

    char buf[32]{};
    sockaddr_in from{};
    int len = static_cast<int>(sizeof(from));
    CHECK(rx.ReceiveFrom(buf, sizeof(buf), AsAddr(from), &len) == 8);
    CHECK(std::memcmp(buf, msg, 8) == 0);
    CHECK(from.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    CHECK(len == static_cast<int>(sizeof(from)));
}

TEST(Udp_SendToHostStringAndReceiveFromString)
{
    CSocketSTL rx, tx;
    rx.Create(true);
    const unsigned short port = BindEphemeral(rx);
    tx.Create(true);

    CHECK(tx.SendTo("abc", 3, port, "127.0.0.1") == 3);
    CHECK(rx.IsReadible(1000));

    char buf[8]{};
    CSocketSTL::String addr;
    UINT srcPort = 0;
    CHECK(rx.ReceiveFrom(buf, sizeof(buf), addr, srcPort) == 3);
    CHECK(addr == "127.0.0.1");
    CHECK(srcPort != 0);
    CHECK(std::string(buf, 3) == "abc");
}

TEST(Udp_ReceiveFromWithoutAddressBuffer)
{
    CSocketSTL rx, tx;
    rx.Create(true);
    const unsigned short port = BindEphemeral(rx);
    tx.Create(true);
    CHECK(tx.SendTo("z", 1, port, "127.0.0.1") == 1);
    CHECK(rx.IsReadible(1000));
    char buf[4];
    CHECK(rx.ReceiveFrom(buf, sizeof(buf), nullptr, nullptr) == 1);
}

TEST(Udp_SendToInvalidArgumentsThrow)
{
    CSocketSTL s;
    s.Create(true);
    sockaddr_in a = Loopback(9);
    const int al = static_cast<int>(sizeof(a));
    CHECK_THROWS_ERR(s.SendTo(nullptr, 1, AsAddr(a), al), EINVAL);
    CHECK_THROWS_ERR(s.SendTo("x", -1, AsAddr(a), al), EINVAL);
    CHECK_THROWS_ERR(s.SendTo("x", 1, nullptr, al), EINVAL);
    CHECK_THROWS_ERR(s.SendTo("x", 1, AsAddr(a), 0), EINVAL);
    CHECK_THROWS_ERR(s.SendTo("x", 1, 65536u, "127.0.0.1"), EINVAL);
}

TEST(Udp_SendToUnresolvableHostThrows)
{
    CSocketSTL s;
    s.Create(true);
    CHECK_THROWS(s.SendTo("x", 1, 9, "999.999.999.999"));
}

TEST(Udp_ReceiveFromInvalidArgumentsThrow)
{
    CSocketSTL s;
    s.Create(true);
    char buf[4];
    CHECK_THROWS_ERR(s.ReceiveFrom(nullptr, 4, nullptr, nullptr), EINVAL);
    CHECK_THROWS_ERR(s.ReceiveFrom(buf, -1, nullptr, nullptr), EINVAL);
}

TEST(Udp_ReceiveFromNonBlockingEmptyThrows)
{
    CSocketSTL s;
    s.Create(true);
    BindEphemeral(s);
    DWORD on = 1;
    s.IOCtl(FIONBIO, &on);
    char buf[4];
    bool bThrown = false;
    try { (void)s.ReceiveFrom(buf, sizeof(buf), nullptr, nullptr); }
    catch (const CSocketException& e) { bThrown = (e.m_nError == EAGAIN || e.m_nError == EWOULDBLOCK); }
    CHECK(bThrown);
}

int main()
{
    int nFailedTests = 0;
    for (const auto& t : Registry())
    {
        const int before = g_nFailures;
        try { t.body(); }
        catch (const std::exception& e) { ++g_nFailures; std::cerr << "    unexpected exception: " << e.what() << "\n"; }
        const bool ok = (g_nFailures == before);
        if (!ok) ++nFailedTests;
        std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << t.name << "\n";
    }
    std::cout << Registry().size() - static_cast<size_t>(nFailedTests) << "/" << Registry().size() << " tests passed\n";
    return nFailedTests == 0 ? 0 : 1;
}
