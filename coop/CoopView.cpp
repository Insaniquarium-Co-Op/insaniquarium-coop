#include "CoopView.h"
#include "CoopSession.h"
#include "CoopUI.h"
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/KeyCodes.h>
#include <SexyAppFramework/SexyAppBase.h>
#include <SexyAppFramework/WidgetManager.h>

using namespace Sexy;

namespace Coop
{
	RemoteView::RemoteView(bool thePassive)
	{
		mPassive = thePassive;
		mMouseVisible = !thePassive;
		mHasAlpha = false;
		mHasTransparencies = false;
		mClip = false;
		mDoFinger = false;
	}

	void RemoteView::Update()
	{
		Widget::Update();
		MarkDirty();
		// Keep the keyboard on the view whenever no local dialog is up.
		if (!mPassive && mWidgetManager != nullptr && mWidgetManager->mFocusWidget != this && gSexyAppBase->mDialogMap.empty())
			mWidgetManager->SetFocus(this);
	}

	void RemoteView::Draw(Graphics* g)
	{
		S().DrawGuestView(g);
	}

	void RemoteView::MouseMove(int x, int y)
	{
		mLastX = x;
		mLastY = y;
		S().GuestInput(IN_MOVE, x, y, 0);
	}

	void RemoteView::MouseDrag(int x, int y)
	{
		mLastX = x;
		mLastY = y;
		S().GuestInput(IN_MOVE, x, y, 0);
	}

	void RemoteView::MouseDown(int x, int y, int theClickCount)
	{
		mLastX = x;
		mLastY = y;
		S().GuestInput(IN_DOWN, x, y, theClickCount);
	}

	void RemoteView::MouseUp(int x, int y, int theClickCount)
	{
		S().GuestInput(IN_UP, x, y, theClickCount);
	}

	void RemoteView::MouseLeave()
	{
		Widget::MouseLeave();
		S().GuestInput(IN_LEAVE, mLastX, mLastY, 0);
	}

	void RemoteView::MouseWheel(int theDelta)
	{
		S().GuestInput(IN_WHEEL, theDelta, 0, 0);
	}

	void RemoteView::KeyDown(KeyCode theKey)
	{
		// Escape (or F10) is ours: it opens the guest's co-op menu instead of
		// reaching the host's game.
		if (theKey == KEYCODE_ESCAPE || theKey == KEYCODE_F10)
		{
			OpenGuestMenu();
			return;
		}
		S().GuestInput(IN_KEYDOWN, (int)theKey, 0, 0);
	}

	void RemoteView::KeyUp(KeyCode theKey)
	{
		if (theKey == KEYCODE_ESCAPE || theKey == KEYCODE_F10)
			return;
		S().GuestInput(IN_KEYUP, (int)theKey, 0, 0);
	}

	void RemoteView::KeyChar(char theChar)
	{
		if (theChar == 27)
			return;
		S().GuestInput(IN_CHAR, (unsigned char)theChar, 0, 0);
	}
}
