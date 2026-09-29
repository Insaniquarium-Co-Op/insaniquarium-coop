#include "CoopNet.h"
#include "CoopProtocol.h"

#include <SDL.h>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
typedef int socklen_t;
#define COOP_CLOSESOCKET closesocket
#define COOP_WOULDBLOCK(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS || (e) == WSAEALREADY)
#define COOP_SENDFLAGS 0
static int SockErr() { return WSAGetLastError(); }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#define COOP_CLOSESOCKET close
#define COOP_WOULDBLOCK(e) ((e) == EWOULDBLOCK || (e) == EAGAIN || (e) == EINPROGRESS || (e) == EALREADY)
#ifdef MSG_NOSIGNAL
#define COOP_SENDFLAGS MSG_NOSIGNAL
#else
#define COOP_SENDFLAGS 0
#endif
static int SockErr() { return errno; }
#endif

namespace Coop
{
#ifdef _WIN32
	const SocketHandle kInvalidSocket = (SocketHandle)INVALID_SOCKET;
#else
	const SocketHandle kInvalidSocket = -1;
#endif

	static const uint32_t kConnectTimeoutMs = 8000;

	static std::string ErrText(int theErr)
	{
#ifdef _WIN32
		char aBuf[256] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, theErr, 0, aBuf, sizeof(aBuf) - 1, nullptr);
		std::string s = aBuf;
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == '.'))
			s.pop_back();
		if (s.empty())
			s = "network error " + std::to_string(theErr);
		return s;
#else
		return strerror(theErr);
#endif
	}

	std::string NetLastError() { return ErrText(SockErr()); }

	uint32_t NetMillis() { return SDL_GetTicks(); }

	bool NetStartup()
	{
		static bool sStarted = false;
		if (sStarted)
			return true;
#ifdef _WIN32
		WSADATA aData;
		if (WSAStartup(MAKEWORD(2, 2), &aData) != 0)
			return false;
#else
		// A peer vanishing mid-send must be an error code, not a crash. Linux
		// uses MSG_NOSIGNAL per send; macOS has no such flag.
		signal(SIGPIPE, SIG_IGN);
#endif
		sStarted = true;
		return true;
	}

	static void SetNonBlocking(SocketHandle s)
	{
#ifdef _WIN32
		u_long aMode = 1;
		ioctlsocket((SOCKET)s, FIONBIO, &aMode);
#else
		int aFlags = fcntl(s, F_GETFL, 0);
		fcntl(s, F_SETFL, aFlags | O_NONBLOCK);
#endif
	}

	static void TuneStream(SocketHandle s)
	{
		int aOne = 1;
		setsockopt((decltype(socket(0,0,0)))s, IPPROTO_TCP, TCP_NODELAY, (const char*)&aOne, sizeof(aOne));
		int aBuf = 512 * 1024;
		setsockopt((decltype(socket(0,0,0)))s, SOL_SOCKET, SO_SNDBUF, (const char*)&aBuf, sizeof(aBuf));
		setsockopt((decltype(socket(0,0,0)))s, SOL_SOCKET, SO_RCVBUF, (const char*)&aBuf, sizeof(aBuf));
	}

	#define RAW(s) ((decltype(socket(0,0,0)))(s))

	///////////////////////////////////////////////////////////////////////////
	Connection::Connection() : mSocket(kInvalidSocket) {}
	Connection::~Connection() { Close(); }

	void Connection::Close()
	{
		if (mSocket != kInvalidSocket)
			COOP_CLOSESOCKET(RAW(mSocket));
		mSocket = kInvalidSocket;
		mState = DISCONNECTED;
		mOut.clear();
		mOutPos = 0;
		mIn.clear();
		mInbox.clear();
	}

	void Connection::Fail(const std::string& theWhy)
	{
		if (mError.empty())
			mError = theWhy;
		if (mSocket != kInvalidSocket)
			COOP_CLOSESOCKET(RAW(mSocket));
		mSocket = kInvalidSocket;
		mState = DISCONNECTED;
	}

	bool Connection::StartConnect(const std::string& theHost, uint16_t thePort, std::string& theError)
	{
		Close();
		mError.clear();
		NetStartup();

		addrinfo aHints;
		memset(&aHints, 0, sizeof(aHints));
		aHints.ai_family = AF_INET;
		aHints.ai_socktype = SOCK_STREAM;
		addrinfo* aResult = nullptr;
		std::string aPort = std::to_string(thePort);
		if (getaddrinfo(theHost.c_str(), aPort.c_str(), &aHints, &aResult) != 0 || aResult == nullptr)
		{
			theError = "Couldn't find \"" + theHost + "\". Check the address.";
			return false;
		}

		SocketHandle s = (SocketHandle)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == kInvalidSocket)
		{
			freeaddrinfo(aResult);
			theError = "Couldn't create a socket: " + NetLastError();
			return false;
		}
		SetNonBlocking(s);
		TuneStream(s);

		int r = connect(RAW(s), aResult->ai_addr, (socklen_t)aResult->ai_addrlen);
		char aAddr[64] = { 0 };
		inet_ntop(AF_INET, &((sockaddr_in*)aResult->ai_addr)->sin_addr, aAddr, sizeof(aAddr));
		freeaddrinfo(aResult);
		if (r != 0 && !COOP_WOULDBLOCK(SockErr()))
		{
			theError = "Couldn't connect: " + NetLastError();
			COOP_CLOSESOCKET(RAW(s));
			return false;
		}

		mSocket = s;
		mPeer = std::string(aAddr) + ":" + aPort;
		mState = (r == 0) ? CONNECTED : CONNECTING;
		mConnectStart = NetMillis();
		return true;
	}

	void Connection::Adopt(SocketHandle theSocket, const std::string& thePeer)
	{
		Close();
		mError.clear();
		mSocket = theSocket;
		mPeer = thePeer;
		SetNonBlocking(mSocket);
		TuneStream(mSocket);
		mState = CONNECTED;
	}

	void Connection::Send(uint8_t theType, const void* theData, size_t theSize)
	{
		if (mState == DISCONNECTED)
			return;
		// Compact the already-sent prefix once it dominates the buffer.
		if (mOutPos > 0 && mOutPos * 2 > mOut.size())
		{
			mOut.erase(mOut.begin(), mOut.begin() + mOutPos);
			mOutPos = 0;
		}
		uint32_t aLen = (uint32_t)theSize + 1;
		uint8_t aHdr[5] = { (uint8_t)(aLen & 0xFF), (uint8_t)((aLen >> 8) & 0xFF), (uint8_t)((aLen >> 16) & 0xFF), (uint8_t)(aLen >> 24), theType };
		mOut.insert(mOut.end(), aHdr, aHdr + 5);
		const uint8_t* p = (const uint8_t*)theData;
		if (theSize > 0)
			mOut.insert(mOut.end(), p, p + theSize);
	}

	bool Connection::FlushOut()
	{
		while (mOutPos < mOut.size())
		{
			size_t aChunk = std::min<size_t>(mOut.size() - mOutPos, 256 * 1024);
			int n = send(RAW(mSocket), (const char*)mOut.data() + mOutPos, (int)aChunk, COOP_SENDFLAGS);
			if (n > 0)
			{
				mOutPos += n;
				mBytesSent += n;
				continue;
			}
			int e = SockErr();
			if (n < 0 && COOP_WOULDBLOCK(e))
				break;
			Fail("Connection lost (" + ErrText(e) + ")");
			return false;
		}
		if (mOutPos == mOut.size())
		{
			mOut.clear();
			mOutPos = 0;
		}
		return true;
	}

	bool Connection::ReadIn()
	{
		uint8_t aBuf[64 * 1024];
		std::string aFailure;
		for (int aLoops = 0; aLoops < 64; aLoops++)
		{
			int n = recv(RAW(mSocket), (char*)aBuf, sizeof(aBuf), 0);
			if (n > 0)
			{
				mIn.insert(mIn.end(), aBuf, aBuf + n);
				mBytesReceived += n;
				continue;
			}
			if (n == 0)
			{
				aFailure = "The other player closed the connection.";
				break;
			}
			int e = SockErr();
			if (!COOP_WOULDBLOCK(e))
				aFailure = "Connection lost (" + ErrText(e) + ")";
			break;
		}

		// Parse everything that arrived, even when the peer hung up right after
		// sending it (its goodbye message says why).
		size_t aPos = 0;
		while (mIn.size() - aPos >= 5)
		{
			uint32_t aLen = mIn[aPos] | (mIn[aPos + 1] << 8) | (mIn[aPos + 2] << 16) | ((uint32_t)mIn[aPos + 3] << 24);
			if (aLen == 0 || aLen > kMaxMessageSize)
			{
				Fail("Received a corrupt message.");
				return false;
			}
			if (mIn.size() - aPos - 4 < aLen)
				break;
			NetMessage aMsg;
			aMsg.mType = mIn[aPos + 4];
			aMsg.mData.assign(mIn.begin() + aPos + 5, mIn.begin() + aPos + 4 + aLen);
			mInbox.push_back(std::move(aMsg));
			aPos += 4 + aLen;
		}
		if (aPos > 0)
			mIn.erase(mIn.begin(), mIn.begin() + aPos);

		if (!aFailure.empty())
		{
			Fail(aFailure);
			return false;
		}
		return true;
	}

	bool Connection::Poll()
	{
		if (mState == DISCONNECTED)
			return false;

		if (mState == CONNECTING)
		{
			fd_set aWrite, aErr;
			FD_ZERO(&aWrite);
			FD_ZERO(&aErr);
			FD_SET(RAW(mSocket), &aWrite);
			FD_SET(RAW(mSocket), &aErr);
			timeval aTv = { 0, 0 };
			int r = select((int)mSocket + 1, nullptr, &aWrite, &aErr, &aTv);
			if (r > 0)
			{
				int aSoErr = 0;
				socklen_t aLen = sizeof(aSoErr);
				getsockopt(RAW(mSocket), SOL_SOCKET, SO_ERROR, (char*)&aSoErr, &aLen);
				if (aSoErr != 0 || FD_ISSET(RAW(mSocket), &aErr))
				{
					Fail("Couldn't reach the host (" + ErrText(aSoErr ? aSoErr : SockErr()) + ").");
					return false;
				}
				mState = CONNECTED;
			}
			else if (NetMillis() - mConnectStart > kConnectTimeoutMs)
			{
				Fail("Timed out reaching the host. Is the address right, and is the host's firewall/port open?");
				return false;
			}
			else
				return true;
		}

		if (!FlushOut())
			return false;
		if (!ReadIn())
			return false;
		return true;
	}

	bool Connection::Receive(NetMessage& theMsg)
	{
		if (mInbox.empty())
			return false;
		theMsg = std::move(mInbox.front());
		mInbox.pop_front();
		return true;
	}

	///////////////////////////////////////////////////////////////////////////
	Listener::Listener() : mSocket(kInvalidSocket) {}
	Listener::~Listener() { Close(); }
	bool Listener::IsOpen() const { return mSocket != kInvalidSocket; }

	void Listener::Close()
	{
		if (mSocket != kInvalidSocket)
			COOP_CLOSESOCKET(RAW(mSocket));
		mSocket = kInvalidSocket;
	}

	bool Listener::Open(uint16_t thePort, std::string& theError)
	{
		Close();
		NetStartup();
		SocketHandle s = (SocketHandle)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == kInvalidSocket)
		{
			theError = "Couldn't create a socket: " + NetLastError();
			return false;
		}
#ifndef _WIN32
		int aOne = 1;
		setsockopt(RAW(s), SOL_SOCKET, SO_REUSEADDR, (const char*)&aOne, sizeof(aOne));
#endif
		sockaddr_in anAddr;
		memset(&anAddr, 0, sizeof(anAddr));
		anAddr.sin_family = AF_INET;
		anAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		anAddr.sin_port = htons(thePort);
		if (bind(RAW(s), (sockaddr*)&anAddr, sizeof(anAddr)) != 0)
		{
			theError = "Port " + std::to_string(thePort) + " is busy (is another copy of the game hosting?)";
			COOP_CLOSESOCKET(RAW(s));
			return false;
		}
		if (listen(RAW(s), 4) != 0)
		{
			theError = "Couldn't listen: " + NetLastError();
			COOP_CLOSESOCKET(RAW(s));
			return false;
		}
		SetNonBlocking(s);
		mSocket = s;
		return true;
	}

	SocketHandle Listener::Accept(std::string& thePeer)
	{
		if (mSocket == kInvalidSocket)
			return kInvalidSocket;
		sockaddr_in anAddr;
		socklen_t aLen = sizeof(anAddr);
		auto s = accept(RAW(mSocket), (sockaddr*)&anAddr, &aLen);
		if ((SocketHandle)s == kInvalidSocket)
			return kInvalidSocket;
		char aBuf[64] = { 0 };
		inet_ntop(AF_INET, &anAddr.sin_addr, aBuf, sizeof(aBuf));
		thePeer = aBuf;
		return (SocketHandle)s;
	}

	///////////////////////////////////////////////////////////////////////////
	static std::vector<uint32_t> GetBroadcastAddresses()
	{
		std::vector<uint32_t> aList;
		aList.push_back(INADDR_BROADCAST);
		aList.push_back(INADDR_LOOPBACK);
#ifdef _WIN32
		ULONG aSize = 16 * 1024;
		std::vector<uint8_t> aBuf(aSize);
		PIP_ADAPTER_ADDRESSES anAdapters = (PIP_ADAPTER_ADDRESSES)aBuf.data();
		if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, anAdapters, &aSize) == ERROR_BUFFER_OVERFLOW)
		{
			aBuf.resize(aSize);
			anAdapters = (PIP_ADAPTER_ADDRESSES)aBuf.data();
			if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, anAdapters, &aSize) != NO_ERROR)
				return aList;
		}
		for (PIP_ADAPTER_ADDRESSES a = anAdapters; a != nullptr; a = a->Next)
		{
			if (a->OperStatus != IfOperStatusUp)
				continue;
			for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u != nullptr; u = u->Next)
			{
				if (u->Address.lpSockaddr->sa_family != AF_INET)
					continue;
				uint32_t anIp = ntohl(((sockaddr_in*)u->Address.lpSockaddr)->sin_addr.s_addr);
				int aPrefix = u->OnLinkPrefixLength;
				if (aPrefix <= 0 || aPrefix >= 32)
					continue;
				uint32_t aMask = aPrefix == 0 ? 0 : (0xFFFFFFFFu << (32 - aPrefix));
				aList.push_back(htonl(anIp | ~aMask));
			}
		}
#else
		ifaddrs* anIfs = nullptr;
		if (getifaddrs(&anIfs) == 0)
		{
			for (ifaddrs* i = anIfs; i != nullptr; i = i->ifa_next)
			{
				if (i->ifa_addr == nullptr || i->ifa_addr->sa_family != AF_INET)
					continue;
				if ((i->ifa_flags & IFF_BROADCAST) && i->ifa_broadaddr != nullptr)
					aList.push_back(((sockaddr_in*)i->ifa_broadaddr)->sin_addr.s_addr);
			}
			freeifaddrs(anIfs);
		}
#endif
		for (uint32_t& a : aList)
			if (a == INADDR_BROADCAST || a == INADDR_LOOPBACK)
				a = htonl(a);
		std::sort(aList.begin(), aList.end());
		aList.erase(std::unique(aList.begin(), aList.end()), aList.end());
		return aList;
	}

	bool IsTailscaleAddress(const std::string& s)
	{
		unsigned a = 0, b = 0, c = 0, d = 0;
		if (sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
			return false;
		return a == 100 && b >= 64 && b <= 127;
	}

	std::vector<std::string> GetLocalAddresses()
	{
		struct Candidate { std::string mAddr; int mRank; };
		std::vector<Candidate> aList;
		auto Rank = [](const std::string& s, bool hasGateway) {
			if (IsTailscaleAddress(s))
				return 1;
			bool isPrivate = s.rfind("192.168.", 0) == 0 || s.rfind("10.", 0) == 0 || s.rfind("172.", 0) == 0;
			if (isPrivate && hasGateway)
				return 0;
			return isPrivate ? 2 : 3;
		};
		NetStartup();
#ifdef _WIN32
		const ULONG kFlags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS;
		ULONG aSize = 16 * 1024;
		std::vector<uint8_t> aBuf(aSize);
		PIP_ADAPTER_ADDRESSES anAdapters = (PIP_ADAPTER_ADDRESSES)aBuf.data();
		ULONG r = GetAdaptersAddresses(AF_INET, kFlags, nullptr, anAdapters, &aSize);
		if (r == ERROR_BUFFER_OVERFLOW)
		{
			aBuf.resize(aSize);
			anAdapters = (PIP_ADAPTER_ADDRESSES)aBuf.data();
			r = GetAdaptersAddresses(AF_INET, kFlags, nullptr, anAdapters, &aSize);
		}
		if (r == NO_ERROR)
		{
			for (PIP_ADAPTER_ADDRESSES a = anAdapters; a != nullptr; a = a->Next)
			{
				if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
					continue;
				bool hasGateway = a->FirstGatewayAddress != nullptr;
				for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u != nullptr; u = u->Next)
				{
					if (u->Address.lpSockaddr->sa_family != AF_INET)
						continue;
					char s[64] = { 0 };
					inet_ntop(AF_INET, &((sockaddr_in*)u->Address.lpSockaddr)->sin_addr, s, sizeof(s));
					if (strncmp(s, "169.254.", 8) != 0)
						aList.push_back({ s, Rank(s, hasGateway) });
				}
			}
		}
#else
		ifaddrs* anIfs = nullptr;
		if (getifaddrs(&anIfs) == 0)
		{
			for (ifaddrs* i = anIfs; i != nullptr; i = i->ifa_next)
			{
				if (i->ifa_addr == nullptr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK))
					continue;
				char s[64] = { 0 };
				inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, s, sizeof(s));
				// No cheap gateway test here; treat broadcast-capable adapters as real LANs.
				aList.push_back({ s, Rank(s, (i->ifa_flags & IFF_BROADCAST) != 0) });
			}
			freeifaddrs(anIfs);
		}
#endif
		std::stable_sort(aList.begin(), aList.end(), [](const Candidate& a, const Candidate& b) { return a.mRank < b.mRank; });
		std::vector<std::string> aResult;
		for (const Candidate& c : aList)
			if (std::find(aResult.begin(), aResult.end(), c.mAddr) == aResult.end())
				aResult.push_back(c.mAddr);
		return aResult;
	}

	///////////////////////////////////////////////////////////////////////////
	Discovery::Discovery() : mSocket(kInvalidSocket) {}
	Discovery::~Discovery() { Stop(); }

	void Discovery::Stop()
	{
		if (mSocket != kInvalidSocket)
			COOP_CLOSESOCKET(RAW(mSocket));
		mSocket = kInvalidSocket;
		mBeacon = false;
		mListening = false;
		mGames.clear();
	}

	bool Discovery::StartBeacon(uint16_t theGamePort, const std::string& theHostName)
	{
		Stop();
		NetStartup();
		SocketHandle s = (SocketHandle)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == kInvalidSocket)
			return false;
		int aOne = 1;
		setsockopt(RAW(s), SOL_SOCKET, SO_BROADCAST, (const char*)&aOne, sizeof(aOne));
		SetNonBlocking(s);
		mSocket = s;
		mBeacon = true;
		mGamePort = theGamePort;
		mHostName = theHostName;
		mLastBeacon = 0;
		return true;
	}

	bool Discovery::StartListening()
	{
		Stop();
		NetStartup();
		SocketHandle s = (SocketHandle)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == kInvalidSocket)
			return false;
		int aOne = 1;
		setsockopt(RAW(s), SOL_SOCKET, SO_REUSEADDR, (const char*)&aOne, sizeof(aOne));
#ifdef SO_REUSEPORT
		setsockopt(RAW(s), SOL_SOCKET, SO_REUSEPORT, (const char*)&aOne, sizeof(aOne));
#endif
		sockaddr_in anAddr;
		memset(&anAddr, 0, sizeof(anAddr));
		anAddr.sin_family = AF_INET;
		anAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		anAddr.sin_port = htons(kDiscoveryPort);
		if (bind(RAW(s), (sockaddr*)&anAddr, sizeof(anAddr)) != 0)
		{
			COOP_CLOSESOCKET(RAW(s));
			return false;
		}
		SetNonBlocking(s);
		mSocket = s;
		mListening = true;
		return true;
	}

	void Discovery::SendBeacon()
	{
		ByteWriter w;
		w.Bytes(kMagic, 8);
		w.U16(kProtocolVersion);
		w.U16(mGamePort);
		w.Str(mHostName);
		w.Str(COOP_VERSION);
		w.U8(mFull ? 1 : 0);
		sockaddr_in aTo;
		memset(&aTo, 0, sizeof(aTo));
		aTo.sin_family = AF_INET;
		aTo.sin_port = htons(kDiscoveryPort);
		for (uint32_t aBcast : GetBroadcastAddresses())
		{
			aTo.sin_addr.s_addr = aBcast;
			sendto(RAW(mSocket), (const char*)w.mData.data(), (int)w.mData.size(), 0, (sockaddr*)&aTo, sizeof(aTo));
		}
	}

	void Discovery::Poll()
	{
		if (mSocket == kInvalidSocket)
			return;
		uint32_t aNow = NetMillis();
		if (mBeacon)
		{
			if (mLastBeacon == 0 || aNow - mLastBeacon >= 1000)
			{
				mLastBeacon = aNow;
				SendBeacon();
			}
			return;
		}
		if (!mListening)
			return;

		uint8_t aBuf[1024];
		for (int i = 0; i < 32; i++)
		{
			sockaddr_in aFrom;
			socklen_t aLen = sizeof(aFrom);
			int n = recvfrom(RAW(mSocket), (char*)aBuf, sizeof(aBuf), 0, (sockaddr*)&aFrom, &aLen);
			if (n <= 0)
				break;
			ByteReader r(aBuf, n);
			const uint8_t* aMagic = r.Bytes(8);
			if (aMagic == nullptr || memcmp(aMagic, kMagic, 8) != 0)
				continue;
			uint16_t aProto = r.U16();
			uint16_t aPort = r.U16();
			std::string aName = r.Str();
			std::string aVer = r.Str();
			bool isFull = r.U8() != 0;
			if (r.mError || aProto != kProtocolVersion)
				continue;
			char anIp[64] = { 0 };
			inet_ntop(AF_INET, &aFrom.sin_addr, anIp, sizeof(anIp));
			std::string anAddr = anIp;
			if (anAddr.rfind("127.", 0) == 0)
				anAddr = "127.0.0.1";
			auto it = std::find_if(mGames.begin(), mGames.end(), [&](const LanGame& g) { return g.mHostName == aName && g.mPort == aPort && (g.mAddress == anAddr || g.mAddress == "127.0.0.1" || anAddr == "127.0.0.1"); });
			if (it == mGames.end())
			{
				mGames.push_back(LanGame());
				it = mGames.end() - 1;
				it->mAddress = anAddr;
			}
			else if (it->mAddress == "127.0.0.1" && anAddr != "127.0.0.1")
				it->mAddress = anAddr;	// prefer a routable address for display
			it->mPort = aPort;
			it->mHostName = aName;
			it->mVersion = aVer;
			it->mFull = isFull;
			it->mLastSeen = aNow;
		}
		mGames.erase(std::remove_if(mGames.begin(), mGames.end(), [&](const LanGame& g) { return aNow - g.mLastSeen > 3500; }), mGames.end());
	}
}
