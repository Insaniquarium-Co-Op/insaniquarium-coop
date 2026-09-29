#include <SexyAppFramework/Font.h>
#include "CoopVersus.h"
#include "CoopUI.h"
#include "CoopRace.h"
#include "CoopHeroes.h"
#include "CoopSession.h"

#include "WinFishApp.h"
#include "WinFishCommon.h"
#include "Res.h"

#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/DialogButton.h>
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Widget.h>
#include <algorithm>
#include <cstdio>
#include <string>

using namespace Sexy;

namespace Coop
{
	enum
	{
		VB_RACE = 1,
		VB_KEEPER,
		VB_TANK1,		// .. VB_TANK1 + 3
		VB_CATCH = 10,
		VB_START,
		VB_HEROES,
		MB_COOP = 20,
		MB_RIVALS,
		MB_VERSUS,
	};

	static WinFishApp* App() { return (WinFishApp*)gSexyAppBase; }

	// The Versus mode picked on this screen (Pet Heroes is separate from the race settings).
	static bool sHeroes = false;

	// Only the host drives these screens; player 2's clicks arrive with CurrentPlayer() == 1.
	static bool FromGuest() { return S().CurrentPlayer() == 1; }

	static void SetLabel(DialogButton* b, const std::string& theLabel)
	{
		if (b->mLabel != theLabel)
		{
			b->mLabel = theLabel;
			b->MarkDirty();
		}
	}

	// The chosen option of a group: a bright gold label; the others dimmer.
	static void SetPicked(DialogButton* b, const std::string& theLabel, bool theOn)
	{
		SetLabel(b, theLabel);
		Color c = theOn ? Color(255, 235, 60) : Color(190, 180, 210);
		if (b->mColors[ButtonWidget::COLOR_LABEL] != c)
		{
			b->mColors[ButtonWidget::COLOR_LABEL] = c;
			b->mColors[ButtonWidget::COLOR_LABEL_HILITE] = theOn ? c : Color(255, 255, 255);
			b->MarkDirty();
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// The Versus screen
	///////////////////////////////////////////////////////////////////////////
	class VersusDialog : public MoneyDialog
	{
	public:
		DialogButton*	mRaceButton;
		DialogButton*	mKeeperButton;
		DialogButton*	mHeroesButton;
		DialogButton*	mTankButtons[4];
		DialogButton*	mCatchButton;
		DialogButton*	mStartButton;

		VersusDialog(WinFishApp* theApp)
			: MoneyDialog(theApp, IMAGE_DIALOG, IMAGE_DIALOGBUTTON, kDialogVersus, true, "VERSUS", "", "Close", BUTTONS_FOOTER)
		{
			SetHeaderFont(FONT_JUNGLEFEVER17OUTLINE ? FONT_JUNGLEFEVER17OUTLINE : FONT_JUNGLEFEVER15OUTLINE);
			SetColor(COLOR_HEADER, Color(0xff, 200, 0));
			mRaceButton = MakeDialogButton(VB_RACE, this, "", FONT_JUNGLEFEVER12OUTLINE);
			mKeeperButton = MakeDialogButton(VB_KEEPER, this, "", FONT_JUNGLEFEVER12OUTLINE);
			mHeroesButton = MakeDialogButton(VB_HEROES, this, "", FONT_JUNGLEFEVER12OUTLINE);
			for (int i = 0; i < 4; i++)
				mTankButtons[i] = MakeDialogButton(VB_TANK1 + i, this, "", FONT_JUNGLEFEVER12OUTLINE);
			mCatchButton = MakeDialogButton(VB_CATCH, this, "", FONT_JUNGLEFEVER12OUTLINE);
			mStartButton = MakeDialogButton(VB_START, this, "", FONT_JUNGLEFEVER15OUTLINE);
			Sync();
		}

		virtual ~VersusDialog()
		{
			delete mRaceButton;
			delete mKeeperButton;
			delete mHeroesButton;
			for (int i = 0; i < 4; i++)
				delete mTankButtons[i];
			delete mCatchButton;
			delete mStartButton;
		}

		virtual void AddedToManager(WidgetManager* theManager) override
		{
			MoneyDialog::AddedToManager(theManager);
			theManager->AddWidget(mRaceButton);
			theManager->AddWidget(mKeeperButton);
			theManager->AddWidget(mHeroesButton);
			for (int i = 0; i < 4; i++)
				theManager->AddWidget(mTankButtons[i]);
			theManager->AddWidget(mCatchButton);
			theManager->AddWidget(mStartButton);
			Layout();
		}

		virtual void RemovedFromManager(WidgetManager* theManager) override
		{
			MoneyDialog::RemovedFromManager(theManager);
			theManager->RemoveWidget(mRaceButton);
			theManager->RemoveWidget(mKeeperButton);
			theManager->RemoveWidget(mHeroesButton);
			for (int i = 0; i < 4; i++)
				theManager->RemoveWidget(mTankButtons[i]);
			theManager->RemoveWidget(mCatchButton);
			theManager->RemoveWidget(mStartButton);
		}

		virtual int GetPreferredHeight(int theWidth) override { return 450; }

		virtual void Resize(int theX, int theY, int theWidth, int theHeight) override
		{
			MoneyDialog::Resize(theX, theY, theWidth, theHeight);
			Layout();
		}

		void Layout()
		{
			int aLeft = mX + 36, aWidth = mWidth - 72, aHalf = (aWidth - 8) / 2, aBtnH = 33;
			int aThird = (aWidth - 16) / 3;
			mRaceButton->Resize(aLeft, mY + 88, aThird, aBtnH);
			mKeeperButton->Resize(aLeft + aThird + 8, mY + 88, aThird, aBtnH);
			mHeroesButton->Resize(aLeft + 2 * (aThird + 8), mY + 88, aThird, aBtnH);
			(void)aHalf;
			int aQuarter = (aWidth - 24) / 4;
			for (int i = 0; i < 4; i++)
				mTankButtons[i]->Resize(aLeft + i * (aQuarter + 8), mY + 214, aQuarter, aBtnH);
			mCatchButton->Resize(aLeft + 60, mY + 290, aWidth - 120, aBtnH);
			mStartButton->Resize(aLeft + 40, mY + 336, aWidth - 80, 38);
		}

		void Sync()
		{
			bool aKeeper = VersusIsKeeper() && !sHeroes;
			SetPicked(mRaceButton, "Tank Race", !aKeeper && !sHeroes);
			SetPicked(mKeeperButton, "Alien Keeper", aKeeper);
			SetPicked(mHeroesButton, "Pet Heroes", sHeroes);
			for (int i = 0; i < 4; i++)
			{
				SetPicked(mTankButtons[i], "Tank " + std::to_string(i + 1), RaceTank() == i + 1);
				if (mTankButtons[i]->mVisible == sHeroes)
					mTankButtons[i]->SetVisible(!sHeroes);
			}
			SetLabel(mCatchButton, std::string("Catch-up: ") + (RaceCatchUp() ? "On" : "Off"));
			bool aCatch = !aKeeper && !sHeroes;
			if (mCatchButton->mVisible != aCatch)
				mCatchButton->SetVisible(aCatch);
			SetLabel(mStartButton, RaceBusy() ? "Call It Off" : (sHeroes ? "Start Match!" : (aKeeper ? "Start Round!" : "Start Race!")));
		}

		virtual void Update() override
		{
			MoneyDialog::Update();
			Sync();
			MarkDirty();
		}

		virtual void Draw(Graphics* g) override
		{
			MoneyDialog::Draw(g);
			Session& s = S();
			Font* aText = FONT_JUNGLEFEVER10OUTLINE;
			Font* aBig = FONT_JUNGLEFEVER12OUTLINE;
			int cx = mWidth / 2;
			auto Centered = [&](Font* f, const std::string& str, int y, const Color& c) {
				g->SetFont(f);
				g->SetColor(c);
				g->DrawString(str, cx - f->StringWidth(str) / 2, y);
			};
			auto Para = [&](Font* f, const std::string& str, int y, const Color& c) {
				g->SetFont(f);
				g->SetColor(c);
				return y + WriteWordWrapped(g, Rect(34, y - f->GetAscent(), mWidth - 68, 200), str, f->GetLineSpacing(), 0);
			};
			Color aWhite(255, 255, 255), aGold(255, 220, 90), aCyan(90, 230, 255), aRed(255, 110, 90), aGrey(215, 215, 225);
			bool aKeeper = VersusIsKeeper();
			Centered(aBig, "You vs " + s.PlayerName(1), 76, aCyan);
			if (sHeroes)
			{
				Para(aText, "A MOBA on two tanks! Each of you picks a pet hero, farms your own tank for money, and sends minions and your hero through the portal to break the other's towers and core.", 142, aWhite);
				Para(aText, "Right-click to move and attack, Q W E R abilities, B shop, Tab to look home. 15-20 minutes. (Practice against the bot from the Co-op window.)", 232, aGrey);
				std::string aWhy;
				if (!HeroesCanStart(aWhy))
					Centered(aText, aWhy, 392, aRed);
				return;
			}
			if (aKeeper)
				Para(aText, "One keeps the fish tank, the other raises aliens in their own tank and sends them through to wipe the fish out. " +
					s.PlayerName(RaceKeeperNextFishKeeper()) + " keeps the fish next; roles swap every round.", 142, aWhite);
			else
				Para(aText, "A tank each: race to the egg and spend spare money sabotaging each other.", 142, aWhite);
			int n = RaceTank();
			char aBuf[160];
			snprintf(aBuf, sizeof(aBuf), "Tank %d plays as its final Adventure level (%d-5). Pets: pick 3 of the %d you'd have by then.", n, n, 5 * n - 1);
			Para(aText, aBuf, 262, aGrey);
			std::string aWhy;
			if (!RaceBusy() && !RaceCanStart(aWhy))
				Centered(aText, aWhy, 392, aRed);
			else if (RaceWins(0) + RaceWins(1) > 0)
			{
				snprintf(aBuf, sizeof(aBuf), "Score: %s %d - %d %s", s.PlayerName(0).c_str(), RaceWins(0), RaceWins(1), s.PlayerName(1).c_str());
				Centered(aText, aBuf, 392, aGold);
			}
		}

		virtual void ButtonDepress(int theId) override
		{
			MoneyDialog::ButtonDepress(theId);
			if (FromGuest())
				return;
			if (theId == VB_RACE || theId == VB_KEEPER)
			{
				sHeroes = false;
				SetVersusKeeper(theId == VB_KEEPER);
			}
			else if (theId == VB_HEROES)
				sHeroes = true;
			else if (theId >= VB_TANK1 && theId < VB_TANK1 + 4)
				SetRaceTank(theId - VB_TANK1 + 1);
			else if (theId == VB_CATCH)
				SetRaceCatchUp(!RaceCatchUp());
			else if (theId == VB_START && sHeroes && !RaceBusy())
			{
				std::string aWhy;
				if (HeroesCanStart(aWhy))
					HeroesHostStart();		// closes this window
				return;
			}
			else if (theId == VB_START)
			{
				if (RaceBusy())
				{
					RaceHostCancel();
					return;
				}
				std::string aWhy;
				if (!RaceCanStart(aWhy))
					return;
				SetRaceLevel(5);
				RaceHostStart();	// closes this window
				return;
			}
			Sync();
		}
	};

	void OpenVersusDialog(WinFishApp* theApp)
	{
		theApp->KillDialog(kDialogVersus);
		VersusDialog* aDialog = new VersusDialog(theApp);
		int aWidth = 460;
		int aHeight = aDialog->GetPreferredHeight(aWidth);
		aDialog->Resize(320 - aWidth / 2, std::max(4, 244 - aHeight / 2), aWidth, aHeight);
		theApp->AddDialog(kDialogVersus, aDialog);
	}

	///////////////////////////////////////////////////////////////////////////
	// The multiplayer panel on the main menu (host with a guest connected)
	///////////////////////////////////////////////////////////////////////////
	static const int kPanelX = 22, kPanelY = 300, kPanelW = 284, kPanelH = 170;

	class MenuPanel : public Widget, public ButtonListener
	{
	public:
		DialogButton*	mCoopButton;
		DialogButton*	mRivalsButton;
		DialogButton*	mVersusButton;
		bool			mShown = false;

		MenuPanel()
		{
			mMouseVisible = false;
			mHasAlpha = true;
			mHasTransparencies = true;
			mCoopButton = MakeDialogButton2(MB_COOP, this, "Co-op", IMAGE_MAINBUTTON);
			mRivalsButton = MakeDialogButton2(MB_RIVALS, this, "Coin Rivals", IMAGE_MAINBUTTON);
			mVersusButton = MakeDialogButton2(MB_VERSUS, this, "VERSUS!", IMAGE_MAINBUTTON);
			mVersusButton->SetFont(FONT_JUNGLEFEVER15OUTLINE);
			mVersusButton->SetColor(0, Color(255, 120, 90));
			Resize(kPanelX, kPanelY, kPanelW, kPanelH);
			mCoopButton->Resize(kPanelX + 12, kPanelY + 64, 126, mCoopButton->mHeight);
			mRivalsButton->Resize(kPanelX + 146, kPanelY + 64, 126, mRivalsButton->mHeight);
			mVersusButton->Resize(kPanelX + 12, kPanelY + 118, kPanelW - 24, 40);
			SetShown(false);
		}

		virtual ~MenuPanel()
		{
			delete mCoopButton;
			delete mRivalsButton;
			delete mVersusButton;
		}

		void SetShown(bool theShown)
		{
			mShown = theShown;
			SetVisible(theShown);
			mCoopButton->SetVisible(theShown);
			mRivalsButton->SetVisible(theShown);
			mVersusButton->SetVisible(theShown);
		}

		void AddButtons(WidgetManager* theManager)
		{
			theManager->AddWidget(mCoopButton);
			theManager->AddWidget(mRivalsButton);
			theManager->AddWidget(mVersusButton);
		}

		void RemoveButtons(WidgetManager* theManager)
		{
			theManager->RemoveWidget(mCoopButton);
			theManager->RemoveWidget(mRivalsButton);
			theManager->RemoveWidget(mVersusButton);
		}

		void ToFront(WidgetManager* theManager, Widget* theSelector)
		{
			theManager->PutInfront(this, theSelector);
			theManager->PutInfront(mCoopButton, this);
			theManager->PutInfront(mRivalsButton, this);
			theManager->PutInfront(mVersusButton, this);
		}

		virtual void Update() override
		{
			Widget::Update();
			Session& s = S();
			bool aShow = s.HasGuest() && !RaceBusy();
			if (aShow != mShown)
				SetShown(aShow);
			bool aRivals = s.GetMode() == MODE_RIVALS;
			SetPicked(mCoopButton, "Co-op", !aRivals);
			SetPicked(mRivalsButton, "Coin Rivals", aRivals);
			if (mShown)
				MarkDirty();
		}

		virtual void Draw(Graphics* g) override
		{
			Session& s = S();
			g->SetColor(Color(10, 20, 60, 205));
			g->FillRect(0, 0, mWidth, mHeight);
			g->SetColor(Color(90, 230, 255));
			g->DrawRect(0, 0, mWidth - 1, mHeight - 1);
			g->DrawRect(1, 1, mWidth - 3, mHeight - 3);
			Font* aBig = FONT_JUNGLEFEVER12OUTLINE;
			Font* aText = FONT_JUNGLEFEVER10OUTLINE;
			auto Centered = [&](Font* f, const std::string& str, int y, const Color& c) {
				g->SetFont(f);
				g->SetColor(c);
				g->DrawString(str, mWidth / 2 - f->StringWidth(str) / 2, y);
			};
			Centered(aBig, "Playing with " + s.PlayerName(1), 22, Color(90, 230, 255));
			Centered(aText, "Adventure & Time Trial together:", 48, Color(255, 220, 90));
			Centered(aText, "Or face off:", 110, Color(255, 220, 90));
		}

		virtual void ButtonDepress(int theId) override
		{
			if (FromGuest())
				return;
			if (theId == MB_COOP || theId == MB_RIVALS)
				S().SetMode(theId == MB_RIVALS ? MODE_RIVALS : MODE_COOP);
			else if (theId == MB_VERSUS)
				OpenVersusDialog(App());
		}
	};

	static MenuPanel* gMenuPanel = nullptr;

	void AttachMenuPanel(WidgetManager* theManager)
	{
		if (gMenuPanel == nullptr)
			gMenuPanel = new MenuPanel();
		theManager->AddWidget(gMenuPanel);
		gMenuPanel->AddButtons(theManager);
	}

	void DetachMenuPanel(WidgetManager* theManager)
	{
		if (gMenuPanel == nullptr)
			return;
		gMenuPanel->RemoveButtons(theManager);
		theManager->RemoveWidget(gMenuPanel);
	}

	void MenuPanelToFront(WidgetManager* theManager, Widget* theSelector)
	{
		if (gMenuPanel != nullptr && gMenuPanel->mWidgetManager == theManager)
			gMenuPanel->ToFront(theManager, theSelector);
	}
}
