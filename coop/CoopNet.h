// Insaniquarium Co-op - minimal non-blocking TCP/UDP layer (Winsock / BSD sockets).
// Everything runs on the game thread: Poll() is called once per update.

#ifndef __COOP_NET_H__
#define __COOP_NET_H__

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace Coop
{
#ifdef _WIN32
	typedef uintptr_t SocketHandle;
#else
	typedef int SocketHandle;
#endif
	extern const SocketHandle kInvalidSocket;

	bool			NetStartup();
	std::string		NetLastError();
	uint32_t		NetMillis();

	struct NetMessage
	{
		uint8_t					mType = 0;
		std::vector<uint8_t>	mData;
	};

	// A framed, buffered, non-blocking TCP connection.
	class Connection
	{
	public:
		enum State { DISCONNECTED, CONNECTING, CONNECTED };

		Connection();
		~Connection();

		bool			StartConnect(const std::string& theHost, uint16_t thePort, std::string& theError);
		void			Adopt(SocketHandle theSocket, const std::string& thePeer);
		void			Close();

		// Pumps the socket: completes a pending connect, flushes queued output and
		// reads whatever arrived. Returns false if the connection just failed/closed.
		bool			Poll();

		void			Send(uint8_t theType, const void* theData, size_t theSize);
		void			Send(uint8_t theType, const std::vector<uint8_t>& theData) { Send(theType, theData.data(), theData.size()); }
		bool			Receive(NetMessage& theMsg);
		// Waits up to theMs for incoming data (or a failure); true if there is some.
		bool			WaitReadable(int theMs);

		State			GetState() const { return mState; }
		bool			IsConnected() const { return mState == CONNECTED; }
		size_t			PendingSendBytes() const { return mOut.size() - mOutPos; }
		const std::string& GetError() const { return mError; }
		const std::string& GetPeer() const { return mPeer; }

		uint64_t		mBytesSent = 0;
		uint64_t		mBytesReceived = 0;

	private:
		void			Fail(const std::string& theWhy);
		bool			FlushOut();
		bool			ReadIn();

		SocketHandle	mSocket;
		State			mState = DISCONNECTED;
		uint32_t		mConnectStart = 0;
		std::string		mError;
		std::string		mPeer;
		std::vector<uint8_t> mOut;
		size_t			mOutPos = 0;
		std::vector<uint8_t> mIn;
		std::deque<NetMessage> mInbox;
	};

	class Listener
	{
	public:
		Listener();
		~Listener();
		bool			Open(uint16_t thePort, std::string& theError);
		void			Close();
		bool			IsOpen() const;
		// Returns kInvalidSocket if nobody is waiting.
		SocketHandle	Accept(std::string& thePeer);

	private:
		SocketHandle	mSocket;
	};

	// LAN discovery: the host broadcasts a small beacon every second; guests
	// listen and list what they hear.
	struct LanGame
	{
		std::string		mAddress;
		uint16_t		mPort = 0;
		std::string		mHostName;
		std::string		mVersion;
		bool			mFull = false;
		uint32_t		mLastSeen = 0;
	};

	class Discovery
	{
	public:
		Discovery();
		~Discovery();
		bool			StartBeacon(uint16_t theGamePort, const std::string& theHostName);
		bool			StartListening();
		void			Stop();
		void			SetFull(bool isFull) { mFull = isFull; }
		void			Poll();
		const std::vector<LanGame>& GetGames() const { return mGames; }

	private:
		void			SendBeacon();
		SocketHandle	mSocket;
		bool			mBeacon = false;
		bool			mListening = false;
		bool			mFull = false;
		uint16_t		mGamePort = 0;
		std::string		mHostName;
		uint32_t		mLastBeacon = 0;
		std::vector<LanGame> mGames;
	};

	// IPv4 addresses of this machine (for "tell your friend to connect to..."),
	// best first: real LAN adapters with a gateway, then Tailscale, then the rest (other
	// VPNs among them). theVpnSeen: a VPN other than Tailscale has an address.
	std::vector<std::string> GetLocalAddresses(bool* theVpnSeen = nullptr);
	bool			IsTailscaleAddress(const std::string& theAddress);
}

#endif
