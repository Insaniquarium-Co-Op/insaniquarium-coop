#include <SexyAppFramework/Font.h>
#include <SexyAppFramework/MemoryImage.h>
#include <SexyAppFramework/Graphics.h>
#include "CoopSession.h"
#include "CoopUpdate.h"
#include "CoopGame.h"
#include "CoopRace.h"
#include "CoopHeroes.h"
#include "CoopView.h"
#include "CoopTest.h"
#include "CoopUI.h"

#include "WinFishApp.h"
#include "Board.h"
#include "ProfileMgr.h"
#include "Res.h"

#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Font.h>
#include <SexyAppFramework/SoundManager.h>
#include <SexyAppFramework/SoundInstance.h>
#include <SexyAppFramework/MusicInterface.h>
#include <SexyAppFramework/SDLMusicInterface.h>
#include <SexyAppFramework/Dialog.h>
#include <SexyAppFramework/KeyCodes.h>
#include <SexyAppFramework/Common.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <algorithm>

using namespace Sexy;

namespace Coop
{
	static const int kDialogCoopInfo = 91;

	static const Tuning kTunings[DIFF_COUNT] =
	{
		{ "Classic",        "The original numbers. With two of you it's a breeze.",          1.00f, 1.00f,  0, 1.00f, 1.00f, 1.00f },
		{ "Double Trouble", "Tougher, more frequent aliens and hungrier fish. Built for two.", 1.60f, 0.80f, 25, 1.15f, 1.00f, 1.00f },
		{ "Insane",         "Twice the aliens, twice the hunger, pricier eggs. Good luck!",   2.20f, 0.60f, 55, 1.35f, 1.20f, 0.85f },
	};

	const Tuning& GetTuning(int theDifficulty)
	{
		if (theDifficulty < 0 || theDifficulty >= DIFF_COUNT)
			theDifficulty = DIFF_TWO_PLAYER;
		return kTunings[theDifficulty];
	}

	const char* GetModeName(int theMode)
	{
		return theMode == MODE_RIVALS ? "Coin Rivals" : "Co-op";
	}

	const char* GetModeBlurb(int theMode)
	{
		if (theMode == MODE_RIVALS)
			return "Rivals: own wallets, race to the egg!";
		return "Co-op: one tank, one wallet, one team.";
	}

	Session& Session::Get()
	{
		static Session sSession;
		return sSession;
	}

	static void ImageDestroyed(Image* theImage);

	void Session::Init(WinFishApp* theApp)
	{
		mApp = theApp;
		gAppHook = this;
		gImageDestroyedHook = &ImageDestroyed;
		NetStartup();
		mTestHarness = TestHarnessActive();

		std::string aPath = GetAppDataFolder() + "coop_log.txt";
		mLogFile = fopen(aPath.c_str(), "w");
		Log("Insaniquarium Co-op %s (protocol %d) starting", COOP_VERSION, kProtocolVersion);

		int aSaved = 0;
		if (mApp->RegistryReadInteger("CoopDifficulty", &aSaved) && aSaved >= 0 && aSaved < DIFF_COUNT)
			mDifficulty = aSaved;
		if (mApp->RegistryReadInteger("CoopMode", &aSaved) && aSaved >= 0 && aSaved < MODE_COUNT)
			mMode = aSaved;
	}

	void Session::Shutdown()
	{
		mShuttingDown = true;
		if (mRole == ROLE_HOST)
			StopHosting("The host closed the game.");
		else if (mRole == ROLE_GUEST)
			Leave("");
		for (int i = 0; i < 5; i++)
			mConn.Poll();
		mConn.Close();
		mDecoder.Reset();	// frees streamed images while the app still exists
		gAppHook = nullptr;
		gAudioHook = nullptr;
		gDrawRecorder = nullptr;
		gImageDestroyedHook = nullptr;
		if (mLogFile)
		{
			fclose(mLogFile);
			mLogFile = nullptr;
		}
	}

	static void ImageDestroyed(Image* theImage)
	{
		Session::Get().OnImageDestroyed(theImage);
	}

	void Session::OnImageDestroyed(Image* theImage)
	{
		mEncoder.OnImageDestroyed(theImage);
		mDecoder.OnImageDestroyed(theImage);
	}

	void Session::Log(const char* theFmt, ...)
	{
		char aBuf[2048];
		va_list anArgs;
		va_start(anArgs, theFmt);
		vsnprintf(aBuf, sizeof(aBuf), theFmt, anArgs);
		va_end(anArgs);
		uint32_t t = NetMillis();
		if (mLogFile)
		{
			fprintf(mLogFile, "[%7u.%03u] %s\n", t / 1000, t % 1000, aBuf);
			fflush(mLogFile);
		}
		if (getenv("INSANIQ_COOPLOG"))
			fprintf(stderr, "[coop %u] %s\n", t, aBuf);
	}

	std::string Session::GetLocalName() const
	{
		if (mApp != nullptr && mApp->mCurrentProfile != nullptr && !mApp->mCurrentProfile->mUserName.empty())
			return mApp->mCurrentProfile->mUserName;
		return "Player";
	}

	std::string Session::PlayerName(int thePlayer) const
	{
		if (mRole == ROLE_HOST)
			return thePlayer == 0 ? GetLocalName() : (mPeerName.empty() ? "Player 2" : mPeerName);
		if (mRole == ROLE_GUEST)
			return thePlayer == 0 ? (mPeerName.empty() ? "Player 1" : mPeerName) : GetLocalName();
		return thePlayer == 0 ? GetLocalName() : "Player 2";
	}

	Color Session::PlayerColor(int thePlayer)
	{
		return thePlayer == 0 ? Color(255, 200, 40) : Color(60, 230, 255);
	}

	void Session::SetDifficulty(int theDifficulty)
	{
		if (theDifficulty < 0 || theDifficulty >= DIFF_COUNT)
			return;
		mDifficulty = theDifficulty;
		if (mApp)
			mApp->RegistryWriteInteger("CoopDifficulty", theDifficulty);
		if (HasGuest())
			AddToast(std::string("Difficulty: ") + GetTuning(theDifficulty).mName);
	}

	void Session::SetMode(int theMode)
	{
		if (theMode < 0 || theMode >= MODE_COUNT || theMode == mMode)
			return;
		mMode = theMode;
		if (mApp)
			mApp->RegistryWriteInteger("CoopMode", theMode);
		Log("Game: mode set to %s", GetModeName(theMode));
		// Wallets are set up when a level starts, so a switch mid-level waits.
		if (HasGuest() && mApp && mApp->mBoard != nullptr)
			AddToast(std::string(GetModeName(theMode)) + " starts with the next level");
		else if (HasGuest())
			AddToast(std::string("Mode: ") + GetModeName(theMode));
	}

	const Tuning& Session::ActiveTuning() const
	{
		if (CoopActive())
			return GetTuning(mDifficulty);
		return kTunings[DIFF_CLASSIC];
	}

	void Session::ResetLevelStats()
	{
		mStats[0].Clear();
		mStats[1].Clear();
	}

	std::string Session::GetStatusLine() const
	{
		char aBuf[256];
		switch (mRole)
		{
		case ROLE_HOST:
			if (mHostState == HOST_PLAYING)
				snprintf(aBuf, sizeof(aBuf), "Playing with %s  (%d ms)", mPeerName.c_str(), mRttMs < 0 ? 0 : mRttMs);
			else if (mHostState == HOST_HANDSHAKE)
				snprintf(aBuf, sizeof(aBuf), "%s is joining...", mConn.GetPeer().c_str());
			else
				snprintf(aBuf, sizeof(aBuf), "Waiting for player 2 on port %d", mPort);
			return aBuf;
		case ROLE_GUEST:
			if (mGuestState == GUEST_PLAYING)
				snprintf(aBuf, sizeof(aBuf), "Playing in %s's tank  (%d ms)", mPeerName.c_str(), mRttMs < 0 ? 0 : mRttMs);
			else
				snprintf(aBuf, sizeof(aBuf), "Connecting to %s...", mConn.GetPeer().c_str());
			return aBuf;
		default:
			return "Not connected";
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Hosting
	///////////////////////////////////////////////////////////////////////////
	bool Session::StartHosting(uint16_t thePort, std::string& theError)
	{
		if (mRole == ROLE_GUEST)
			Leave("");
		if (mRole == ROLE_HOST)
			return true;
		if (!mListener.Open(thePort, theError))
		{
			Log("Host: listen on %d failed: %s", thePort, theError.c_str());
			return false;
		}
		mPort = thePort;
		mDiscovery.StartBeacon(thePort, GetLocalName());
		if (getenv("INSANIQ_NO_UPNP") == nullptr)
			mUpnp.Start(thePort);
		mRole = ROLE_HOST;
		mHostState = HOST_WAITING;
		mStateSince = NetMillis();
		Log("Host: listening on port %d", thePort);
		return true;
	}

	void Session::StopHosting(const std::string& theReason)
	{
		if (mRole != ROLE_HOST)
			return;
		if (mHostState == HOST_PLAYING || mHostState == HOST_HANDSHAKE)
		{
			ByteWriter w;
			w.Str(theReason);
			mConn.Send(MSG_BYE, w.mData);
			for (int i = 0; i < 3; i++)
				mConn.Poll();
		}
		GuestLeft("");
		mListener.Close();
		mDiscovery.Stop();
		mUpnp.Stop();
		mRole = ROLE_NONE;
		mHostState = HOST_IDLE;
		Log("Host: stopped (%s)", theReason.c_str());
	}

	void Session::KickGuest()
	{
		if (!HasGuest())
			return;
		ByteWriter w;
		w.Str("The host removed you from the game.");
		mConn.Send(MSG_BYE, w.mData);
		mConn.Poll();
		GuestLeft("removed by host");
	}

	void Session::GuestLeft(const std::string& theReason)
	{
		bool wasPlaying = mHostState == HOST_PLAYING;
		std::string aName = mPeerName;
		if (wasPlaying)
		{
			RivalsGuestLeft();
			RacePeerLost(aName + " left the race.");
			HeroesPeerLost(aName + " left the match.");
		}
		mConn.Close();
		if (mApp && mApp->mWidgetManager)
		{
			WidgetManager* wm = mApp->mWidgetManager;
			// Release anything the guest was holding down.
			if (mGuestMouse.mLastDownWidget != nullptr || mGuestMouse.mDownButtons != 0)
			{
				wm->SwapMouseState(mGuestMouse);
				gRemoteDispatch = true;
				wm->DoMouseUps();
				gRemoteDispatch = false;
				wm->SwapMouseState(mGuestMouse);
			}
			if (mGuestMouse.mOverWidget != nullptr)
				mGuestMouse.mOverWidget->mIsOver = false;
			wm->UnregisterAltMouseState(&mGuestMouse);
		}
		mGuestMouse = MouseState();
		gAudioHook = nullptr;
		if (gDrawRecorder == &mEncoder)
			gDrawRecorder = nullptr;
		mEncoder.AbortFrame();
		mCapturing = false;
		mAudioOut.Clear();
		mRttMs = -1;
		mHostState = mListener.IsOpen() ? HOST_WAITING : HOST_IDLE;
		mDiscovery.SetFull(false);
		if (wasPlaying)
		{
			Log("Host: guest '%s' left (%s); %u frames, %u sounds sent", aName.c_str(), theReason.c_str(), mFrameSeq, mAudioSent);
			if (!mShuttingDown)
				mApp->PlaySample(SOUND_BUTTONCLICK);
			if (!theReason.empty())
				AddToast(aName + " left the tank (" + theReason + ")", 1);
			else
				AddToast(aName + " left the tank", 1);
		}
		mPeerName.clear();
	}

	void Session::PollHost()
	{
		uint32_t aNow = NetMillis();
		mDiscovery.SetFull(mHostState != HOST_WAITING);
		mDiscovery.Poll();

		for (;;)
		{
			std::string aPeer;
			SocketHandle s = mListener.Accept(aPeer);
			if (s == kInvalidSocket)
				break;
			if (mHostState == HOST_WAITING)
			{
				mConn.Adopt(s, aPeer);
				mHostState = HOST_HANDSHAKE;
				mStateSince = aNow;
				mLastHeard = aNow;
				Log("Host: connection from %s", aPeer.c_str());
			}
			else
			{
				Connection aBusy;
				aBusy.Adopt(s, aPeer);
				ByteWriter w;
				w.Str("This tank already has two players.");
				aBusy.Send(MSG_REJECT, w.mData);
				aBusy.Poll();
				aBusy.Close();
				Log("Host: turned away %s (full)", aPeer.c_str());
			}
		}

		if (mHostState != HOST_HANDSHAKE && mHostState != HOST_PLAYING)
			return;

		if (!mConn.Poll())
		{
			std::string aWhy = "connection lost";
			NetMessage aLast;
			while (mConn.Receive(aLast))
				if (aLast.mType == MSG_BYE)
					aWhy = "";
			GuestLeft(aWhy);
			return;
		}

		NetMessage aMsg;
		while (mConn.Receive(aMsg))
		{
			HostHandleMessage(aMsg);
			if (mHostState != HOST_HANDSHAKE && mHostState != HOST_PLAYING)
				return;
		}

		if (mHostState == HOST_HANDSHAKE && (int32_t)(NetMillis() - mStateSince) > 15000)
		{
			Log("Host: handshake timed out");
			GuestLeft("");
			return;
		}
		if (mHostState == HOST_PLAYING)
		{
			if ((int32_t)(NetMillis() - mLastPingSent) >= 1000)
				SendPing();
			if ((int32_t)(NetMillis() - mLastHeard) > 20000)
			{
				GuestLeft("lost contact");
				return;
			}
			if (aNow - mRateStart >= 2000)
			{
				mKbps = (float)((mConn.mBytesSent - mBytesAtLastRate) * 8.0 / (aNow - mRateStart));
				mBytesAtLastRate = (uint32_t)mConn.mBytesSent;
				mRateStart = aNow;
			}
		}
	}

	void Session::SendPing()
	{
		ByteWriter w;
		w.U32(NetMillis());
		mConn.Send(MSG_PING, w.mData);
		mLastPingSent = NetMillis();
	}

	// The version this copy tells its partner (test scripts can pretend: "fakeversion").
	static std::string gFakeVersion;
	void SetFakeVersion(const std::string& theVersion) { gFakeVersion = theVersion; }
	std::string LocalVersion() { return gFakeVersion.empty() ? std::string(COOP_VERSION) : gFakeVersion; }

	// Both players need the same version; say who should update.
	static std::string VersionMismatchText(const std::string& theHost, const std::string& theGuest)
	{
		std::string s = "The host runs Insaniquarium Co-op " + theHost + " and you run " + theGuest + ". ";
		int c = CompareVersions(theHost, theGuest);
		if (c > 0)
			s += "Update to " + theHost + " to join.";
		else if (c < 0)
			s += "The host needs to update to " + theGuest + ".";
		else
			s += "Use the same version.";
		return s;
	}

	// Versions that still play together (same protocol) but aren't the same release. Toasts
	// are drawn on the shared overlay, so the note names both players.
	static std::string PeerVersionNote(const std::string& theHost, const std::string& theHostVersion,
		const std::string& theGuest, const std::string& theGuestVersion)
	{
		bool hostNewer = CompareVersions(theHostVersion, theGuestVersion) > 0;
		return theHost + " has version " + theHostVersion + ", " + theGuest + " has " + theGuestVersion + ": " +
			(hostNewer ? theGuest : theHost) + " should update.";
	}

	void Session::HostHandleMessage(NetMessage& theMsg)
	{
		mLastHeard = NetMillis();
		ByteReader r(theMsg.mData.data(), theMsg.mData.size());
		switch (theMsg.mType)
		{
		case MSG_HELLO:
		{
			const uint8_t* aMagic = r.Bytes(8);
			uint16_t aProto = r.U16();
			std::string aVersion = r.Str();
			std::string aName = r.Str();
			uint32_t aCount = r.U32();
			if (aMagic == nullptr || memcmp(aMagic, kMagic, 8) != 0 || r.mError)
			{
				GuestLeft("bad hello");
				return;
			}
			if (aProto != kProtocolVersion)
			{
				ByteWriter w;
				w.Str(VersionMismatchText(LocalVersion(), aVersion));
				mConn.Send(MSG_REJECT, w.mData);
				mConn.Poll();
				Log("Host: rejected %s (protocol %d, version %s)", aName.c_str(), aProto, aVersion.c_str());
				GuestLeft("");
				return;
			}
			std::vector<uint64_t> aHashes;
			aHashes.reserve(std::min<uint32_t>(aCount, 200000));
			for (uint32_t i = 0; i < aCount && !r.mError && i < 200000; i++)
				aHashes.push_back(r.U64());
			mPeerName = aName.empty() ? "Player 2" : aName.substr(0, 24);
			mPeerVersion = aVersion;
			mEncoder.Reset(aHashes);
			mFrameSeq = 0;
			mAckedSeq = 0;
			mLastFrameSentMs = 0;
			mGuestMouse = MouseState();
			mGuestCursor = CURSOR_POINTER;
			mApp->mWidgetManager->RegisterAltMouseState(&mGuestMouse);

			ByteWriter w;
			w.U16(kProtocolVersion);
			w.Str(GetLocalName());
			w.U8((uint8_t)mDifficulty);
			w.Str(LocalVersion());		// optional (2.1+): older guests stop reading before it
			mConn.Send(MSG_WELCOME, w.mData);
			mHostState = HOST_PLAYING;
			mRateStart = NetMillis();
			mBytesAtLastRate = (uint32_t)mConn.mBytesSent;
			gAudioHook = this;
			SendMusicSnapshot();
			SendPing();
			mApp->mWidgetManager->MarkAllDirty();
			Log("Host: %s joined (%u known images, version %s)", mPeerName.c_str(), (unsigned)aHashes.size(), aVersion.c_str());
			AddToast(mPeerName + " jumped into the tank!", 1);
			if (!aVersion.empty() && aVersion != LocalVersion())
				AddToast(PeerVersionNote(GetLocalName(), LocalVersion(), mPeerName, aVersion), -1, 700);
			mApp->PlaySample(SOUND_BUTTONCLICK);
			RivalsGuestJoined();
			break;
		}
		case MSG_INPUT:
			if (mHostState == HOST_PLAYING)
			{
				while (!r.AtEnd() && !r.mError)
					InjectGuestEvent(r);
			}
			break;
		case MSG_FRAME_ACK:
			HandleFrameAck(r);
			break;
		case MSG_NEED_PIXELS:
			HandleNeedPixels(r);
			break;
		case MSG_STREAM_START:
			if (mHostState == HOST_PLAYING)
				HandleStreamStart(r);
			break;
		case MSG_FRAME:
			if (mKeeperViewing)
				HandleFrame(theMsg);
			break;
		case MSG_AUDIO:
			if (mKeeperViewing)
				PlayGuestAudio(r);
			break;
		case MSG_PING:
			mConn.Send(MSG_PONG, theMsg.mData);
			break;
		case MSG_PONG:
		{
			uint32_t aSent = r.U32();
			if (!r.mError)
			{
				int aRtt = (int)(NetMillis() - aSent);
				mRttMs = mRttMs < 0 ? aRtt : (mRttMs * 3 + aRtt) / 4;
			}
			break;
		}
		case MSG_BYE:
		{
			std::string aWhy = r.Str();
			GuestLeft(aWhy.empty() ? "" : aWhy);
			break;
		}
		default:
			if (mHostState == HOST_PLAYING && theMsg.mType >= MSG_RACE_SETUP && theMsg.mType <= MSG_LAST_ROUND_MSG)
				RaceHandleMessage(theMsg.mType, r);
			else if (mHostState == HOST_PLAYING && theMsg.mType >= MSG_HEROES_FIRST && theMsg.mType <= MSG_HEROES_LAST)
				HeroesHandleMessage(theMsg.mType, r);
			break;
		}
	}

	bool Session::IsRacing() const
	{
		return RaceBusy() || HeroesNetworkBusy();	// a Pet Heroes match counts as a race here
	}

	void Session::SendMsg(uint8_t theType, const std::vector<uint8_t>& theData)
	{
		if (IsConnected())
			mConn.Send(theType, theData);
	}

	void Session::SuspendGuestView()
	{
		if (mView != nullptr)
		{
			mApp->mWidgetManager->RemoveWidget(mView);
			mApp->SafeDeleteWidget(mView);
			mView = nullptr;
		}
		mApp->mMusicInterface->StopAllMusic();
		mApp->SetCursor(CURSOR_POINTER);
		mApp->mWidgetManager->MarkAllDirty();
	}

	void Session::ResumeGuestView()
	{
		if (IsGuestPlaying())
			EnterGuestView();
	}

	void Session::SuspendStreaming(bool theSuspend)
	{
		if (mRole != ROLE_HOST)
			return;
		if (theSuspend)
		{
			if (gDrawRecorder == &mEncoder)
				gDrawRecorder = nullptr;
			mEncoder.AbortFrame();
			mCapturing = false;
			FlushAudio();
			gAudioHook = nullptr;
		}
		else if (HasGuest())
		{
			gAudioHook = this;
			SendMusicSnapshot();
			mApp->mWidgetManager->MarkAllDirty();
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Frame stream plumbing, shared by Co-op (host -> guest) and Alien Keeper
	// (fish keeper -> alien keeper, either direction).
	///////////////////////////////////////////////////////////////////////////
	void Session::HandleFrame(NetMessage& theMsg)
	{
		if (!mDecoder.Decode(theMsg.mData.data(), theMsg.mData.size()))
		{
			Log("Frame decode failed: %s", mDecoder.mError.c_str());
			if (mKeeperViewing)
			{
				SendStreamStart();		// start the stream over from scratch
				return;
			}
			Leave("The picture stream from the host got corrupted.");
			return;
		}
		mLastFrameRecvMs = NetMillis();
		ByteWriter w;
		w.U32(mDecoder.mSeq);
		mConn.Send(MSG_FRAME_ACK, w.mData);
		for (uint32_t anId : mDecoder.mUnresolved)
		{
			ByteWriter n;
			n.U32(anId);
			mConn.Send(MSG_NEED_PIXELS, n.mData);
		}
		mDecoder.mUnresolved.clear();
		if (mView)
			mView->MarkDirty();
	}

	void Session::HandleFrameAck(ByteReader& r)
	{
		uint32_t aSeq = r.U32();
		if (!r.mError && aSeq > mAckedSeq && aSeq <= mFrameSeq)
			mAckedSeq = aSeq;
	}

	void Session::HandleNeedPixels(ByteReader& r)
	{
		uint32_t anId = r.U32();
		if (!r.mError)
		{
			Log("Viewer can't resolve image %u, sending pixels", anId);
			mEncoder.GuestCannotResolve(anId);
		}
	}

	void Session::SendStreamStart()
	{
		mDecoder.Reset();
		std::vector<uint64_t> aHashes;
		FrameDecoder::CollectLocalHashes(aHashes);
		ByteWriter w;
		w.U32((uint32_t)aHashes.size());
		for (uint64_t h : aHashes)
			w.U64(h);
		mConn.Send(MSG_STREAM_START, w.mData);
		Log("Keeper: asked for the tank stream (%u image hashes)", (unsigned)aHashes.size());
	}

	void Session::HandleStreamStart(ByteReader& r)
	{
		if (!IsRacing())
			return;
		uint32_t aCount = r.U32();
		std::vector<uint64_t> aHashes;
		aHashes.reserve(std::min<uint32_t>(aCount, 200000));
		for (uint32_t i = 0; i < aCount && !r.mError && i < 200000; i++)
			aHashes.push_back(r.U64());
		if (r.mError)
			return;
		if (gDrawRecorder == &mEncoder)
			gDrawRecorder = nullptr;
		mEncoder.AbortFrame();
		mCapturing = false;
		mEncoder.Reset(aHashes);
		mFrameSeq = 0;
		mAckedSeq = 0;
		mLastFrameSentMs = 0;
		mKeeperSending = true;
		gAudioHook = this;
		SendMusicSnapshot();
		mApp->mWidgetManager->MarkAllDirty();
		Log("Keeper: streaming my tank (%u image hashes known)", (unsigned)aHashes.size());
	}

	void Session::StartKeeperView()
	{
		if (!IsConnected())
			return;
		if (mView != nullptr)
		{
			mApp->mWidgetManager->RemoveWidget(mView);
			mApp->SafeDeleteWidget(mView);
			mView = nullptr;
		}
		mView = new RemoteView(true);
		mView->Resize(0, 0, mApp->mWidth, mApp->mHeight);
		mApp->mWidgetManager->AddWidget(mView);
		mKeeperViewing = true;
		SetKeeperViewShown(false);
		mLastFrameRecvMs = NetMillis();
		SendStreamStart();
	}

	void Session::SetKeeperViewShown(bool theShown)
	{
		mKeeperViewShown = theShown;
		if (mView != nullptr && mKeeperViewing && mView->mVisible != theShown)
			mView->SetVisible(theShown);
	}

	void Session::StopKeeperView()
	{
		if (!mKeeperViewing)
			return;
		mKeeperViewing = false;
		SuspendGuestView();		// removes the view, stops the streamed music
	}

	void Session::StopKeeperStream()
	{
		if (!mKeeperSending)
			return;
		mKeeperSending = false;
		if (gDrawRecorder == &mEncoder)
			gDrawRecorder = nullptr;
		mEncoder.AbortFrame();
		mCapturing = false;
		FlushAudio();
		gAudioHook = nullptr;
	}

	void Session::TryBeginFrame(int theViewerCursor)
	{
		uint32_t aNow = NetMillis();
		uint32_t anInFlight = mFrameSeq - mAckedSeq;
		int aRtt = mRttMs < 0 ? 100 : mRttMs;
		uint32_t aWindow = (uint32_t)std::clamp((aRtt + 80) / 16, 4, 20);
		if (aNow - mLastFrameSentMs >= 15 && anInFlight < aWindow && mConn.PendingSendBytes() < 2 * 1024 * 1024)
		{
			WidgetManager* wm = mApp->mWidgetManager;
			int hx = wm->mMouseIn ? wm->mLastMouseX : -1;
			int hy = wm->mMouseIn ? wm->mLastMouseY : -1;
			mEncoder.BeginFrame(wm->mImage, ++mFrameSeq, hx, hy, theViewerCursor, (uint8_t)mDifficulty);
			gDrawRecorder = &mEncoder;
			mCapturing = true;
		}
	}

	void Session::FinishFrame()
	{
		if (!mCapturing)
			return;
		gDrawRecorder = nullptr;
		mCapturing = false;
		mEncoder.EndFrame(mFrameBuf);
		mConn.Send(MSG_FRAME, mFrameBuf);
		mLastFrameSentMs = NetMillis();
		if (getenv("INSANIQ_COOPSTATS") && mFrameSeq % 120 == 0)
			Log("Stream: frame %u raw=%u z=%u inflight=%u rtt=%d kbps=%.0f pixdefs=%u live=%lluKB", mFrameSeq, mEncoder.mLastRawBytes, (unsigned)mFrameBuf.size(), mFrameSeq - mAckedSeq, mRttMs, mKbps, mEncoder.mPixelDefsSent, (unsigned long long)(mEncoder.mPixelBytesLive / 1024));
	}

	void Session::TestGuestClick(int theX, int theY)
	{
		if (!HasGuest())
			return;
		ByteWriter w;
		w.U8(IN_MOVE); w.I16(theX); w.I16(theY);
		w.U8(IN_DOWN); w.I16(theX); w.I16(theY); w.I8(1);
		w.U8(IN_UP); w.I16(theX); w.I16(theY); w.I8(1);
		ByteReader r(w.mData.data(), w.mData.size());
		while (!r.AtEnd() && !r.mError)
			InjectGuestEvent(r);
	}

	void Session::InjectGuestEvent(ByteReader& r)
	{
		uint8_t aKind = r.U8();
		WidgetManager* wm = mApp->mWidgetManager;
		int x = 0, y = 0, aButton = 0;
		switch (aKind)
		{
		case IN_MOVE:
		case IN_LEAVE:
			x = r.I16(); y = r.I16();
			break;
		case IN_DOWN:
		case IN_UP:
			x = r.I16(); y = r.I16(); aButton = r.I8();
			break;
		case IN_WHEEL:
			aButton = r.I16();
			break;
		case IN_KEYDOWN:
		case IN_KEYUP:
			aButton = r.U16();
			break;
		case IN_CHAR:
			aButton = r.U8();
			break;
		default:
			r.mError = true;
			return;
		}
		if (r.mError)
			return;
		x = std::clamp(x, 0, mApp->mWidth - 1);
		y = std::clamp(y, 0, mApp->mHeight - 1);

		bool aHadFocus = wm->mHasFocus;
		wm->mHasFocus = true;
		wm->SwapMouseState(mGuestMouse);
		gRemoteDispatch = true;
		gRemoteCursor = mGuestCursor;
		mCurrentPlayer = 1;
		RivalsScope aWallet(1);	// Coin Rivals: player 2 spends and earns from their own wallet

		switch (aKind)
		{
		case IN_MOVE:
			wm->MouseMove(x, y);
			break;
		case IN_LEAVE:
			if (wm->mDownButtons == 0)
				wm->MouseExit(x, y);
			break;
		case IN_DOWN:
			wm->MouseMove(x, y);
			wm->MouseDown(x, y, aButton);
			break;
		case IN_UP:
			wm->MouseMove(x, y);
			wm->MouseUp(x, y, aButton);
			break;
		case IN_WHEEL:
			wm->MouseWheel(aButton);
			break;
		case IN_KEYDOWN:
			if (aButton > 0 && aButton < 0xFF)
				wm->KeyDown((KeyCode)aButton);
			break;
		case IN_KEYUP:
			if (aButton > 0 && aButton < 0xFF)
				wm->KeyUp((KeyCode)aButton);
			break;
		case IN_CHAR:
			if (aButton >= 32 && aButton < 127)
				wm->KeyChar((char)aButton);
			break;
		}

		mCurrentPlayer = 0;
		mGuestCursor = gRemoteCursor;
		gRemoteDispatch = false;
		wm->SwapMouseState(mGuestMouse);
		wm->mHasFocus = aHadFocus;
	}

	bool Session::GetPlayerPointer(int thePlayer, int& theX, int& theY, bool& theLeftDown)
	{
		if (mApp == nullptr || mApp->mWidgetManager == nullptr)
			return false;
		WidgetManager* wm = mApp->mWidgetManager;
		if (thePlayer == 0)
		{
			theX = wm->mLastMouseX;
			theY = wm->mLastMouseY;
			theLeftDown = (wm->mActualDownButtons & 0x01) != 0;
			return true;
		}
		if (!HasGuest())
			return false;
		// While player 2's events are being dispatched, their state is the live one.
		const MouseState* s = &mGuestMouse;
		if (gRemoteDispatch)
		{
			theX = wm->mLastMouseX;
			theY = wm->mLastMouseY;
			theLeftDown = (wm->mActualDownButtons & 0x01) != 0;
			return true;
		}
		theX = s->mLastMouseX;
		theY = s->mLastMouseY;
		theLeftDown = (s->mActualDownButtons & 0x01) != 0;
		return true;
	}

	///////////////////////////////////////////////////////////////////////////
	// Host audio mirroring
	///////////////////////////////////////////////////////////////////////////
	void Session::OnSoundPlay(uint32_t theSerial, int theSfxId, double theVolume, int thePan, float thePitch, bool looping)
	{
		if (!IsConnected())
			return;
		mAudioSent++;
		mAudioOut.U8(AE_PLAY);
		mAudioOut.U32(theSerial);
		mAudioOut.I16(theSfxId);
		mAudioOut.F32((float)theVolume);
		mAudioOut.I16(thePan);
		mAudioOut.F32(thePitch);
		mAudioOut.U8(looping ? 1 : 0);
	}

	void Session::OnSoundStop(uint32_t theSerial)
	{
		if (!IsConnected())
			return;
		mAudioOut.U8(AE_STOP);
		mAudioOut.U32(theSerial);
	}

	void Session::OnMusic(int theOp, int theSongId, int theOffset, double theValue, bool theFlag)
	{
		if (!IsConnected())
			return;
		mAudioOut.U8(AE_MUSIC);
		mAudioOut.U8((uint8_t)theOp);
		mAudioOut.I16(theSongId);
		mAudioOut.I32(theOffset);
		mAudioOut.F32((float)theValue);
		mAudioOut.U8(theFlag ? 1 : 0);
	}

	void Session::FlushAudio()
	{
		if (mAudioOut.Size() == 0)
			return;
		if (IsConnected())
			mConn.Send(MSG_AUDIO, mAudioOut.mData);
		mAudioOut.Clear();
	}

	void Session::SendMusicSnapshot()
	{
		SDLMusicInterface* aMusic = dynamic_cast<SDLMusicInterface*>(mApp->mMusicInterface);
		OnMusic(AudioHook::MUSIC_STOP_ALL, 0, 0, 0, false);
		for (int aSong = 0; aSong < 16; aSong++)
		{
			if (mApp->mMusicInterface->IsPlaying(aSong))
			{
				int anOrder = aMusic != nullptr ? aMusic->GetMusicOrder(aSong) : 0;
				OnMusic(AudioHook::MUSIC_PLAY, aSong, anOrder < 0 ? 0 : anOrder, 0, false);
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Joining
	///////////////////////////////////////////////////////////////////////////
	bool Session::StartJoin(const std::string& theAddress, uint16_t thePort, std::string& theError)
	{
		if (mRole == ROLE_HOST)
			StopHosting("The host started joining another game.");
		if (mRole == ROLE_GUEST)
			Leave("");
		std::string anAddr = theAddress;
		uint16_t aPort = thePort;
		size_t aColon = anAddr.rfind(':');
		if (aColon != std::string::npos)
		{
			aPort = (uint16_t)atoi(anAddr.c_str() + aColon + 1);
			anAddr = anAddr.substr(0, aColon);
		}
		while (!anAddr.empty() && anAddr.back() == ' ')
			anAddr.pop_back();
		while (!anAddr.empty() && anAddr.front() == ' ')
			anAddr.erase(anAddr.begin());
		if (anAddr.empty())
		{
			theError = "Type the host's address first.";
			return false;
		}
		if (aPort == 0)
			aPort = kDefaultPort;
		if (!mConn.StartConnect(anAddr, aPort, theError))
		{
			Log("Guest: connect to %s:%d failed: %s", anAddr.c_str(), aPort, theError.c_str());
			return false;
		}
		mDecoder.Reset();
		mRole = ROLE_GUEST;
		mGuestState = GUEST_CONNECTING;
		mStateSince = NetMillis();
		mPendingLeaveReason.clear();
		Log("Guest: connecting to %s:%d", anAddr.c_str(), aPort);
		return true;
	}

	void Session::Leave(const std::string& theReason)
	{
		CancelAutoJoin();
		if (mRole != ROLE_GUEST)
			return;
		if (mConn.IsConnected())
		{
			ByteWriter w;
			w.Str("");
			mConn.Send(MSG_BYE, w.mData);
			for (int i = 0; i < 3; i++)
				mConn.Poll();
		}
		mConn.Close();
		bool wasPlaying = mGuestState == GUEST_PLAYING;
		mRole = ROLE_NONE;
		mGuestState = GUEST_IDLE;
		ExitGuestView(wasPlaying ? theReason : "");
		Log("Guest: left (%s) after %u frames; played %u sounds, %u music changes", theReason.c_str(), mDecoder.mFramesDecoded, mAudioPlayed, mMusicOps);
		mDecoder.Reset();
		mRttMs = -1;
	}

	void Session::PollGuest()
	{
		uint32_t aNow = NetMillis();
		bool wasConnecting = mConn.GetState() == Connection::CONNECTING;
		if (!mConn.Poll())
		{
			std::string aWhy = mConn.GetError();
			// The host usually says why before hanging up; prefer that.
			NetMessage aLast;
			while (mConn.Receive(aLast))
			{
				if (aLast.mType == MSG_BYE || aLast.mType == MSG_REJECT)
				{
					ByteReader r(aLast.mData.data(), aLast.mData.size());
					std::string aSaid = r.Str();
					if (!aSaid.empty())
						aWhy = aSaid;
					else if (aLast.mType == MSG_BYE)
						aWhy = "The host ended the game.";
				}
			}
			if (!mPendingLeaveReason.empty())
				aWhy = mPendingLeaveReason;
			bool wasPlaying = mGuestState == GUEST_PLAYING;
			if (!wasPlaying && mGuestState == GUEST_CONNECTING && !mAutoJoinAddr.empty() && (int32_t)(mAutoJoinDeadline - NetMillis()) > 0)
			{
				// -join=<address>: the host is probably still starting up; try again shortly.
				Log("Guest: %s - retrying", aWhy.c_str());
				mConn.Close();
				mRole = ROLE_NONE;
				mGuestState = GUEST_IDLE;
				mAutoJoinRetryAt = NetMillis() + 1500;
				return;
			}
			Log("Guest: connection ended: %s", aWhy.c_str());
			mConn.Close();
			mRole = ROLE_NONE;
			mGuestState = GUEST_IDLE;
			ExitGuestView(aWhy.empty() ? "Disconnected." : aWhy);
			mDecoder.Reset();
			if (!wasPlaying)
				ShowInfo("Couldn't Join", aWhy.empty() ? "Couldn't connect to the host." : aWhy);
			return;
		}

		if (wasConnecting && mConn.IsConnected())
		{
			mGuestState = GUEST_HANDSHAKE;
			mStateSince = aNow;
			std::vector<uint64_t> aHashes;
			uint32_t t0 = NetMillis();
			FrameDecoder::CollectLocalHashes(aHashes);
			if (const char* aDrop = getenv("INSANIQ_TEST_HASHDROP"))
			{
				// Test hook: pretend we lack some assets so the host must send pixels.
				int n = std::max(2, atoi(aDrop));
				std::vector<uint64_t> aKept;
				for (size_t i = 0; i < aHashes.size(); i++)
					if (i % n != 0)
						aKept.push_back(aHashes[i]);
				aHashes.swap(aKept);
			}
			ByteWriter w;
			w.Bytes(kMagic, 8);
			w.U16(kProtocolVersion);
			w.Str(LocalVersion());
			w.Str(GetLocalName());
			w.U32((uint32_t)aHashes.size());
			for (uint64_t h : aHashes)
				w.U64(h);
			mConn.Send(MSG_HELLO, w.mData);
			Log("Guest: connected, sent hello with %u image hashes (%u ms)", (unsigned)aHashes.size(), NetMillis() - t0);
		}

		NetMessage aMsg;
		while (mRole == ROLE_GUEST && mConn.Receive(aMsg))
			GuestHandleMessage(aMsg);
		if (mRole != ROLE_GUEST)
			return;

		FlushGuestMove();

		if (mGuestState == GUEST_HANDSHAKE && (int32_t)(NetMillis() - mStateSince) > 15000)
		{
			Leave("");
			ShowInfo("Couldn't Join", "The host didn't answer. Make sure they have Host Game open and the same version of the mod.");
			return;
		}
		if (mGuestState == GUEST_PLAYING)
		{
			if ((int32_t)(NetMillis() - mLastPingSent) >= 1000)
				SendPing();
			if ((int32_t)(NetMillis() - mLastHeard) > 20000)
			{
				Leave("Lost contact with the host.");
				return;
			}
		}
	}

	void Session::GuestHandleMessage(NetMessage& theMsg)
	{
		mLastHeard = NetMillis();
		ByteReader r(theMsg.mData.data(), theMsg.mData.size());
		switch (theMsg.mType)
		{
		case MSG_WELCOME:
		{
			uint16_t aProto = r.U16();
			mPeerName = r.Str();
			int aDiff = r.U8();
			std::string aHostVersion = r.Str();		// empty from hosts before 2.1
			if (r.mError)
				aHostVersion.clear();
			(void)aProto;
			if (aDiff < DIFF_COUNT)
				mDifficulty = aDiff;
			mGuestState = GUEST_PLAYING;
			mAutoJoinAddr.clear();
			mLastFrameRecvMs = NetMillis();
			mPeerVersion = aHostVersion;
			Log("Guest: welcomed by %s (version %s)", mPeerName.c_str(), aHostVersion.empty() ? "2.0 or older" : aHostVersion.c_str());
			EnterGuestView();
			if (!aHostVersion.empty() && aHostVersion != LocalVersion())
				Log("Guest: the host runs %s, this is %s", aHostVersion.c_str(), LocalVersion().c_str());	// the host shows the note
			SendPing();
			break;
		}
		case MSG_REJECT:
		{
			std::string aWhy = r.Str();
			Log("Guest: rejected: %s", aWhy.c_str());
			mConn.Close();
			mRole = ROLE_NONE;
			mGuestState = GUEST_IDLE;
			ExitGuestView("");
			mDecoder.Reset();
			ShowInfo("Couldn't Join", aWhy);
			break;
		}
		case MSG_FRAME:
			HandleFrame(theMsg);
			break;
		case MSG_FRAME_ACK:
			if (mKeeperSending)
				HandleFrameAck(r);
			break;
		case MSG_NEED_PIXELS:
			if (mKeeperSending)
				HandleNeedPixels(r);
			break;
		case MSG_STREAM_START:
			HandleStreamStart(r);
			break;
		case MSG_AUDIO:
			PlayGuestAudio(r);
			break;
		case MSG_PING:
			mConn.Send(MSG_PONG, theMsg.mData);
			break;
		case MSG_PONG:
		{
			uint32_t aSent = r.U32();
			if (!r.mError)
			{
				int aRtt = (int)(NetMillis() - aSent);
				mRttMs = mRttMs < 0 ? aRtt : (mRttMs * 3 + aRtt) / 4;
			}
			break;
		}
		case MSG_BYE:
		{
			std::string aWhy = r.Str();
			Log("Guest: host said bye: %s", aWhy.c_str());
			mConn.Close();
			mRole = ROLE_NONE;
			mGuestState = GUEST_IDLE;
			ExitGuestView(aWhy.empty() ? "The host ended the game." : aWhy);
			mDecoder.Reset();
			break;
		}
		default:
			if (mGuestState == GUEST_PLAYING && theMsg.mType >= MSG_RACE_SETUP && theMsg.mType <= MSG_LAST_ROUND_MSG)
				RaceHandleMessage(theMsg.mType, r);
			else if (mGuestState == GUEST_PLAYING && theMsg.mType >= MSG_HEROES_FIRST && theMsg.mType <= MSG_HEROES_LAST)
				HeroesHandleMessage(theMsg.mType, r);
			break;
		}
	}

	void Session::PlayGuestAudio(ByteReader& r)
	{
		while (!r.AtEnd() && !r.mError)
		{
			uint8_t anEvt = r.U8();
			if (anEvt == AE_PLAY)
			{
				r.U32();
				int aSfx = r.I16();
				float aVol = r.F32();
				int aPan = r.I16();
				float aPitch = r.F32();
				bool aLoop = r.U8() != 0;
				if (r.mError)
					return;
				SoundInstance* anInst = mKeeperViewing && !mKeeperViewShown ? nullptr : mApp->mSoundManager->GetSoundInstance(aSfx);
				if (anInst != nullptr)
				{
					anInst->SetVolume(aVol);
					if (aPan != 0)
						anInst->SetPan(aPan);
					if (aPitch > 0.01f && std::fabs(aPitch - 1.0f) > 0.001f)
						anInst->AdjustPitch(std::log(aPitch) / std::log(1.0594630943592952645618252949463));
					anInst->Play(aLoop, true);
					mAudioPlayed++;
				}
			}
			else if (anEvt == AE_STOP)
				r.U32();
			else if (anEvt == AE_VOLUME)
			{
				r.U32(); r.F32();
			}
			else if (anEvt == AE_PAN)
			{
				r.U32(); r.I16();
			}
			else if (anEvt == AE_PITCH)
			{
				r.U32(); r.F32();
			}
			else if (anEvt == AE_MUSIC)
			{
				int anOp = r.U8();
				int aSong = r.I16();
				int anOffset = r.I32();
				double aValue = r.F32();
				bool aFlag = r.U8() != 0;
				if (r.mError)
					return;
				if (mKeeperViewing)
					continue;		// the lair plays its own music
				MusicInterface* m = mApp->mMusicInterface;
				mMusicOps++;
				switch (anOp)
				{
				case MUSIC_PLAY: m->PlayMusic(aSong, anOffset, aFlag); break;
				case MUSIC_STOP: m->StopMusic(aSong); break;
				case MUSIC_STOP_ALL: m->StopAllMusic(); break;
				case MUSIC_FADE_IN: m->FadeIn(aSong, anOffset, aValue, aFlag); break;
				case MUSIC_FADE_OUT: m->FadeOut(aSong, aFlag, aValue); break;
				case MUSIC_FADE_OUT_ALL: m->FadeOutAll(aFlag, aValue); break;
				case MUSIC_SONG_VOLUME: m->SetSongVolume(aSong, aValue); break;
				case MUSIC_SONG_MAX_VOLUME: m->SetSongMaxVolume(aSong, aValue); break;
				case MUSIC_PAUSE: m->PauseMusic(aSong); break;
				case MUSIC_RESUME: m->ResumeMusic(aSong); break;
				case MUSIC_PAUSE_ALL: m->PauseAllMusic(); break;
				case MUSIC_RESUME_ALL: m->ResumeAllMusic(); break;
				}
			}
			else
				return;
		}
	}

	void Session::EnterGuestView()
	{
		mApp->CleanDialogs();
		mApp->KillDialog(kDialogCoop);
		mApp->mMusicInterface->StopAllMusic();
		if (mView == nullptr)
		{
			mView = new RemoteView();
			mView->Resize(0, 0, mApp->mWidth, mApp->mHeight);
			mApp->mWidgetManager->AddWidget(mView);
		}
		mApp->mWidgetManager->BringToFront(mView);
		mApp->mWidgetManager->SetFocus(mView);
		mLastAppliedCursor = -1;
	}

	void Session::ExitGuestView(const std::string& theReason)
	{
		if (!mShuttingDown)
		{
			RacePeerLost(mPeerName + " left the race.");
			HeroesPeerLost(mPeerName + " left the match.");
		}
		if (mShuttingDown)
		{
			if (mView != nullptr && mApp->mWidgetManager != nullptr)
				mApp->mWidgetManager->RemoveWidget(mView);
			delete mView;
			mView = nullptr;
			return;
		}
		if (mView != nullptr)
		{
			mApp->mWidgetManager->RemoveWidget(mView);
			mApp->SafeDeleteWidget(mView);
			mView = nullptr;
			mApp->mMusicInterface->StopAllMusic();
			mApp->SetCursor(CURSOR_POINTER);
			// Back to the local menu, with its music.
			mApp->PlayMusic(2, 0);
			mApp->mWidgetManager->MarkAllDirty();
		}
		if (!theReason.empty())
			ShowInfo("Co-op Ended", theReason);
	}

	void Session::ShowInfo(const std::string& theTitle, const std::string& theText)
	{
		if (mApp == nullptr || mShuttingDown)
			return;
		mApp->KillDialog(kDialogCoopInfo);
		mApp->DoDialog(kDialogCoopInfo, true, theTitle, theText, "OK", Dialog::BUTTONS_FOOTER);
	}

	void Session::GuestInput(uint8_t theKind, int a, int b, int c)
	{
		if (!IsGuestPlaying())
			return;
		if (theKind == IN_MOVE)
		{
			mMovePending = true;
			mMoveX = a;
			mMoveY = b;
			return;
		}
		ByteWriter w;
		if (mMovePending && (theKind == IN_DOWN || theKind == IN_UP || theKind == IN_LEAVE))
		{
			// The button event carries its own position; the pending move is folded in.
			mMovePending = false;
		}
		w.U8(theKind);
		switch (theKind)
		{
		case IN_DOWN:
		case IN_UP:
			w.I16(a); w.I16(b); w.I8((int8_t)c);
			break;
		case IN_LEAVE:
			w.I16(a); w.I16(b);
			break;
		case IN_WHEEL:
			w.I16(a);
			break;
		case IN_KEYDOWN:
		case IN_KEYUP:
			w.U16((uint16_t)a);
			break;
		case IN_CHAR:
			w.U8((uint8_t)a);
			break;
		}
		mConn.Send(MSG_INPUT, w.mData);
	}

	void Session::FlushGuestMove()
	{
		if (!mMovePending || !IsGuestPlaying())
			return;
		mMovePending = false;
		ByteWriter w;
		w.U8(IN_MOVE);
		w.I16(mMoveX);
		w.I16(mMoveY);
		mConn.Send(MSG_INPUT, w.mData);
	}

	///////////////////////////////////////////////////////////////////////////
	// Frame loop
	///////////////////////////////////////////////////////////////////////////
	void Session::PreUpdateFrames()
	{
		if (mTestHarness)
			TestHarnessUpdate();
		if ((mAutoHost || !mAutoJoin.empty()) && mApp->mGameSelector != nullptr)
		{
			std::string anError;
			if (mAutoHost && !StartHosting(kDefaultPort, anError))
				ShowInfo("Can't Host", anError);
			if (!mAutoJoin.empty())
			{
				mAutoJoinAddr = mAutoJoin;
				mAutoJoinDeadline = NetMillis() + 30000;
				if (!StartJoin(mAutoJoin, kDefaultPort, anError))
					ShowInfo("Couldn't Join", anError);
			}
			mAutoHost = false;
			mAutoJoin.clear();
		}
		if (mAutoJoinRetryAt != 0 && (int32_t)(NetMillis() - mAutoJoinRetryAt) >= 0)
		{
			mAutoJoinRetryAt = 0;
			std::string anError;
			if (mRole == ROLE_NONE && !StartJoin(mAutoJoinAddr, kDefaultPort, anError))
				ShowInfo("Couldn't Join", anError);
		}
		if (mRole == ROLE_HOST)
			PollHost();
		else
			mDiscovery.Poll();	// the Join page listens for LAN hosts
		if (mRole == ROLE_GUEST)
			PollGuest();
		RaceUpdate();
		UpdateCheckFrame();
		UpdateOverlayTimers();
	}

	void Session::UpdateOverlayTimers()
	{
		for (Marker& m : mMarkers)
			m.mAge++;
		while (!mMarkers.empty() && mMarkers.front().mAge > 150)
			mMarkers.pop_front();
		for (Toast& t : mToasts)
			t.mAge++;
		mToasts.erase(std::remove_if(mToasts.begin(), mToasts.end(), [](const Toast& t) { return t.mAge > t.mDuration; }), mToasts.end());
	}

	void Session::PreDrawScreen()
	{
		if (IsRacing() || !mToasts.empty())
			mApp->mWidgetManager->MarkAllDirty();	// our overlay is drawn over the whole screen
		if (mKeeperSending)
		{
			TryBeginFrame(CURSOR_POINTER);
			return;
		}
		if (mRole != ROLE_HOST)
			return;
		if (!mMarkers.empty() || !mToasts.empty() || HasGuest())
			mApp->mWidgetManager->MarkAllDirty();
		if (!HasGuest() || IsRacing())
			return;
		TryBeginFrame(mGuestCursor);
	}

	void Session::PostDrawScreen()
	{
		if (mApp == nullptr || mApp->mWidgetManager == nullptr || mApp->mWidgetManager->mImage == nullptr)
			return;
		PostDrawOverlays();
		if (mTestHarness)
			TestHarnessPostDraw();
	}

	void Session::PostDrawOverlays()
	{
		if (IsRacing())
		{
			// Each computer draws its own race overlay. In Alien Keeper the fish
			// keeper's tank and its shared overlay are streamed; what's drawn after
			// FinishFrame (toasts, the blackout) stays on this screen.
			Graphics g(mApp->mWidgetManager->mImage);
			g.Translate(-mApp->mWidgetManager->mMouseDestRect.mX, -mApp->mWidgetManager->mMouseDestRect.mY);
			DrawRaceOverlay(&g);
			FinishFrame();
			FlushAudio();
			DrawRaceLocalOverlay(&g);
			if (!RaceHidesToasts())
				DrawToasts(&g, 104, 258);	// left of the rival panel
			return;
		}
		if (mMarkers.empty() && mToasts.empty() && !mCapturing && !HasGuest())
			return;

		Graphics g(mApp->mWidgetManager->mImage);
		g.Translate(-mApp->mWidgetManager->mMouseDestRect.mX, -mApp->mWidgetManager->mMouseDestRect.mY);
		if (mRole == ROLE_HOST)
			DrawSharedOverlay(&g);

		FinishFrame();
		FlushAudio();

		if (HasGuest() && mGuestMouse.mMouseIn)
			DrawRemoteCursor(&g, mGuestMouse.mLastMouseX, mGuestMouse.mLastMouseY, mGuestCursor, 1);
	}

	void Session::DrawGuestView(Graphics* g)
	{
		g->SetColor(Color::Black);
		g->FillRect(0, 0, mApp->mWidth, mApp->mHeight);
		if (mDecoder.HasFrame())
			mDecoder.Replay(g->mDestImage);

		if (mDecoder.mGuestCursor != mLastAppliedCursor)
		{
			mLastAppliedCursor = mDecoder.mGuestCursor;
			mApp->SetCursor(mLastAppliedCursor);
		}

		if (mDecoder.mHostX >= 0)	// the streaming player's mouse
			DrawRemoteCursor(g, mDecoder.mHostX, mDecoder.mHostY, CURSOR_POINTER, mKeeperViewing ? 1 - LocalPlayer() : 0);

		uint32_t aStall = NetMillis() - mLastFrameRecvMs;
		if (!mDecoder.HasFrame() || aStall > 1500)
		{
			std::string aMsg = mDecoder.HasFrame() ? "Waiting for " + mPeerName + "..." : "Joining " + mPeerName + "'s tank...";
			g->SetFont(FONT_JUNGLEFEVER15OUTLINE);
			int w = g->GetFont()->StringWidth(aMsg);
			g->SetColor(Color(0, 0, 0, 160));
			g->FillRect(320 - w / 2 - 16, 212, w + 32, 40);
			g->SetColor(Color(255, 240, 0));
			g->DrawString(aMsg, 320 - w / 2, 240);
		}
		else if (mRttMs > 250)
		{
			char aBuf[64];
			snprintf(aBuf, sizeof(aBuf), "Laggy connection: %d ms", mRttMs);
			g->SetFont(FONT_JUNGLEFEVER12OUTLINE);
			g->SetColor(Color(255, 120, 80));
			g->DrawString(aBuf, 8, 474);
		}
	}

	void Session::DrawRemoteCursor(Graphics* g, int x, int y, int theCursor, int thePlayer)
	{
		if (x < 0 || y < 0)
			return;
		Image* anImg = (theCursor == CURSOR_HAND) ? IMAGE_CURSOR_HAND : IMAGE_CURSOR_POINTER;
		Color aColor = PlayerColor(thePlayer);
		if (anImg != nullptr)
		{
			int cx = x - anImg->mWidth / 2;
			int cy = y - anImg->mHeight / 2;
			g->SetColorizeImages(true);
			g->SetColor(aColor);
			g->DrawImage(anImg, cx, cy);
			g->SetColorizeImages(false);
		}
		std::string aName = PlayerName(thePlayer);
		if (aName.size() > 12)
			aName = aName.substr(0, 12);
		Font* aFont = FONT_JUNGLEFEVER10OUTLINE;
		if (aFont == nullptr)
			return;
		g->SetFont(aFont);
		int w = aFont->StringWidth(aName);
		int tx = x + 14, ty = y + 26;
		if (tx + w + 6 > mApp->mWidth)
			tx = x - w - 14;
		if (ty > mApp->mHeight - 4)
			ty = y - 14;
		g->SetColor(Color(0, 0, 0, 150));
		g->FillRect(tx - 3, ty - aFont->GetAscent() - 1, w + 6, aFont->GetHeight() + 2);
		g->SetColor(aColor);
		g->DrawString(aName, tx, ty);
	}

	void Session::AddMarker(int thePlayer, int theX, int theY)
	{
		if (mMarkers.size() > 8)
			mMarkers.pop_front();
		mMarkers.push_back({ theX, theY, thePlayer, 0 });
		if (mApp)
			mApp->PlaySample(SOUND_BUTTONCLICK);
	}

	void Session::AddToast(const std::string& theText, int thePlayer, int theDuration)
	{
		if (mToasts.size() > 3)
			mToasts.pop_front();
		mToasts.push_back({ theText, thePlayer, 0, std::max(theDuration, 100) });
		Log("Toast: %s", theText.c_str());
	}

	void Session::DrawSharedOverlay(Graphics* g)
	{
		DrawRivalsOverlay(g);

		// "Look here!" pings.
		for (const Marker& m : mMarkers)
		{
			Color c = PlayerColor(m.mPlayer);
			float t = m.mAge / 150.0f;
			int anAlpha = (int)(255 * (1.0f - t));
			for (int aRing = 0; aRing < 2; aRing++)
			{
				float r = 10.0f + ((m.mAge + aRing * 25) % 50) * 1.2f;
				c.mAlpha = std::max(0, anAlpha - aRing * 60);
				g->SetColor(c);
				const int kSeg = 20;
				for (int i = 0; i < kSeg; i++)
				{
					float a0 = i * 6.2831853f / kSeg, a1 = (i + 1) * 6.2831853f / kSeg;
					g->DrawLine((int)(m.mX + cosf(a0) * r), (int)(m.mY + sinf(a0) * r), (int)(m.mX + cosf(a1) * r), (int)(m.mY + sinf(a1) * r));
				}
			}
			c.mAlpha = anAlpha;
			g->SetColor(c);
			g->FillRect(m.mX - 2, m.mY - 2, 5, 5);
			Font* aFont = FONT_JUNGLEFEVER10OUTLINE;
			if (aFont)
			{
				g->SetFont(aFont);
				std::string s = PlayerName(m.mPlayer) + "!";
				g->DrawString(s, m.mX - aFont->StringWidth(s) / 2, m.mY - 16);
			}
		}

		// Toasts (newest at the bottom), just under the store bar (and the
		// Coin Rivals wallet panel).
		DrawToasts(g, RivalsActive() ? 128 : 92);
	}

	void Session::DrawToasts(Graphics* g, int theTop, int theCenterX)
	{
		int y = theTop;
		Font* aFont = FONT_JUNGLEFEVER12OUTLINE;
		if (aFont == nullptr)
			return;
		g->SetFont(aFont);
		for (const Toast& t : mToasts)
		{
			int anAlpha = t.mAge < 20 ? t.mAge * 255 / 20 : (t.mAge > t.mDuration - 60 ? std::max(0, (t.mDuration - t.mAge) * 255 / 60) : 255);
			int w = aFont->StringWidth(t.mText);
			g->SetColor(Color(0, 0, 0, anAlpha * 150 / 255));
			g->FillRect(theCenterX - w / 2 - 10, y - aFont->GetAscent() - 4, w + 20, aFont->GetHeight() + 8);
			Color c = t.mPlayer < 0 ? Color(255, 255, 255) : PlayerColor(t.mPlayer);
			c.mAlpha = anAlpha;
			g->SetColor(c);
			g->DrawString(t.mText, theCenterX - w / 2, y);
			y += aFont->GetHeight() + 10;
		}
	}
}
