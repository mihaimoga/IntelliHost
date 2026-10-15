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
struct TestCase { const char* pszName; std::function<void()> fnBody; };

std::vector<TestCase>& Registry() { static std::vector<TestCase> vecRegistry; return vecRegistry; }
int g_nFailures = 0;

struct Registrar { Registrar(const char* pszName, std::function<void()> fnBody) { Registry().push_back({ pszName, std::move(fnBody) }); } };

#define TEST(name) \
    static void name(); \
    static Registrar reg_##name(#name, name); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) { ++g_nFailures; std::cerr << "    FAILED " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_THROWS_ERR(expr, err) \
    do { \
        bool bThrown = false; int nCode = 0; \
        try { expr; } catch (const CSocketException& e) { bThrown = true; nCode = e._nError; } \
        if (!bThrown || nCode != (err)) { ++g_nFailures; std::cerr << "    FAILED " << __FILE__ << ":" << __LINE__ << ": " #expr " expected error " #err " got " << (bThrown ? nCode : -1) << "\n"; } \
    } while (0)

#define CHECK_THROWS(expr) \
    do { \
        bool bThrown = false; \
        try { expr; } catch (const CSocketException&) { bThrown = true; } \
        if (!bThrown) { ++g_nFailures; std::cerr << "    FAILED " << __FILE__ << ":" << __LINE__ << ": " #expr " expected CSocketException\n"; } \
    } while (0)

sockaddr_in Loopback(unsigned short nPort = 0)
{
    sockaddr_in sinAddr{};
    sinAddr.sin_family = AF_INET;
    sinAddr.sin_port = htons(nPort);
    sinAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return sinAddr;
}

SOCKADDR* AsAddr(sockaddr_in& sinAddr) { return reinterpret_cast<SOCKADDR*>(&sinAddr); }
const SOCKADDR* AsAddr(const sockaddr_in& sinAddr) { return reinterpret_cast<const SOCKADDR*>(&sinAddr); }

// Binds `objSocket` to an ephemeral loopback nPort and returns that nPort.
unsigned short BindEphemeral(CSocketSTL& objSocket)
{
    sockaddr_in sinAddr = Loopback();
    objSocket.Bind(AsAddr(sinAddr), static_cast<int>(sizeof(sinAddr)));
    sockaddr_in sinBound{};
    int nLen = static_cast<int>(sizeof(sinBound));
    objSocket.GetSockName(AsAddr(sinBound), &nLen);
    return ntohs(sinBound.sin_port);
}

// Creates sinAddr connected TCP pair on loopback.
void MakeTcpPair(CSocketSTL& objListener, CSocketSTL& objClient, CSocketSTL& objServer)
{
    objListener.Create();
    const unsigned short nPort = BindEphemeral(objListener);
    objListener.Listen(5);
    objClient.Create();
    sockaddr_in sinAddr = Loopback(nPort);
    objClient.Connect(AsAddr(sinAddr), static_cast<int>(sizeof(sinAddr)));
    objListener.Accept(objServer);
}
}

// ---------------------------------------------------------------- CSocketException

TEST(Exception_StoresErrorCode)
{
    CSocketException ex(ECONNREFUSED);
    CHECK(ex._nError == ECONNREFUSED);
}

TEST(Exception_MessageForKnownError)
{
    CSocketException ex(ECONNREFUSED);
    std::string strMsg;
    CHECK(ex.GetErrorMessage(strMsg));
    CHECK(!strMsg.empty());
}

TEST(Exception_HelpContextIsZeroed)
{
    CSocketException ex(EINVAL);
    std::string strMsg;
    unsigned int nCtx = 123;
    CHECK(ex.GetErrorMessage(strMsg, &nCtx));
    CHECK(nCtx == 0);
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
    CSocketSTL objSocket;
    CHECK(!objSocket.IsCreated());
}

TEST(Socket_CreateTcpAndClose)
{
    CSocketSTL objSocket;
    objSocket.Create();
    CHECK(objSocket.IsCreated());
    objSocket.Close();
    CHECK(!objSocket.IsCreated());
    objSocket.Close(); // idempotent
    CHECK(!objSocket.IsCreated());
}

TEST(Socket_CreateUdpIPv4TypeIsDgram)
{
    CSocketSTL objSocket;
    objSocket.Create(true, false);
    int nType = 0, nLen = static_cast<int>(sizeof(nType));
    objSocket.GetSockOpt(SO_TYPE, &nType, &nLen, SOL_SOCKET);
    CHECK(nType == SOCK_DGRAM);
}

TEST(Socket_CreateTcpTypeIsStream)
{
    CSocketSTL objSocket;
    objSocket.Create(false, false);
    int nType = 0, nLen = static_cast<int>(sizeof(nType));
    objSocket.GetSockOpt(SO_TYPE, &nType, &nLen, SOL_SOCKET);
    CHECK(nType == SOCK_STREAM);
}

TEST(Socket_CreateIPv6)
{
    CSocketSTL objSocket;
    try { objSocket.Create(false, true); }
    catch (const CSocketException& ex) { if (ex._nError == EAFNOSUPPORT) return; throw; }
    sockaddr_in6 sinAddr{};
    int nLen = static_cast<int>(sizeof(sinAddr));
    // unbound socket: family must still be AF_INET6
    objSocket.GetSockName(reinterpret_cast<SOCKADDR*>(&sinAddr), &nLen);
    CHECK(sinAddr.sin6_family == AF_INET6);
}

TEST(Socket_CreateInvalidTypeThrows)
{
    CSocketSTL objSocket;
    CHECK_THROWS(objSocket.Create(-1, 0, AF_INET));
    CHECK(!objSocket.IsCreated());
}

TEST(Socket_AttachDetach)
{
    const int nFd = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK(nFd >= 0);
    CSocketSTL objSocket;
    objSocket.Attach(nFd);
    CHECK(objSocket.IsCreated());
    CHECK(objSocket.Detach() == nFd);
    CHECK(!objSocket.IsCreated());
    CHECK(objSocket.Detach() == -1);
    ::close(nFd);
}

TEST(Socket_AttachClosesPrevious)
{
    const int nFd1 = ::socket(AF_INET, SOCK_STREAM, 0);
    const int nFd2 = ::socket(AF_INET, SOCK_STREAM, 0);
    CSocketSTL objSocket;
    objSocket.Attach(nFd1);
    objSocket.Attach(nFd2);
    CHECK(::fcntl(nFd1, F_GETFD) == -1 && errno == EBADF);
    CHECK(objSocket.Detach() == nFd2);
    ::close(nFd2);
}

TEST(Socket_DestructorClosesSocket)
{
    int nFd = -1;
    {
        CSocketSTL objSocket;
        objSocket.Create();
        nFd = objSocket.Detach();
        objSocket.Attach(nFd);
    }
    CHECK(::fcntl(nFd, F_GETFD) == -1 && errno == EBADF);
}

TEST(Socket_BindAddressAccessors)
{
    CSocketSTL objSocket;
    CHECK(objSocket.GetBindAddress().empty());
    objSocket.SetBindAddress("127.0.0.1");
    CHECK(objSocket.GetBindAddress() == "127.0.0.1");
}

// ---------------------------------------------------------------- options / ioctl

TEST(Socket_SetGetSockOpt)
{
    CSocketSTL objSocket;
    objSocket.Create();
    int nOn = 1;
    objSocket.SetSockOpt(SO_REUSEADDR, &nOn, static_cast<int>(sizeof(nOn)), SOL_SOCKET);
    int nVal = 0, nLen = static_cast<int>(sizeof(nVal));
    objSocket.GetSockOpt(SO_REUSEADDR, &nVal, &nLen, SOL_SOCKET);
    CHECK(nVal != 0);
    CHECK(nLen == static_cast<int>(sizeof(nVal)));
}

TEST(Socket_SockOptOnInvalidSocketThrows)
{
    CSocketSTL objSocket;
    int nV = 1, nLen = static_cast<int>(sizeof(nV));
    CHECK_THROWS_ERR(objSocket.SetSockOpt(SO_REUSEADDR, &nV, nLen, SOL_SOCKET), EBADF);
    CHECK_THROWS_ERR(objSocket.GetSockOpt(SO_REUSEADDR, &nV, &nLen, SOL_SOCKET), EBADF);
}

TEST(Socket_IOCtlNonBlockingToggle)
{
    CSocketSTL objSocket;
    objSocket.Create();
    DWORD dwOn = 1;
    objSocket.IOCtl(FIONBIO, &dwOn);
    int nFd = objSocket.Detach();
    CHECK((::fcntl(nFd, F_GETFL) & O_NONBLOCK) != 0);
    objSocket.Attach(nFd);
    DWORD dwOff = 0;
    objSocket.IOCtl(FIONBIO, &dwOff);
    nFd = objSocket.Detach();
    CHECK((::fcntl(nFd, F_GETFL) & O_NONBLOCK) == 0);
    objSocket.Attach(nFd);
}

TEST(Socket_IOCtlNullArgumentThrows)
{
    CSocketSTL objSocket;
    objSocket.Create();
    CHECK_THROWS_ERR(objSocket.IOCtl(FIONBIO, nullptr), EFAULT);
}

TEST(Socket_IOCtlInvalidSocketThrows)
{
    CSocketSTL objSocket;
    DWORD dwV = 1;
    CHECK_THROWS_ERR(objSocket.IOCtl(FIONBIO, &dwV), EBADF);
}

// ---------------------------------------------------------------- bind / names

TEST(Socket_BindAndGetSockName)
{
    CSocketSTL objSocket;
    objSocket.Create();
    const unsigned short nPort = BindEphemeral(objSocket);
    CHECK(nPort != 0);
    CSocketSTL::String sAddr;
    UINT nPeerPort = 0;
    objSocket.GetSockName(sAddr, nPeerPort);
    CHECK(sAddr == "127.0.0.1");
    CHECK(nPeerPort == nPort);
}

TEST(Socket_BindInvalidArgumentsThrow)
{
    CSocketSTL objSocket;
    objSocket.Create();
    sockaddr_in sinAddr = Loopback();
    CHECK_THROWS_ERR(objSocket.Bind(nullptr, static_cast<int>(sizeof(sinAddr))), EINVAL);
    CHECK_THROWS_ERR(objSocket.Bind(AsAddr(sinAddr), 0), EINVAL);
}

TEST(Socket_BindAddressInUseThrows)
{
    CSocketSTL objA, objB;
    objA.Create();
    const unsigned short nPort = BindEphemeral(objA);
    objB.Create();
    sockaddr_in sinAddr = Loopback(nPort);
    CHECK_THROWS_ERR(objB.Bind(AsAddr(sinAddr), static_cast<int>(sizeof(sinAddr))), EADDRINUSE);
}

TEST(Socket_GetPeerNameUnconnectedThrows)
{
    CSocketSTL objSocket;
    objSocket.Create();
    CSocketSTL::String sAddr;
    UINT nPeerPort = 0;
    CHECK_THROWS_ERR(objSocket.GetPeerName(sAddr, nPeerPort), ENOTCONN);
}

TEST(Socket_CreateAndBindWildcardIPv4)
{
    CSocketSTL objSocket;
    objSocket.CreateAndBind(0, SOCK_STREAM, AF_INET);
    CHECK(objSocket.IsCreated());
    CSocketSTL::String sAddr;
    UINT nPeerPort = 0;
    objSocket.GetSockName(sAddr, nPeerPort);
    CHECK(sAddr == "0.0.0.0");
    CHECK(nPeerPort != 0);
}

TEST(Socket_CreateAndBindUsesBindAddress)
{
    CSocketSTL objSocket;
    objSocket.SetBindAddress("127.0.0.1");
    objSocket.CreateAndBind(0, SOCK_DGRAM, AF_INET);
    CSocketSTL::String sAddr;
    UINT nPeerPort = 0;
    objSocket.GetSockName(sAddr, nPeerPort);
    CHECK(sAddr == "127.0.0.1");
}

TEST(Socket_CreateAndBindPortTooLargeThrows)
{
    CSocketSTL objSocket;
    CHECK_THROWS_ERR(objSocket.CreateAndBind(65536, SOCK_STREAM, AF_INET), EINVAL);
    CHECK(!objSocket.IsCreated());
}

TEST(Socket_CreateAndBindUnknownFamilyIsNoOp)
{
    CSocketSTL objSocket;
    objSocket.CreateAndBind(0, SOCK_STREAM, AF_UNSPEC);
    CHECK(!objSocket.IsCreated());
}

TEST(Socket_CreateAndBindBadHostThrows)
{
    CSocketSTL objSocket;
    objSocket.SetBindAddress("999.999.999.999");
    CHECK_THROWS(objSocket.CreateAndBind(0, SOCK_STREAM, AF_INET));
}

// ---------------------------------------------------------------- TCP

TEST(Tcp_ConnectAcceptSendReceive)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);
    CHECK(objServer.IsCreated());

    const char strMsg[] = "hello";
    CHECK(objClient.Send(strMsg, 5) == 5);
    CHECK(objServer.IsReadible(1000));
    char acBuf[16]{};
    CHECK(objServer.Receive(acBuf, sizeof(acBuf)) == 5);
    CHECK(std::memcmp(acBuf, strMsg, 5) == 0);

    CHECK(objServer.Send("bOk", 2) == 2);
    CHECK(objClient.Receive(acBuf, sizeof(acBuf)) == 2);
}

TEST(Tcp_PeerAndSockNames)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);

    CSocketSTL::String sAddr;
    UINT nClientPort = 0, nServerPort = 0;
    objClient.GetSockName(sAddr, nClientPort);
    CHECK(sAddr == "127.0.0.1");
    objServer.GetPeerName(sAddr, nServerPort);
    CHECK(sAddr == "127.0.0.1");
    CHECK(nServerPort == nClientPort);
}

TEST(Tcp_AcceptFillsAddress)
{
    CSocketSTL objListener, objClient;
    objListener.Create();
    const unsigned short nPort = BindEphemeral(objListener);
    objListener.Listen(1);
    objClient.Create();
    sockaddr_in sinTarget = Loopback(nPort);
    objClient.Connect(AsAddr(sinTarget), static_cast<int>(sizeof(sinTarget)));

    CSocketSTL objServer;
    sockaddr_in sinPeer{};
    int nLen = static_cast<int>(sizeof(sinPeer));
    objListener.Accept(objServer, AsAddr(sinPeer), &nLen);
    CHECK(nLen == static_cast<int>(sizeof(sinPeer)));
    CHECK(sinPeer.sin_family == AF_INET);
    CHECK(sinPeer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
}

TEST(Tcp_ConnectInvalidArgumentsThrow)
{
    CSocketSTL objSocket;
    objSocket.Create();
    sockaddr_in sinAddr = Loopback(1);
    CHECK_THROWS_ERR(objSocket.Connect(nullptr, static_cast<int>(sizeof(sinAddr))), EINVAL);
    CHECK_THROWS_ERR(objSocket.Connect(AsAddr(sinAddr), 0), EINVAL);
}

TEST(Tcp_ConnectRefusedThrows)
{
    // Obtain sinAddr nPort that is certainly closed: bind, then close.
    unsigned short nPort;
    {
        CSocketSTL objTmp;
        objTmp.Create();
        nPort = BindEphemeral(objTmp);
    }
    CSocketSTL objSocket;
    objSocket.Create();
    sockaddr_in sinAddr = Loopback(nPort);
    CHECK_THROWS_ERR(objSocket.Connect(AsAddr(sinAddr), static_cast<int>(sizeof(sinAddr))), ECONNREFUSED);
}

TEST(Tcp_ListenNegativeBacklogUsesDefault)
{
    CSocketSTL objSocket;
    objSocket.Create();
    BindEphemeral(objSocket);
    objSocket.Listen(-1);
    objSocket.Listen(0);
}

TEST(Tcp_ListenInvalidSocketThrows)
{
    CSocketSTL objSocket;
    CHECK_THROWS_ERR(objSocket.Listen(1), EBADF);
}

TEST(Tcp_AcceptNonBlockingNoPendingThrows)
{
    CSocketSTL objSocket;
    objSocket.Create();
    BindEphemeral(objSocket);
    objSocket.Listen(1);
    DWORD dwOn = 1;
    objSocket.IOCtl(FIONBIO, &dwOn);
    CSocketSTL objC;
    bool bThrown = false;
    try { objSocket.Accept(objC); } catch (const CSocketException& ex) { bThrown = (ex._nError == EAGAIN || ex._nError == EWOULDBLOCK); }
    CHECK(bThrown);
    CHECK(!objC.IsCreated());
}

TEST(Tcp_ReceiveInvalidArgumentsThrow)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);
    char acBuf[4];
    CHECK_THROWS_ERR(objServer.Receive(nullptr, 4), EINVAL);
    CHECK_THROWS_ERR(objServer.Receive(acBuf, -1), EINVAL);
}

TEST(Tcp_ReceiveReturnsZeroOnPeerClose)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);
    objClient.Close();
    char acBuf[4];
    CHECK(objServer.Receive(acBuf, sizeof(acBuf)) == 0);
}

TEST(Tcp_ShutDownModes)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);
    objClient.ShutDown(1);
    char acBuf[4];
    CHECK(objServer.Receive(acBuf, sizeof(acBuf)) == 0); // EOF after send-side shutdown
    CHECK_THROWS_ERR(objClient.Send("x", 1), EPIPE);
    objServer.ShutDown(2);
}

TEST(Tcp_ShutDownInvalidHowThrows)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);
    CHECK_THROWS_ERR(objClient.ShutDown(-1), EINVAL);
    CHECK_THROWS_ERR(objClient.ShutDown(3), EINVAL);
}

TEST(Tcp_ShutDownNotConnectedThrows)
{
    CSocketSTL objSocket;
    objSocket.Create();
    CHECK_THROWS_ERR(objSocket.ShutDown(2), ENOTCONN);
}

TEST(Tcp_SendOnInvalidSocketThrows)
{
    CSocketSTL objSocket;
    CHECK_THROWS_ERR(objSocket.Send("x", 1), EBADF);
}

TEST(Tcp_IsReadibleTimesOutWhenIdle)
{
    CSocketSTL objListener, objClient, objServer;
    MakeTcpPair(objListener, objClient, objServer);
    CHECK(!objServer.IsReadible(10));
    CHECK(objClient.IsWritable(10));
}

TEST(Poll_UncreatedSocketIsNeverReady)
{
    // poll() ignores negative descriptors, so an uncreated socket simply times out.
    CSocketSTL objSocket;
    CHECK(!objSocket.IsReadible(0));
    CHECK(!objSocket.IsWritable(0));
}

TEST(Poll_StaleDescriptorThrows)
{
    const int nFd = ::socket(AF_INET, SOCK_STREAM, 0);
    ::close(nFd);
    CSocketSTL objSocket;
    objSocket.Attach(nFd);
    CHECK_THROWS_ERR((void)objSocket.IsReadible(0), EBADF);
    CHECK_THROWS_ERR((void)objSocket.IsWritable(0), EBADF);
    (void)objSocket.Detach();
}

// ---------------------------------------------------------------- UDP

TEST(Udp_SendToReceiveFromRawAddress)
{
    CSocketSTL objRx, objTx;
    objRx.Create(true);
    const unsigned short nPort = BindEphemeral(objRx);
    objTx.Create(true);

    sockaddr_in sinDst = Loopback(nPort);
    const char strMsg[] = "datagram";
    CHECK(objTx.SendTo(strMsg, 8, AsAddr(sinDst), static_cast<int>(sizeof(sinDst))) == 8);
    CHECK(objRx.IsReadible(1000));

    char acBuf[32]{};
    sockaddr_in sinFrom{};
    int nLen = static_cast<int>(sizeof(sinFrom));
    CHECK(objRx.ReceiveFrom(acBuf, sizeof(acBuf), AsAddr(sinFrom), &nLen) == 8);
    CHECK(std::memcmp(acBuf, strMsg, 8) == 0);
    CHECK(sinFrom.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    CHECK(nLen == static_cast<int>(sizeof(sinFrom)));
}

TEST(Udp_SendToHostStringAndReceiveFromString)
{
    CSocketSTL objRx, objTx;
    objRx.Create(true);
    const unsigned short nPort = BindEphemeral(objRx);
    objTx.Create(true);

    CHECK(objTx.SendTo("abc", 3, nPort, "127.0.0.1") == 3);
    CHECK(objRx.IsReadible(1000));

    char acBuf[8]{};
    CSocketSTL::String sAddr;
    UINT nSrcPort = 0;
    CHECK(objRx.ReceiveFrom(acBuf, sizeof(acBuf), sAddr, nSrcPort) == 3);
    CHECK(sAddr == "127.0.0.1");
    CHECK(nSrcPort != 0);
    CHECK(std::string(acBuf, 3) == "abc");
}

TEST(Udp_ReceiveFromWithoutAddressBuffer)
{
    CSocketSTL objRx, objTx;
    objRx.Create(true);
    const unsigned short nPort = BindEphemeral(objRx);
    objTx.Create(true);
    CHECK(objTx.SendTo("z", 1, nPort, "127.0.0.1") == 1);
    CHECK(objRx.IsReadible(1000));
    char acBuf[4];
    CHECK(objRx.ReceiveFrom(acBuf, sizeof(acBuf), nullptr, nullptr) == 1);
}

TEST(Udp_SendToInvalidArgumentsThrow)
{
    CSocketSTL objSocket;
    objSocket.Create(true);
    sockaddr_in sinAddr = Loopback(9);
    const int nAddrLen = static_cast<int>(sizeof(sinAddr));
    CHECK_THROWS_ERR(objSocket.SendTo(nullptr, 1, AsAddr(sinAddr), nAddrLen), EINVAL);
    CHECK_THROWS_ERR(objSocket.SendTo("x", -1, AsAddr(sinAddr), nAddrLen), EINVAL);
    CHECK_THROWS_ERR(objSocket.SendTo("x", 1, nullptr, nAddrLen), EINVAL);
    CHECK_THROWS_ERR(objSocket.SendTo("x", 1, AsAddr(sinAddr), 0), EINVAL);
    CHECK_THROWS_ERR(objSocket.SendTo("x", 1, 65536u, "127.0.0.1"), EINVAL);
}

TEST(Udp_SendToUnresolvableHostThrows)
{
    CSocketSTL objSocket;
    objSocket.Create(true);
    CHECK_THROWS(objSocket.SendTo("x", 1, 9, "999.999.999.999"));
}

TEST(Udp_ReceiveFromInvalidArgumentsThrow)
{
    CSocketSTL objSocket;
    objSocket.Create(true);
    char acBuf[4];
    CHECK_THROWS_ERR(objSocket.ReceiveFrom(nullptr, 4, nullptr, nullptr), EINVAL);
    CHECK_THROWS_ERR(objSocket.ReceiveFrom(acBuf, -1, nullptr, nullptr), EINVAL);
}

TEST(Udp_ReceiveFromNonBlockingEmptyThrows)
{
    CSocketSTL objSocket;
    objSocket.Create(true);
    BindEphemeral(objSocket);
    DWORD dwOn = 1;
    objSocket.IOCtl(FIONBIO, &dwOn);
    char acBuf[4];
    bool bThrown = false;
    try { (void)objSocket.ReceiveFrom(acBuf, sizeof(acBuf), nullptr, nullptr); }
    catch (const CSocketException& ex) { bThrown = (ex._nError == EAGAIN || ex._nError == EWOULDBLOCK); }
    CHECK(bThrown);
}

int main()
{
    int nFailedTests = 0;
    for (const auto& testCase : Registry())
    {
        const int nBefore = g_nFailures;
        try { testCase.fnBody(); }
        catch (const std::exception& ex) { ++g_nFailures; std::cerr << "    unexpected exception: " << ex.what() << "\n"; }
        const bool bOk = (g_nFailures == nBefore);
        if (!bOk) ++nFailedTests;
        std::cout << (bOk ? "[  OK  ] " : "[ FAIL ] ") << testCase.pszName << "\n";
    }
    std::cout << Registry().size() - static_cast<size_t>(nFailedTests) << "/" << Registry().size() << " tests passed\n";
    return nFailedTests == 0 ? 0 : 1;
}
