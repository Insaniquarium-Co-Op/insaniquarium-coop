#include <SexyAppFramework/Font.h>
#include <SexyAppFramework/MemoryImage.h>
#include <SexyAppFramework/Graphics.h>
#include "CoopUI.h"
#include "CoopHeroes.h"
#include "CoopRace.h"
#include "CoopSession.h"
#include "CoopUpdate.h"

#include "WinFishApp.h"
#include "WinFishCommon.h"
#include "Res.h"

#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/DialogButton.h>
#include <SexyAppFramework/EditWidget.h>
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Font.h>

using namespace Sexy;

namespace Coop
{
	enum
	{
		BTN_HOST = 1,
		BTN_JOIN,
		BTN_DIFF,
		BTN_STOP,
		BTN_KICK,
		BTN_CONNECT,
		BTN_BACK,
		BTN_COPY,
		BTN_PRACTICE,
		BTN_UPDATES,
		BTN_LAN0 = 10,
		BTN_LEAVE = 20,
		BTN_RESUME,
		BTN_OPTIONS,
	};

	static WinFishApp* gCoopApp = nullptr;

	std::string CoopButtonLabel()
	{
		Session& s = S();
		if (s.HasGuest())
			return "Co-op (2P)";
		if (s.IsHosting())
			return "Co-op (Hosting)";
		return "Co-op";
	}

	void OpenCoopDialog(WinFishApp* theApp)
	{
		gCoopApp = theApp;
		theApp->KillDialog(kDialogCoop);
		CoopDialog* aDialog = new CoopDialog(theApp);
		int aWidth = 460;
		int aHeight = aDialog->GetPreferredHeight(aWidth);
		aDialog->Resize(320 - aWidth / 2, std::max(4, 244 - aHeight / 2), aWidth, aHeight);
		theApp->AddDialog(kDialogCoop, aDialog);
	}

	void OpenGuestMenu()
	{
		WinFishApp* anApp = (WinFishApp*)gSexyAppBase;
		if (anApp->GetDialog(kDialogGuestMenu) != nullptr)
			return;
		GuestMenu* aDialog = new GuestMenu(anApp);
		int aWidth = 360;
		int aHeight = aDialog->GetPreferredHeight(aWidth);
		aDialog->Resize(320 - aWidth / 2, 240 - aHeight / 2, aWidth, aHeight);
		anApp->AddDialog(kDialogGuestMenu, aDialog);
		anApp->mWidgetManager->SetFocus(aDialog);
	}

	static DialogButton* MakeButton(int theId, ButtonListener* theListener, const std::string& theLabel)
	{
		DialogButton* b = MakeDialogButton(theId, theListener, theLabel, FONT_JUNGLEFEVER12OUTLINE);
		return b;
	}

	///////////////////////////////////////////////////////////////////////////
	CoopDialog::CoopDialog(WinFishApp* theApp)
		: MoneyDialog(theApp, IMAGE_DIALOG, IMAGE_DIALOGBUTTON, kDialogCoop, true, "CO-OP", "", "Close", BUTTONS_FOOTER)
	{
		SetHeaderFont(FONT_JUNGLEFEVER17OUTLINE ? FONT_JUNGLEFEVER17OUTLINE : FONT_JUNGLEFEVER15OUTLINE);
		SetColor(COLOR_HEADER, Color(0xff, 200, 0));
		mHostButton = MakeButton(BTN_HOST, this, "Host a Game");
		mJoinButton = MakeButton(BTN_JOIN, this, "Join a Game");
		mDiffButton = MakeButton(BTN_DIFF, this, "");
		mStopButton = MakeButton(BTN_STOP, this, "Stop Hosting");
		mKickButton = MakeButton(BTN_KICK, this, "Remove Player 2");
		mCopyButton = MakeButton(BTN_COPY, this, "Copy Address");
		mConnectButton = MakeButton(BTN_CONNECT, this, "Connect");
		mBackButton = MakeButton(BTN_BACK, this, "Back");
		mPracticeButton = MakeButton(BTN_PRACTICE, this, "Heroes Practice");
		mUpdateButton = MakeButton(BTN_UPDATES, this, "");
		for (int i = 0; i < 3; i++)
			mLanButtons[i] = MakeDialogButton(BTN_LAN0 + i, this, "", FONT_JUNGLEFEVER10OUTLINE);
		mAddressEdit = MakeEditWidget(0, this);
		mAddressEdit->mMaxChars = 60;
		std::string aLast;
		if (mApp->RegistryReadString("CoopLastAddress", &aLast))
			mAddressEdit->SetText(aLast);
		mLocalIps = GetLocalAddresses();
		mPage = S().IsHosting() ? PAGE_HOST : PAGE_MAIN;
	}

	CoopDialog::~CoopDialog()
	{
		delete mHostButton;
		delete mJoinButton;
		delete mDiffButton;
		delete mStopButton;
		delete mKickButton;
		delete mCopyButton;
		delete mConnectButton;
		delete mBackButton;
		delete mPracticeButton;
		delete mUpdateButton;
		for (int i = 0; i < 3; i++)
			delete mLanButtons[i];
		delete mAddressEdit;
		if (mDiscoveryStarted && S().GetRole() != ROLE_HOST)
			S().GetDiscovery().Stop();
	}

	void CoopDialog::AddedToManager(WidgetManager* theWidgetManager)
	{
		MoneyDialog::AddedToManager(theWidgetManager);
		theWidgetManager->AddWidget(mHostButton);
		theWidgetManager->AddWidget(mJoinButton);
		theWidgetManager->AddWidget(mDiffButton);
		theWidgetManager->AddWidget(mStopButton);
		theWidgetManager->AddWidget(mKickButton);
		theWidgetManager->AddWidget(mCopyButton);
		theWidgetManager->AddWidget(mConnectButton);
		theWidgetManager->AddWidget(mBackButton);
		theWidgetManager->AddWidget(mPracticeButton);
		theWidgetManager->AddWidget(mUpdateButton);
		for (int i = 0; i < 3; i++)
			theWidgetManager->AddWidget(mLanButtons[i]);
		theWidgetManager->AddWidget(mAddressEdit);
		SetPage(mPage);
	}

	void CoopDialog::RemovedFromManager(WidgetManager* theWidgetManager)
	{
		MoneyDialog::RemovedFromManager(theWidgetManager);
		theWidgetManager->RemoveWidget(mHostButton);
		theWidgetManager->RemoveWidget(mJoinButton);
		theWidgetManager->RemoveWidget(mDiffButton);
		theWidgetManager->RemoveWidget(mStopButton);
		theWidgetManager->RemoveWidget(mKickButton);
		theWidgetManager->RemoveWidget(mCopyButton);
		theWidgetManager->RemoveWidget(mConnectButton);
		theWidgetManager->RemoveWidget(mBackButton);
		theWidgetManager->RemoveWidget(mPracticeButton);
		theWidgetManager->RemoveWidget(mUpdateButton);
		for (int i = 0; i < 3; i++)
			theWidgetManager->RemoveWidget(mLanButtons[i]);
		theWidgetManager->RemoveWidget(mAddressEdit);
	}

	void CoopDialog::SetPage(Page thePage)
	{
		mPage = thePage;
		bool isMain = thePage == PAGE_MAIN, isHost = thePage == PAGE_HOST, isJoin = thePage == PAGE_JOIN;
		mHostButton->SetVisible(isMain);
		mJoinButton->SetVisible(isMain);
		mPracticeButton->SetVisible(isMain);
		mUpdateButton->SetVisible(isMain);
		mUpdateButton->mLabel = UpdateCheckEnabled() ? "Update Check: On" : "Update Check: Off";
		mDiffButton->SetVisible(isMain || isHost);
		mStopButton->SetVisible(isHost);
		mKickButton->SetVisible(isHost && S().HasGuest());
		mCopyButton->SetVisible(isHost && !S().HasGuest());
		mConnectButton->SetVisible(isJoin);
		mBackButton->SetVisible(isJoin);
		mAddressEdit->SetVisible(isJoin);
		for (int i = 0; i < 3; i++)
			mLanButtons[i]->SetVisible(false);
		if (isJoin)
		{
			if (!mDiscoveryStarted)
				mDiscoveryStarted = S().GetDiscovery().StartListening();
			if (mWidgetManager)
				mWidgetManager->SetFocus(mAddressEdit);
		}
		else if (mDiscoveryStarted && S().GetRole() != ROLE_HOST)
		{
			S().GetDiscovery().Stop();
			mDiscoveryStarted = false;
		}
		mDiffButton->mLabel = std::string("Difficulty: ") + GetTuning(S().GetDifficulty()).mName;
		Layout();
		MarkDirty();
	}

	int CoopDialog::GetPreferredHeight(int theWidth)
	{
		return 470;
	}

	void CoopDialog::Resize(int theX, int theY, int theWidth, int theHeight)
	{
		MoneyDialog::Resize(theX, theY, theWidth, theHeight);
		Layout();
	}

	void CoopDialog::Layout()
	{
		int aLeft = mX + 36;
		int aWidth = mWidth - 72;
		int aHalf = (aWidth - 8) / 2;
		int aBtnH = 33;

		// main
		mHostButton->Resize(aLeft, mY + 150, aHalf, aBtnH);
		mJoinButton->Resize(aLeft + aHalf + 8, mY + 150, aHalf, aBtnH);
		mDiffButton->Resize(aLeft, mY + 366, aWidth, aBtnH);
		mPracticeButton->Resize(aLeft, mY + 326, aHalf, aBtnH);
		mUpdateButton->Resize(aLeft + aHalf + 8, mY + 326, aHalf, aBtnH);
		// host
		mStopButton->Resize(aLeft, mY + 236, aHalf, aBtnH);
		mKickButton->Resize(aLeft + aHalf + 8, mY + 236, aHalf, aBtnH);
		mCopyButton->Resize(aLeft + aHalf + 8, mY + 236, aHalf, aBtnH);
		// join
		mAddressEdit->Resize(aLeft + 8, mY + 100, aWidth - 16, 24);
		mConnectButton->Resize(aLeft, mY + 134, aHalf, aBtnH);
		mBackButton->Resize(aLeft + aHalf + 8, mY + 134, aHalf, aBtnH);
		for (int i = 0; i < 3; i++)
			mLanButtons[i]->Resize(aLeft, mY + 232 + i * 34, aWidth, aBtnH);
	}

	void CoopDialog::Update()
	{
		MoneyDialog::Update();
		Session& s = S();
		if (mPage == PAGE_JOIN)
		{
			// LAN games heard so far.
			const std::vector<LanGame>& aGames = s.GetDiscovery().GetGames();
			mLanAddresses.clear();
			for (int i = 0; i < 3; i++)
			{
				bool aShow = i < (int)aGames.size();
				if (aShow)
				{
					const LanGame& g = aGames[i];
					std::string anAddr = g.mAddress + (g.mPort != kDefaultPort ? ":" + std::to_string(g.mPort) : "");
					mLanAddresses.push_back(anAddr);
					std::string aLabel = g.mHostName + "'s tank  (" + g.mAddress + ")" + (g.mFull ? "  - full" : "");
					if (mLanButtons[i]->mLabel != aLabel)
					{
						mLanButtons[i]->mLabel = aLabel;
						mLanButtons[i]->MarkDirty();
					}
				}
				if (mLanButtons[i]->mVisible != aShow)
					mLanButtons[i]->SetVisible(aShow);
			}
			if (s.IsGuestPlaying())
				mApp->KillDialog(kDialogCoop);
		}
		if (mPage == PAGE_HOST)
		{
			bool aKick = s.HasGuest();
			if (mKickButton->mVisible != aKick)
			{
				mKickButton->SetVisible(aKick);
				mCopyButton->SetVisible(!aKick);
				Layout();
			}
			if (mCopiedTimer > 0 && --mCopiedTimer == 0)
				mCopyButton->mLabel = "Copy Address";
			if (!s.IsHosting())
				SetPage(PAGE_MAIN);
		}
		if (mPage == PAGE_MAIN && s.IsHosting())
			SetPage(PAGE_HOST);
		std::string aDiff = std::string("Difficulty: ") + GetTuning(s.GetDifficulty()).mName;
		if (mDiffButton->mLabel != aDiff)
			mDiffButton->mLabel = aDiff;
		MarkDirty();
	}

	void CoopDialog::Draw(Graphics* g)
	{
		MoneyDialog::Draw(g);
		Session& s = S();
		Font* aText = FONT_JUNGLEFEVER10OUTLINE;
		Font* aBig = FONT_JUNGLEFEVER12OUTLINE;
		int cx = mWidth / 2;
		int aBoxX = 34, aBoxW = mWidth - 68;
		auto Centered = [&](Font* f, const std::string& str, int y, const Color& c) {
			g->SetFont(f);
			g->SetColor(c);
			g->DrawString(str, cx - f->StringWidth(str) / 2, y);
		};
		// Word-wrapped, centered paragraph starting at baseline y; returns the next free y.
		auto Para = [&](Font* f, const std::string& str, int y, const Color& c) {
			g->SetFont(f);
			g->SetColor(c);
			int h = WriteWordWrapped(g, Rect(aBoxX, y - f->GetAscent(), aBoxW, 200), str, f->GetLineSpacing(), 0);
			return y + h;
		};
		Color aWhite(255, 255, 255), aGold(255, 220, 90), aCyan(90, 230, 255), aRed(255, 110, 90), aGrey(215, 215, 225);

		if (mPage == PAGE_MAIN)
		{
			Centered(aBig, "Play the same tank together!", 76, aGold);
			Para(aText, "One of you hosts, the other joins from their own computer. You share one tank: collect, feed, zap and shop together. Progress is saved to the host.", 98, aWhite);
			Para(aText, "Both players need their own copy of Insaniquarium Deluxe and the same version of this mod. Middle-click to ping a spot.", 210, aGrey);
			Centered(aText, "Once connected, pick a mode on the main menu.", 268, aGold);
			std::string aNewer = UpdateAvailableVersion();
			std::string aFooter = std::string("Version ") + COOP_VERSION +
				(aNewer.empty() ? "" : " (" + aNewer + " is out!)") +
				". Unofficial fan mod, not endorsed by PopCap or EA.";
			Para(aText, aFooter, 290, aNewer.empty() ? Color(190, 190, 200) : aCyan);
		}
		else if (mPage == PAGE_HOST)
		{
			if (s.HasGuest())
			{
				Centered(aBig, s.PlayerName(1) + " is in your tank!", 76, aCyan);
				char aBuf[64];
				snprintf(aBuf, sizeof(aBuf), "Connection: %d ms", std::max(0, s.GetPingMs()));
				Centered(aText, aBuf, 100, aWhite);
				Para(aText, "Close this window. On the main menu, pick Co-op or Coin Rivals and start Adventure or Time Trial together, or press Versus for Tank Race and Alien Keeper.", 126, aGrey);
			}
			else
			{
				Centered(aBig, "Waiting for player 2...", 76, aGold);
				Para(aText, "Same house? They'll find you under \"Join a Game\". Anywhere else, they type your address:", 98, aWhite);
				std::string aLan, aTail;
				for (const std::string& ip : mLocalIps)
				{
					if (IsTailscaleAddress(ip))
					{
						if (aTail.empty())
							aTail = ip;
					}
					else if (aLan.empty())
						aLan = ip;
				}
				if (aLan.empty())
					aLan = "(no network found)";
				auto Labelled = [&](const std::string& theLabel, const std::string& theValue, int y) {
					int aW = aText->StringWidth(theLabel) + aBig->StringWidth(theValue);
					g->SetFont(aText);
					g->SetColor(aGrey);
					g->DrawString(theLabel, cx - aW / 2, y);
					g->SetFont(aBig);
					g->SetColor(aCyan);
					g->DrawString(theValue, cx - aW / 2 + aText->StringWidth(theLabel), y);
				};
				if (!aTail.empty())
				{
					Labelled("Home network:  ", aLan, 146);
					Labelled("Tailscale:  ", aTail, 164);
				}
				else
					Labelled("Home network:  ", aLan, 150);

				PortMapper& m = s.GetPortMapper();
				PortMapper::State aState = m.GetState();
				std::string anExt = m.GetExternalAddress();
				if (aState == PortMapper::MAPPED && !m.IsSharedAddress() && !anExt.empty())
				{
					int y = aTail.empty() ? 172 : 184;
					Labelled("Internet:  ", anExt, y);
					Centered(aText, "(your router opened the door automatically)", y + 18, aGrey);
				}
				else if (aState == PortMapper::WORKING)
					Centered(aText, "Internet: checking your router...", aTail.empty() ? 172 : 184, aGrey);
				else if (!aTail.empty())
					Centered(aText, "Far away? Your partner can join through Tailscale.", 184, aGrey);
				else if (aState == PortMapper::MAPPED && m.IsSharedAddress())
					Para(aText, "Internet: your provider shares your address, so use Tailscale to play from far away (see the README).", 172, aRed);
				else
				{
					char aBuf[160];
					snprintf(aBuf, sizeof(aBuf), "Internet: open TCP port %d on your router or use Tailscale (see the README).", s.GetPort());
					Para(aText, aBuf, 172, aGrey);
				}
			}
			Centered(aText, "Co-op difficulty:", 290, aGold);
			Para(aText, GetTuning(s.GetDifficulty()).mBlurb, 307, aCyan);
		}
		else if (mPage == PAGE_JOIN)
		{
			Centered(aBig, "Type the host's address:", 76, aGold);
			Centered(aText, "e.g. 192.168.1.23 (same network) or 100.64.1.2 (Tailscale)", 94, aGrey);
			DrawEditWidgetBox(g, mAddressEdit);
			if (s.IsGuest())
				Centered(aText, s.GetStatusLine(), 190, aCyan);
			else if (!mMessage.empty())
				Para(aText, mMessage, 190, mMessageIsError ? aRed : aWhite);
			if (!s.IsGuest())
			{
				if (s.GetDiscovery().GetGames().empty())
					Centered(aText, "Looking for games on your network...", 224, aGrey);
				else
					Centered(aText, "Games on your network - click one to join:", 224, aGrey);
			}
		}
	}

	void CoopDialog::ButtonPress(int theId)
	{
		mApp->PlaySample(SOUND_BUTTONCLICK);
	}

	void CoopDialog::DoConnect(const std::string& theAddress)
	{
		std::string anError;
		mApp->RegistryWriteString("CoopLastAddress", theAddress);
		S().CancelAutoJoin();
		if (!S().StartJoin(theAddress, kDefaultPort, anError))
		{
			mMessage = anError;
			mMessageIsError = true;
		}
		else
		{
			mMessage.clear();
			mMessageIsError = false;
		}
		MarkDirty();
	}

	void CoopDialog::ButtonDepress(int theId)
	{
		MoneyDialog::ButtonDepress(theId);
		Session& s = S();
		// The co-op window belongs to the host: player 2 can see it but not
		// stop the game, kick themselves or change the difficulty.
		if (s.CurrentPlayer() == 1)
			return;
		switch (theId)
		{
		case BTN_HOST:
		{
			std::string anError;
			if (!s.StartHosting(kDefaultPort, anError))
			{
				mApp->DoDialog(94, true, "Can't Host", anError, "OK", BUTTONS_FOOTER);
				return;
			}
			SetPage(PAGE_HOST);
			break;
		}
		case BTN_JOIN:
			mMessage.clear();
			SetPage(PAGE_JOIN);
			break;
		case BTN_PRACTICE:
			HeroesOpenPractice();		// closes this window
			return;
		case BTN_UPDATES:
			SetUpdateCheckEnabled(!UpdateCheckEnabled());
			mUpdateButton->mLabel = UpdateCheckEnabled() ? "Update Check: On" : "Update Check: Off";
			MarkDirty();
			break;
		case BTN_DIFF:
			s.SetDifficulty((s.GetDifficulty() + 1) % DIFF_COUNT);
			mDiffButton->mLabel = std::string("Difficulty: ") + GetTuning(s.GetDifficulty()).mName;
			MarkDirty();
			break;
		case BTN_STOP:
			s.StopHosting();
			SetPage(PAGE_MAIN);
			break;
		case BTN_KICK:
			s.KickGuest();
			Layout();
			break;
		case BTN_COPY:
			mApp->CopyToClipboard(BestAddress());
			mCopyButton->mLabel = "Copied!";
			mCopiedTimer = 200;
			break;
		case BTN_CONNECT:
			if (!s.IsGuest())
				DoConnect(mAddressEdit->mString);
			break;
		case BTN_BACK:
			if (s.IsGuest())
				s.Leave("");
			SetPage(PAGE_MAIN);
			break;
		default:
			if (theId >= BTN_LAN0 && theId < BTN_LAN0 + 3)
			{
				int anIdx = theId - BTN_LAN0;
				if (anIdx < (int)mLanAddresses.size() && !s.IsGuest())
				{
					mAddressEdit->SetText(mLanAddresses[anIdx]);
					DoConnect(mLanAddresses[anIdx]);
				}
			}
			break;
		}
	}

	std::string CoopDialog::BestAddress()
	{
		PortMapper& m = S().GetPortMapper();
		std::string anExt = m.GetExternalAddress();
		if (m.GetState() == PortMapper::MAPPED && !m.IsSharedAddress() && !anExt.empty())
			return anExt;
		for (const std::string& ip : mLocalIps)
			if (IsTailscaleAddress(ip))
				return ip;
		return mLocalIps.empty() ? std::string() : mLocalIps[0];
	}

	void CoopDialog::EditWidgetText(int theId, const std::string& theString)
	{
		if (mPage == PAGE_JOIN && !S().IsGuest())
			DoConnect(theString);
	}

	bool CoopDialog::AllowChar(int theId, char theChar)
	{
		return isalnum((unsigned char)theChar) || theChar == '.' || theChar == ':' || theChar == '-' || theChar == '_';
	}

	///////////////////////////////////////////////////////////////////////////
	GuestMenu::GuestMenu(WinFishApp* theApp)
		: MoneyDialog(theApp, IMAGE_DIALOG, IMAGE_DIALOGBUTTON, kDialogGuestMenu, true, "CO-OP", "", "", BUTTONS_NONE)
	{
		SetHeaderFont(FONT_JUNGLEFEVER15OUTLINE);
		SetColor(COLOR_HEADER, Color(0xff, 200, 0));
		mLeaveButton = MakeButton(BTN_LEAVE, this, "Leave Game");
		mBackButton = MakeButton(BTN_RESUME, this, "Keep Playing");
		mOptionsButton = MakeButton(BTN_OPTIONS, this, "Options");
	}

	GuestMenu::~GuestMenu()
	{
		delete mLeaveButton;
		delete mBackButton;
		delete mOptionsButton;
	}

	void GuestMenu::AddedToManager(WidgetManager* theWidgetManager)
	{
		MoneyDialog::AddedToManager(theWidgetManager);
		theWidgetManager->AddWidget(mLeaveButton);
		theWidgetManager->AddWidget(mBackButton);
		theWidgetManager->AddWidget(mOptionsButton);
	}

	void GuestMenu::RemovedFromManager(WidgetManager* theWidgetManager)
	{
		MoneyDialog::RemovedFromManager(theWidgetManager);
		theWidgetManager->RemoveWidget(mLeaveButton);
		theWidgetManager->RemoveWidget(mBackButton);
		theWidgetManager->RemoveWidget(mOptionsButton);
	}

	int GuestMenu::GetPreferredHeight(int theWidth)
	{
		return 250;
	}

	void GuestMenu::Resize(int theX, int theY, int theWidth, int theHeight)
	{
		MoneyDialog::Resize(theX, theY, theWidth, theHeight);
		int aFull = mWidth - 80;
		int aHalf = (aFull - 8) / 2;
		mBackButton->Resize(mX + 40, mY + mHeight - 120, aFull, 33);
		mOptionsButton->Resize(mX + 40, mY + mHeight - 82, aHalf, 33);
		mLeaveButton->Resize(mX + 40 + aHalf + 8, mY + mHeight - 82, aHalf, 33);
	}

	void GuestMenu::Draw(Graphics* g)
	{
		MoneyDialog::Draw(g);
		Session& s = S();
		Font* f = FONT_JUNGLEFEVER10OUTLINE;
		g->SetFont(f);
		std::string a = "You're playing in " + s.PlayerName(0) + "'s tank.";
		char b[64];
		snprintf(b, sizeof(b), "Ping: %d ms", std::max(0, s.GetPingMs()));
		g->SetColor(Color(255, 255, 255));
		g->DrawString(a, mWidth / 2 - f->StringWidth(a) / 2, 76);
		g->SetColor(Color(200, 200, 210));
		g->DrawString(b, mWidth / 2 - f->StringWidth(b) / 2, 98);
	}

	void GuestMenu::KeyDown(KeyCode theKey)
	{
		if (theKey == KEYCODE_ESCAPE || theKey == KEYCODE_F10)
			mApp->KillDialog(kDialogGuestMenu);
		else
			MoneyDialog::KeyDown(theKey);
	}

	void GuestMenu::ButtonPress(int theId)
	{
		mApp->PlaySample(SOUND_BUTTONCLICK);
	}

	void GuestMenu::ButtonDepress(int theId)
	{
		MoneyDialog::ButtonDepress(theId);
		if (theId == BTN_LEAVE)
		{
			mApp->KillDialog(kDialogGuestMenu);
			S().Leave("");
		}
		else if (theId == BTN_RESUME)
			mApp->KillDialog(kDialogGuestMenu);
		else if (theId == BTN_OPTIONS)
		{
			// This computer's own settings (volume, fullscreen, cursors).
			mApp->KillDialog(kDialogGuestMenu);
			mApp->DoOptionsDialog(true);
		}
	}
}
