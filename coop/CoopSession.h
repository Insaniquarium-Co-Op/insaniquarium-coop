// Insaniquarium Co-op - session: hosting, joining, input injection, streaming.

#ifndef __COOP_SESSION_H__
#define __COOP_SESSION_H__

#include "CoopNet.h"
#include "CoopProtocol.h"
#include "CoopStream.h"
#include "CoopUpnp.h"
#include <SexyAppFramework/CoopHooks.h>
#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/Color.h>
#include <string>
#include <vector>
#include <deque>

namespace Sexy
{
	class WinFishApp;
	class Graphics;
	class Widget;
}

namespace Coop
{
	class RemoteView;

	enum Role { ROLE_NONE, ROLE_HOST, ROLE_GUEST };

	enum Difficulty
	{
		DIFF_CLASSIC = 0,	// original numbers, two players just make it easier
		DIFF_TWO_PLAYER,	// scaled so two players feel the original pressure (default)
		DIFF_INSANE,		// "Insaniquarium" for real
		DIFF_COUNT
	};

	enum GameMode
	{
		// How two players share Adventure and Time Trial (Tank Race and Alien Keeper
		// live in the Versus menu, CoopRace).
		MODE_COOP = 0,		// share everything
		MODE_RIVALS,		// Coin Rivals: separate wallets, race to the egg
		MODE_COUNT
	};
	const char*		GetModeName(int theMode);
	const char*		GetModeBlurb(int theMode);

	struct Tuning
	{
		const char*	mName;
		const char*	mBlurb;
		float		mAlienHealth;		// alien/boss hit points multiplier
		float		mAlienDelay;		// time between invasions multiplier (lower = sooner)
		int			mBonusAlienPct;		// chance an invasion brings a second alien
		float		mHungerRate;		// how fast fish get hungry
		float		mEggPrice;			// egg piece price multiplier
		float		mCoinLife;			// how long dropped coins linger (lower = collect faster)
	};
	const Tuning&	GetTuning(int theDifficulty);

	struct PlayerStats
	{
		int			mCoins = 0;			// coins/pearls/treasure picked up
		int			mMoney = 0;			// value of those
		int			mShots = 0;			// laser shots fired
		int			mAlienHits = 0;		// shots that landed
		int			mFoodDropped = 0;
		int			mPurchases = 0;
		void		Clear() { *this = PlayerStats(); }
	};

	struct Marker
	{
		int			mX, mY, mPlayer, mAge;
	};

	struct Toast
	{
		std::string	mText;
		int			mPlayer;	// -1 = neutral
		int			mAge;
		int			mDuration;	// in updates (100 per second)
	};

	class Session : public Sexy::AppHook, public Sexy::AudioHook
	{
	public:
		static Session&	Get();

		void			Init(Sexy::WinFishApp* theApp);
		void			Shutdown();

		// ---- host ----
		bool			StartHosting(uint16_t thePort, std::string& theError);
		void			StopHosting(const std::string& theReason = "The host stopped the game.");
		bool			IsHosting() const { return mRole == ROLE_HOST; }
		bool			HasGuest() const { return mRole == ROLE_HOST && mHostState == HOST_PLAYING; }
		void			KickGuest();

		// ---- guest ----
		bool			StartJoin(const std::string& theAddress, uint16_t thePort, std::string& theError);
		void			Leave(const std::string& theReason);
		bool			IsGuest() const { return mRole == ROLE_GUEST; }
		bool			IsGuestPlaying() const { return mRole == ROLE_GUEST && mGuestState == GUEST_PLAYING; }

		// ---- shared ----
		Role			GetRole() const { return mRole; }
		std::string		GetStatusLine() const;
		std::string		GetPeerName() const { return mPeerName; }
		int				GetPingMs() const { return mRttMs; }
		std::string		GetLocalName() const;
		Discovery&		GetDiscovery() { return mDiscovery; }
		PortMapper&		GetPortMapper() { return mUpnp; }
		uint16_t		GetPort() const { return mPort; }

		// ---- gameplay (host side) ----
		// 0 = player 1 (host), 1 = player 2 (guest) for the input being handled now.
		int				CurrentPlayer() const { return mCurrentPlayer; }
		bool			CoopActive() const { return HasGuest() && !IsRacing(); }
		int				GetDifficulty() const { return mDifficulty; }
		void			SetDifficulty(int theDifficulty);
		int				GetMode() const { return mMode; }
		void			SetMode(int theMode);
		const Tuning&	ActiveTuning() const;	// classic numbers when solo
		PlayerStats&	Stats(int thePlayer) { return mStats[thePlayer & 1]; }
		void			ResetLevelStats();
		// Pointer state of a player for hold-to-feed / hold-to-fire.
		bool			GetPlayerPointer(int thePlayer, int& theX, int& theY, bool& theLeftDown);
		void			AddMarker(int thePlayer, int theX, int theY);
		void			AddToast(const std::string& theText, int thePlayer = -1, int theDuration = 400);
		static Sexy::Color PlayerColor(int thePlayer);
		std::string		PlayerName(int thePlayer) const;

		// ---- AppHook ----
		virtual void	PreUpdateFrames() override;
		virtual void	PreDrawScreen() override;
		virtual void	PostDrawScreen() override;
		virtual bool	AllowLostFocusPause() override { return mRole == ROLE_NONE; }
		virtual bool	KeepRunningWhenMinimized() override { return HasGuest() || IsRacing(); }
		virtual bool	IdleWait(int theMs) override;
		uint64_t		NetBytesSent() const { return mConn.mBytesSent; }
		uint64_t		NetBytesReceived() const { return mConn.mBytesReceived; }

		// ---- AudioHook (host) ----
		virtual void	OnSoundPlay(uint32_t theSerial, int theSfxId, double theVolume, int thePan, float thePitch, bool looping) override;
		virtual void	OnSoundStop(uint32_t theSerial) override;
		virtual void	OnSoundVolume(uint32_t, double) override {}
		virtual void	OnSoundPan(uint32_t, int) override {}
		virtual void	OnSoundPitch(uint32_t, float) override {}
		virtual void	OnMusic(int theOp, int theSongId, int theOffset, double theValue, bool theFlag) override;

		// ---- guest view callbacks ----
		void			GuestInput(uint8_t theKind, int a, int b, int c);
		void			DrawGuestView(Sexy::Graphics* g);
		bool			GuestWantsLeaveDialog();

		void			Log(const char* theFmt, ...);
		// Command-line conveniences (-host / -join=<address>), run once the menu is up.
		void			QueueAutoHost() { mAutoHost = true; }
		void			QueueAutoJoin(const std::string& theAddress) { mAutoJoin = theAddress; }
		void			CancelAutoJoin() { mAutoJoinAddr.clear(); mAutoJoinRetryAt = 0; }
		void			OnImageDestroyed(Sexy::Image* theImage);
		void			TestGuestClick(int theX, int theY);	// test harness: a left click from player 2

		// ---- Tank Race plumbing ----
		bool			IsConnected() const { return HasGuest() || IsGuestPlaying(); }
		bool			IsRacing() const;
		int				LocalPlayer() const { return mRole == ROLE_GUEST ? 1 : 0; }
		void			SendMsg(uint8_t theType, const std::vector<uint8_t>& theData);
		void			SuspendGuestView();		// guest: stop showing the host's screen (own tank)
		void			ResumeGuestView();		// guest: back to watching the host
		void			SuspendStreaming(bool theSuspend);	// host: no frames/audio while racing

		// ---- Alien Keeper: the fish keeper streams their tank to the alien keeper,
		// whichever of them hosts ----
		void			StartKeeperView();		// watch the other computer's tank (asks it to stream)
		void			StopKeeperView();
		void			StopKeeperStream();		// fish keeper: stop sending
		bool			KeeperSending() const { return mKeeperSending; }
		bool			KeeperViewing() const { return mKeeperViewing; }
		// The alien keeper's live view is only on screen while Tab is held; the lair keeps
		// its own music, and the fish tank's sounds play only while the view shows.
		void			SetKeeperViewShown(bool theShown);
		bool			KeeperViewShown() const { return mKeeperViewShown; }
		void			DrawToasts(Sexy::Graphics* g, int theTop, int theCenterX = 320);

	private:
		enum HostState { HOST_IDLE, HOST_WAITING, HOST_HANDSHAKE, HOST_PLAYING };
		enum GuestState { GUEST_IDLE, GUEST_CONNECTING, GUEST_HANDSHAKE, GUEST_PLAYING };

		void			PollHost();
		void			PollGuest();
		void			HostHandleMessage(NetMessage& theMsg);
		void			GuestHandleMessage(NetMessage& theMsg);
		void			GuestLeft(const std::string& theReason);
		void			InjectGuestEvent(ByteReader& r);
		void			FlushAudio();
		void			SendMusicSnapshot();
		void			SendPing();
		void			LogReceiveStats();
		void			DrawSharedOverlay(Sexy::Graphics* g);
		void			PostDrawOverlays();
		void			DrawRemoteCursor(Sexy::Graphics* g, int x, int y, int theCursor, int thePlayer);
		void			UpdateOverlayTimers();
		void			PlayGuestAudio(ByteReader& r);
		void			EnterGuestView();
		void			ExitGuestView(const std::string& theReason);
		void			FlushGuestMove();
		void			ShowInfo(const std::string& theTitle, const std::string& theText);
		void			TryBeginFrame(int theViewerCursor);
		void			FinishFrame();
		void			HandleFrame(NetMessage& theMsg);
		void			HandleFrameAck(ByteReader& r);
		void			HandleNeedPixels(ByteReader& r);
		void			HandleStreamStart(ByteReader& r);
		void			SendStreamStart();

		Sexy::WinFishApp* mApp = nullptr;
		Role			mRole = ROLE_NONE;
		HostState		mHostState = HOST_IDLE;
		GuestState		mGuestState = GUEST_IDLE;
		Connection		mConn;
		Listener		mListener;
		Discovery		mDiscovery;
		PortMapper		mUpnp;
		uint16_t		mPort = kDefaultPort;
		std::string		mPeerName;
		std::string		mPeerVersion;
		uint32_t		mStateSince = 0;

		// host streaming
		FrameEncoder	mEncoder;
		uint32_t		mFrameSeq = 0;
		uint32_t		mAckedSeq = 0;
		uint32_t		mLastFrameSentMs = 0;
		bool			mCapturing = false;
		std::vector<uint8_t> mFrameBuf;
		ByteWriter		mAudioOut;
		Sexy::MouseState mGuestMouse;
		int				mGuestCursor = 0;
		int				mCurrentPlayer = 0;
		int				mDifficulty = DIFF_TWO_PLAYER;
		int				mMode = MODE_COOP;
		PlayerStats		mStats[2];
		uint32_t		mBytesAtLastRate = 0;
		uint32_t		mRateStart = 0;
		float			mKbps = 0;

		// guest
		FrameDecoder	mDecoder;
		RemoteView*		mView = nullptr;
		bool			mKeeperSending = false;
		bool			mKeeperViewing = false;
		bool			mKeeperViewShown = false;
		bool			mMovePending = false;
		int				mMoveX = 0, mMoveY = 0;
		uint32_t		mLastFrameRecvMs = 0;
		uint32_t		mFramesShown = 0;		// frames decoded (IdleWait draws new ones at once)
		// INSANIQ_COOPSTATS on the receiving side: frames, worst gap and ping samples per 2 s
		uint32_t		mRecvStatStart = 0, mRecvStatFrames = 0, mRecvStatMaxGap = 0, mRecvStatBytes = 0;
		int				mRecvStatRttMin = -1, mRecvStatRttMax = -1;
		int				mLastAppliedCursor = -1;
		std::string		mPendingLeaveReason;

		// both
		int				mRttMs = -1;
		uint32_t		mLastPingSent = 0;
		uint32_t		mLastHeard = 0;
		std::deque<Marker> mMarkers;
		std::deque<Toast> mToasts;
		FILE*			mLogFile = nullptr;
		bool			mTestHarness = false;
		bool			mShuttingDown = false;
		uint32_t		mAudioSent = 0, mAudioPlayed = 0, mMusicOps = 0;
		bool			mAutoHost = false;
		std::string		mAutoJoin;
		std::string		mAutoJoinAddr;		// -join keeps retrying for a while (host may still be starting)
		uint32_t		mAutoJoinDeadline = 0;
		uint32_t		mAutoJoinRetryAt = 0;
	};

	inline Session& S() { return Session::Get(); }
}

#endif
