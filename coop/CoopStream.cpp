#include "CoopStream.h"

#include <SexyAppFramework/SexyAppBase.h>
#include <SexyAppFramework/MemoryImage.h>
#include <SexyAppFramework/Image.h>
#include <SexyAppFramework/SexyMatrix.h>
#include <SexyAppFramework/TriVertex.h>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

using namespace Sexy;

namespace Coop
{
	static const uint64_t kPixelBudget = 48ull * 1024 * 1024;	// guest-side cache for runtime images
	static const size_t kMaxTris = 4096;
	static const size_t kMaxPoly = 4096;

	uint64_t HashImage(MemoryImage* theImage)
	{
		const uint32_t* aBits = theImage->GetBits();
		const uint64_t kMul = 0x9E3779B97F4A7C15ull;
		uint64_t h = 0x243F6A8885A308D3ull ^ ((uint64_t)(uint32_t)theImage->mWidth << 32) ^ (uint32_t)theImage->mHeight;
		size_t n = (size_t)theImage->mWidth * (size_t)theImage->mHeight;
		if (aBits == nullptr)
			return h;
		size_t i = 0;
		for (; i + 2 <= n; i += 2)
		{
			uint64_t v = (uint64_t)aBits[i] | ((uint64_t)aBits[i + 1] << 32);
			h ^= v;
			h *= kMul;
			h ^= h >> 31;
		}
		if (i < n)
		{
			h ^= aBits[i];
			h *= kMul;
			h ^= h >> 31;
		}
		h ^= h >> 33;
		h *= 0xFF51AFD7ED558CCDull;
		h ^= h >> 33;
		return h == 0 ? 1 : h;
	}

	///////////////////////////////////////////////////////////////////////////
	// Host
	///////////////////////////////////////////////////////////////////////////
	FrameEncoder::FrameEncoder()
	{
		memset(&mZ, 0, sizeof(mZ));
	}

	FrameEncoder::~FrameEncoder()
	{
		if (mZInit)
			deflateEnd(&mZ);
	}

	void FrameEncoder::Reset(const std::vector<uint64_t>& theGuestHashes)
	{
		if (mZInit)
			deflateEnd(&mZ);
		memset(&mZ, 0, sizeof(mZ));
		// Level 3 keeps a 60 fps stream cheap on the host while still folding each
		// frame against the previous one (32 KB window).
		deflateInit2(&mZ, 3, Z_DEFLATED, 15, 9, Z_DEFAULT_STRATEGY);
		mZInit = true;
		mRecording = false;
		mFrameNum = 0;
		mDefs.Clear();
		mOps.Clear();
		mPendingFrees.clear();
		mEntries.clear();
		mHashToId.clear();
		mSerials.clear();
		mGuestHas.clear();
		mGuestHas.insert(theGuestHashes.begin(), theGuestHashes.end());
		mNextId = 1;
		mPixelDefsSent = 0;
		mPixelBytesLive = 0;
	}

	void FrameEncoder::BeginFrame(Image* theScreen, uint32_t theSeq, int theHostX, int theHostY, int theGuestCursor, uint8_t theFlags)
	{
		mTarget = theScreen;
		mRecording = true;
		mFrameNum++;
		mDefs.Clear();
		mOps.Clear();
		for (uint32_t anId : mPendingFrees)
		{
			mDefs.U8(FC_FREE);
			mDefs.U32(anId);
		}
		mPendingFrees.clear();
		mDefs.U8(FC_HEADER);
		mDefs.U32(theSeq);
		mDefs.I16(theHostX);
		mDefs.I16(theHostY);
		mDefs.U8((uint8_t)theGuestCursor);
		mDefs.U8(theFlags);
	}

	void FrameEncoder::AbortFrame()
	{
		mRecording = false;
		mTarget = nullptr;
	}

	void FrameEncoder::EndFrame(std::vector<uint8_t>& theOut)
	{
		mRecording = false;
		mTarget = nullptr;
		mOps.U8(FC_END);

		mLastRawBytes = (uint32_t)(mDefs.Size() + mOps.Size());

		theOut.clear();
		theOut.resize(deflateBound(&mZ, (uLong)mLastRawBytes) + 64);
		mZ.next_out = theOut.data();
		mZ.avail_out = (uInt)theOut.size();
		const ByteWriter* aParts[2] = { &mDefs, &mOps };
		for (int i = 0; i < 2; i++)
		{
			mZ.next_in = (Bytef*)aParts[i]->mData.data();
			mZ.avail_in = (uInt)aParts[i]->mData.size();
			while (mZ.avail_in > 0)
			{
				if (mZ.avail_out < 1024)
				{
					size_t aUsed = theOut.size() - mZ.avail_out;
					theOut.resize(theOut.size() * 2);
					mZ.next_out = theOut.data() + aUsed;
					mZ.avail_out = (uInt)(theOut.size() - aUsed);
				}
				deflate(&mZ, Z_NO_FLUSH);
			}
		}
		for (;;)
		{
			if (mZ.avail_out < 1024)
			{
				size_t aUsed = theOut.size() - mZ.avail_out;
				theOut.resize(theOut.size() * 2 + 1024);
				mZ.next_out = theOut.data() + aUsed;
				mZ.avail_out = (uInt)(theOut.size() - aUsed);
			}
			deflate(&mZ, Z_SYNC_FLUSH);
			if (mZ.avail_out >= 1024)
				break;
		}
		theOut.resize(theOut.size() - mZ.avail_out);
		Evict();
	}

	void FrameEncoder::Evict()
	{
		if (mPixelBytesLive <= kPixelBudget)
			return;
		std::vector<std::pair<uint32_t, uint32_t>> aCandidates;	// lastFrame, id
		for (auto& kv : mEntries)
			if (kv.second.mPixels && kv.second.mLastFrame != mFrameNum)
				aCandidates.push_back({ kv.second.mLastFrame, kv.first });
		std::sort(aCandidates.begin(), aCandidates.end());
		for (auto& c : aCandidates)
		{
			if (mPixelBytesLive <= kPixelBudget * 3 / 4)
				break;
			auto it = mEntries.find(c.second);
			mPixelBytesLive -= it->second.mBytes;
			auto h = mHashToId.find(it->second.mHash);
			if (h != mHashToId.end() && h->second == c.second)
				mHashToId.erase(h);
			mEntries.erase(it);
			mPendingFrees.push_back(c.second);
		}
		// Forget serial mappings that point at freed ids.
		for (auto it = mSerials.begin(); it != mSerials.end();)
		{
			if (mEntries.find(it->second.mId) == mEntries.end())
				it = mSerials.erase(it);
			else
				++it;
		}
	}

	void FrameEncoder::GuestCannotResolve(uint32_t theId)
	{
		auto it = mEntries.find(theId);
		if (it == mEntries.end())
			return;
		mGuestHas.erase(it->second.mHash);
		auto h = mHashToId.find(it->second.mHash);
		if (h != mHashToId.end() && h->second == theId)
			mHashToId.erase(h);
		if (it->second.mPixels)
			mPixelBytesLive -= it->second.mBytes;
		mEntries.erase(it);
		for (auto s = mSerials.begin(); s != mSerials.end();)
		{
			if (s->second.mId == theId)
				s = mSerials.erase(s);
			else
				++s;
		}
		mPendingFrees.push_back(theId);
	}

	void FrameEncoder::OnImageDestroyed(Image* theImage)
	{
		mSerials.erase(theImage->mCoopSerial);
	}

	uint32_t FrameEncoder::ImageRef(Image* theImage)
	{
		MemoryImage* anImage = dynamic_cast<MemoryImage*>(theImage);
		if (anImage == nullptr || anImage->mWidth <= 0 || anImage->mHeight <= 0)
			return 0;

		auto s = mSerials.find(anImage->mCoopSerial);
		if (s != mSerials.end() && s->second.mChangeCount == (uint32_t)anImage->mBitsChangedCount &&
			s->second.mWidth == anImage->mWidth && s->second.mHeight == anImage->mHeight)
		{
			auto e = mEntries.find(s->second.mId);
			if (e != mEntries.end())
			{
				e->second.mLastFrame = mFrameNum;
				return s->second.mId;
			}
		}

		uint64_t aHash = HashImage(anImage);
		uint32_t anId = 0;
		auto h = mHashToId.find(aHash);
		if (h != mHashToId.end() && mEntries.count(h->second))
		{
			anId = h->second;
		}
		else
		{
			anId = mNextId++;
			if (mNextId == 0)
				mNextId = 1;
			Entry anEntry;
			anEntry.mHash = aHash;
			if (mGuestHas.count(aHash))
			{
				mDefs.U8(FC_DEF_REF);
				mDefs.U32(anId);
				mDefs.U64(aHash);
				anEntry.mPixels = false;
			}
			else
			{
				const uint32_t* aBits = anImage->GetBits();
				int w = std::min(anImage->mWidth, 4096);
				int hgt = std::min(anImage->mHeight, 4096);
				mDefs.U8(FC_DEF_PIX);
				mDefs.U32(anId);
				mDefs.U16((uint16_t)w);
				mDefs.U16((uint16_t)hgt);
				mDefs.U8((anImage->mHasTrans ? 1 : 0) | (anImage->mHasAlpha ? 2 : 0));
				for (int y = 0; y < hgt; y++)
					mDefs.Bytes(aBits + (size_t)y * anImage->mWidth, (size_t)w * 4);
				anEntry.mPixels = true;
				anEntry.mBytes = (uint32_t)w * hgt * 4;
				mPixelBytesLive += anEntry.mBytes;
				mPixelDefsSent++;
			}
			mEntries[anId] = anEntry;
			mHashToId[aHash] = anId;
		}
		SerialInfo anInfo;
		anInfo.mChangeCount = (uint32_t)anImage->mBitsChangedCount;
		anInfo.mId = anId;
		anInfo.mWidth = anImage->mWidth;
		anInfo.mHeight = anImage->mHeight;
		mSerials[anImage->mCoopSerial] = anInfo;
		mEntries[anId].mLastFrame = mFrameNum;
		return anId;
	}

	void FrameEncoder::Rect16(const Rect& r)
	{
		mOps.I16(r.mX);
		mOps.I16(r.mY);
		mOps.I16(r.mWidth);
		mOps.I16(r.mHeight);
	}

	void FrameEncoder::ColorU32(const Color& c)
	{
		uint32_t v = ((uint32_t)(c.mAlpha & 0xFF) << 24) | ((uint32_t)(c.mRed & 0xFF) << 16) | ((uint32_t)(c.mGreen & 0xFF) << 8) | (uint32_t)(c.mBlue & 0xFF);
		mOps.U32(v);
	}

	void FrameEncoder::RecFillRect(const Rect& theRect, const Color& theColor, int theDrawMode)
	{
		if (!mRecording) return;
		mLastOps++;
		mOps.U8(FC_FILLRECT);
		Rect16(theRect);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
	}

	void FrameEncoder::RecLine(double x1, double y1, double x2, double y2, const Color& theColor, int theDrawMode, bool aa)
	{
		if (!mRecording) return;
		mOps.U8(FC_LINE);
		mOps.F32((float)x1); mOps.F32((float)y1); mOps.F32((float)x2); mOps.F32((float)y2);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		mOps.U8(aa ? 1 : 0);
	}

	void FrameEncoder::RecPoly(const Point* theVertices, int theNum, const Rect* theClip, const Color& theColor, int theDrawMode, int tx, int ty)
	{
		if (!mRecording || theNum <= 0 || (size_t)theNum > kMaxPoly) return;
		mOps.U8(FC_POLY);
		mOps.U16((uint16_t)theNum);
		for (int i = 0; i < theNum; i++)
		{
			mOps.I16(theVertices[i].mX);
			mOps.I16(theVertices[i].mY);
		}
		mOps.U8(theClip != nullptr ? 1 : 0);
		if (theClip != nullptr)
			Rect16(*theClip);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		mOps.I16(tx);
		mOps.I16(ty);
	}

	void FrameEncoder::RecBlt(Image* theImage, int x, int y, const Rect& theSrc, const Color& theColor, int theDrawMode, bool linear, bool mirror)
	{
		if (!mRecording) return;
		uint32_t anId = ImageRef(theImage);
		if (anId == 0) return;
		mOps.U8(mirror ? FC_BLTMIRROR : FC_BLT);
		mOps.U32(anId);
		mOps.I16(x);
		mOps.I16(y);
		Rect16(theSrc);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		mOps.U8(linear ? 1 : 0);
	}

	void FrameEncoder::RecBltF(Image* theImage, float x, float y, const Rect& theSrc, const Rect& theClip, const Color& theColor, int theDrawMode)
	{
		if (!mRecording) return;
		uint32_t anId = ImageRef(theImage);
		if (anId == 0) return;
		mOps.U8(FC_BLTF);
		mOps.U32(anId);
		mOps.F32(x);
		mOps.F32(y);
		Rect16(theSrc);
		Rect16(theClip);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
	}

	void FrameEncoder::RecBltRotated(Image* theImage, float x, float y, const Rect& theSrc, const Rect& theClip, const Color& theColor, int theDrawMode, double theRot, float cx, float cy)
	{
		if (!mRecording) return;
		uint32_t anId = ImageRef(theImage);
		if (anId == 0) return;
		mOps.U8(FC_BLTROT);
		mOps.U32(anId);
		mOps.F32(x);
		mOps.F32(y);
		Rect16(theSrc);
		Rect16(theClip);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		mOps.F32((float)theRot);
		mOps.F32(cx);
		mOps.F32(cy);
	}

	void FrameEncoder::RecStretch(Image* theImage, const Rect& theDest, const Rect& theSrc, const Rect& theClip, const Color& theColor, int theDrawMode, bool fast, bool mirror)
	{
		if (!mRecording) return;
		uint32_t anId = ImageRef(theImage);
		if (anId == 0) return;
		mOps.U8(mirror ? FC_STRETCHMIRROR : FC_STRETCH);
		mOps.U32(anId);
		Rect16(theDest);
		Rect16(theSrc);
		Rect16(theClip);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		mOps.U8(fast ? 1 : 0);
	}

	void FrameEncoder::RecMatrix(Image* theImage, float x, float y, const SexyMatrix3& theMatrix, const Rect& theClip, const Color& theColor, int theDrawMode, const Rect& theSrc, bool blend)
	{
		if (!mRecording) return;
		uint32_t anId = ImageRef(theImage);
		if (anId == 0) return;
		mOps.U8(FC_MATRIX);
		mOps.U32(anId);
		mOps.F32(x);
		mOps.F32(y);
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				mOps.F32(theMatrix.m[r][c]);
		Rect16(theClip);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		Rect16(theSrc);
		mOps.U8(blend ? 1 : 0);
	}

	void FrameEncoder::RecTris(Image* theImage, const TriVertex theVertices[][3], int theNum, const Rect& theClip, const Color& theColor, int theDrawMode, float tx, float ty, bool blend)
	{
		if (!mRecording || theNum <= 0 || (size_t)theNum > kMaxTris) return;
		uint32_t anId = ImageRef(theImage);
		if (anId == 0) return;
		mOps.U8(FC_TRIS);
		mOps.U32(anId);
		mOps.U16((uint16_t)theNum);
		for (int i = 0; i < theNum; i++)
			for (int v = 0; v < 3; v++)
			{
				const TriVertex& tv = theVertices[i][v];
				mOps.F32(tv.x); mOps.F32(tv.y); mOps.F32(tv.u); mOps.F32(tv.v);
				mOps.U32(tv.color);
			}
		Rect16(theClip);
		ColorU32(theColor);
		mOps.U8((uint8_t)theDrawMode);
		mOps.F32(tx);
		mOps.F32(ty);
		mOps.U8(blend ? 1 : 0);
	}

	///////////////////////////////////////////////////////////////////////////
	// Guest
	///////////////////////////////////////////////////////////////////////////
	FrameDecoder::FrameDecoder()
	{
		memset(&mZ, 0, sizeof(mZ));
	}

	FrameDecoder::~FrameDecoder()
	{
		Reset();
		if (mZInit)
			inflateEnd(&mZ);
	}

	void FrameDecoder::Reset()
	{
		for (auto& kv : mSlots)
			delete kv.second.mOwned;
		mSlots.clear();
		if (mZInit)
			inflateEnd(&mZ);
		memset(&mZ, 0, sizeof(mZ));
		inflateInit2(&mZ, 15);
		mZInit = true;
		mFrameOps.clear();
		mHaveFrame = false;
		mUnresolved.clear();
		mFramesDecoded = 0;
		mError.clear();
		mLocal.clear();
		mLocalRev.clear();
		mLocalIndexed = false;
	}

	void FrameDecoder::CollectLocalHashes(std::vector<uint64_t>& theOut)
	{
		theOut.clear();
		ForEachMemoryImage([&](MemoryImage* anImage) {
			if (anImage->mWidth > 0 && anImage->mHeight > 0 && anImage->mWidth <= 4096 && anImage->mHeight <= 4096)
				theOut.push_back(HashImage(anImage));
		});
		std::sort(theOut.begin(), theOut.end());
		theOut.erase(std::unique(theOut.begin(), theOut.end()), theOut.end());
	}

	void FrameDecoder::EnsureLocalIndex()
	{
		if (mLocalIndexed)
			return;
		mLocalIndexed = true;
		// Test hook: "forget" some local images so references can't resolve.
		int aForget = getenv("INSANIQ_TEST_FORGET") ? std::max(2, atoi(getenv("INSANIQ_TEST_FORGET"))) : 0;
		int aCounter = 0;
		ForEachMemoryImage([&](MemoryImage* anImage) {
			if (aForget > 0 && (++aCounter % aForget) == 0)
				return;
			if (anImage->mWidth > 0 && anImage->mHeight > 0 && anImage->mWidth <= 4096 && anImage->mHeight <= 4096)
			{
				// Our own streamed copies are not "local assets".
				for (auto& kv : mSlots)
					if (kv.second.mOwned == anImage)
						return;
				uint64_t h = HashImage(anImage);
				mLocal.emplace(h, anImage);
				mLocalRev[anImage] = h;
			}
		});
	}

	void FrameDecoder::OnImageDestroyed(Image* theImage)
	{
		auto r = mLocalRev.find(theImage);
		if (r != mLocalRev.end())
		{
			auto l = mLocal.find(r->second);
			if (l != mLocal.end() && l->second == theImage)
				mLocal.erase(l);
			mLocalRev.erase(r);
		}
		for (auto& kv : mSlots)
			if (kv.second.mImage == theImage && kv.second.mOwned == nullptr)
				kv.second.mImage = nullptr;
	}

	void FrameDecoder::FreeSlot(uint32_t theId)
	{
		auto it = mSlots.find(theId);
		if (it == mSlots.end())
			return;
		MemoryImage* anOwned = it->second.mOwned;
		mSlots.erase(it);
		delete anOwned;
	}

	Image* FrameDecoder::Lookup(uint32_t theId)
	{
		auto it = mSlots.find(theId);
		if (it == mSlots.end())
			return nullptr;
		return it->second.mImage;
	}

	bool FrameDecoder::Decode(const uint8_t* theData, size_t theSize)
	{
		mZ.next_in = (Bytef*)theData;
		mZ.avail_in = (uInt)theSize;
		mRaw.clear();
		uint8_t aBuf[64 * 1024];
		while (mZ.avail_in > 0 || mZ.avail_out == 0)
		{
			mZ.next_out = aBuf;
			mZ.avail_out = sizeof(aBuf);
			int r = inflate(&mZ, Z_SYNC_FLUSH);
			size_t aGot = sizeof(aBuf) - mZ.avail_out;
			mRaw.insert(mRaw.end(), aBuf, aBuf + aGot);
			if (r == Z_BUF_ERROR && aGot == 0)
				break;
			if (r != Z_OK && r != Z_BUF_ERROR)
			{
				mError = "stream corrupt (zlib " + std::to_string(r) + ")";
				return false;
			}
			if (mZ.avail_in == 0 && mZ.avail_out != 0)
				break;
		}
		return ParseCommands(mRaw.data(), mRaw.size());
	}

	// Walks one frame: applies defs/frees, and keeps the op section for replay.
	bool FrameDecoder::ParseCommands(const uint8_t* p, size_t n)
	{
		ByteReader r(p, n);
		size_t anOpsStart = 0;
		bool aSawHeader = false;
		while (!r.AtEnd())
		{
			uint8_t aCmd = r.U8();
			if (aCmd == FC_FREE)
			{
				FreeSlot(r.U32());
			}
			else if (aCmd == FC_DEF_REF)
			{
				uint32_t anId = r.U32();
				uint64_t aHash = r.U64();
				FreeSlot(anId);
				EnsureLocalIndex();
				Slot aSlot;
				auto l = mLocal.find(aHash);
				if (l != mLocal.end())
					aSlot.mImage = l->second;
				else
					mUnresolved.push_back(anId);
				mSlots[anId] = aSlot;
			}
			else if (aCmd == FC_DEF_PIX)
			{
				uint32_t anId = r.U32();
				int w = r.U16();
				int h = r.U16();
				uint8_t aFlags = r.U8();
				const uint8_t* aPix = r.Bytes((size_t)w * h * 4);
				if (aPix == nullptr)
					break;
				FreeSlot(anId);
				MemoryImage* anImage = new MemoryImage(gSexyAppBase);
				anImage->Create(w, h);
				memcpy(anImage->GetBits(), aPix, (size_t)w * h * 4);
				anImage->mHasTrans = (aFlags & 1) != 0;
				anImage->mHasAlpha = (aFlags & 2) != 0;
				anImage->BitsChanged();
				Slot aSlot;
				aSlot.mImage = anImage;
				aSlot.mOwned = anImage;
				mSlots[anId] = aSlot;
			}
			else if (aCmd == FC_HEADER)
			{
				mSeq = r.U32();
				mHostX = r.I16();
				mHostY = r.I16();
				mGuestCursor = r.U8();
				mFlags = r.U8();
				aSawHeader = true;
				anOpsStart = r.mPos;
			}
			else
			{
				// First drawing op: everything from here to FC_END is the display list.
				if (!aSawHeader)
				{
					mError = "frame without header";
					return false;
				}
				size_t aStart = r.mPos - 1;
				// Defs never follow ops within a frame, so the rest is ops.
				(void)anOpsStart;
				mFrameOps.assign(p + aStart, p + n);
				mHaveFrame = true;
				mFramesDecoded++;
				return true;
			}
			if (r.mError)
				break;
		}
		if (r.mError)
		{
			mError = "truncated frame";
			return false;
		}
		// A frame with no drawing ops (only END) is still a frame.
		if (aSawHeader)
		{
			mFrameOps.clear();
			mFrameOps.push_back(FC_END);
			mHaveFrame = true;
			mFramesDecoded++;
		}
		return true;
	}

	static Rect ReadRect(ByteReader& r)
	{
		Rect aRect;
		aRect.mX = r.I16();
		aRect.mY = r.I16();
		aRect.mWidth = r.I16();
		aRect.mHeight = r.I16();
		return aRect;
	}

	static Color ReadColor(ByteReader& r)
	{
		uint32_t v = r.U32();
		return Color((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, (v >> 24) & 0xFF);
	}

	void FrameDecoder::Replay(Image* theScreen)
	{
		if (!mHaveFrame)
			return;
		ByteReader r(mFrameOps.data(), mFrameOps.size());
		std::vector<Point> aPoints;
		std::vector<TriVertex> aTris;
		while (!r.AtEnd() && !r.mError)
		{
			uint8_t anOp = r.U8();
			switch (anOp)
			{
			case FC_END:
				return;
			case FC_FILLRECT:
			{
				Rect aRect = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				if (!r.mError) theScreen->FillRect(aRect, aColor, aMode);
				break;
			}
			case FC_LINE:
			{
				float x1 = r.F32(), y1 = r.F32(), x2 = r.F32(), y2 = r.F32();
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				bool aa = r.U8() != 0;
				if (r.mError) break;
				if (aa) theScreen->DrawLineAA(x1, y1, x2, y2, aColor, aMode);
				else theScreen->DrawLine(x1, y1, x2, y2, aColor, aMode);
				break;
			}
			case FC_POLY:
			{
				int aNum = r.U16();
				aPoints.resize(aNum);
				for (int i = 0; i < aNum; i++)
				{
					aPoints[i].mX = r.I16();
					aPoints[i].mY = r.I16();
				}
				bool hasClip = r.U8() != 0;
				Rect aClip;
				if (hasClip) aClip = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				int tx = r.I16(), ty = r.I16();
				if (!r.mError && aNum > 0) theScreen->PolyFill3D(aPoints.data(), aNum, hasClip ? &aClip : nullptr, aColor, aMode, tx, ty);
				break;
			}
			case FC_BLT:
			case FC_BLTMIRROR:
			{
				Image* anImage = Lookup(r.U32());
				int x = r.I16(), y = r.I16();
				Rect aSrc = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				bool aLinear = r.U8() != 0;
				if (r.mError || anImage == nullptr) break;
				if (anOp == FC_BLT) theScreen->Blt(anImage, x, y, aSrc, aColor, aMode, aLinear);
				else theScreen->BltMirror(anImage, x, y, aSrc, aColor, aMode, aLinear);
				break;
			}
			case FC_BLTF:
			{
				Image* anImage = Lookup(r.U32());
				float x = r.F32(), y = r.F32();
				Rect aSrc = ReadRect(r);
				Rect aClip = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				if (!r.mError && anImage != nullptr) theScreen->BltF(anImage, x, y, aSrc, aClip, aColor, aMode);
				break;
			}
			case FC_BLTROT:
			{
				Image* anImage = Lookup(r.U32());
				float x = r.F32(), y = r.F32();
				Rect aSrc = ReadRect(r);
				Rect aClip = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				float aRot = r.F32(), cx = r.F32(), cy = r.F32();
				if (!r.mError && anImage != nullptr) theScreen->BltRotated(anImage, x, y, aSrc, aClip, aColor, aMode, aRot, cx, cy);
				break;
			}
			case FC_STRETCH:
			case FC_STRETCHMIRROR:
			{
				Image* anImage = Lookup(r.U32());
				Rect aDest = ReadRect(r);
				Rect aSrc = ReadRect(r);
				Rect aClip = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				bool aFast = r.U8() != 0;
				if (r.mError || anImage == nullptr) break;
				if (anOp == FC_STRETCH) theScreen->StretchBlt(anImage, aDest, aSrc, aClip, aColor, aMode, aFast);
				else theScreen->StretchBltMirror(anImage, aDest, aSrc, aClip, aColor, aMode, aFast);
				break;
			}
			case FC_MATRIX:
			{
				Image* anImage = Lookup(r.U32());
				float x = r.F32(), y = r.F32();
				SexyMatrix3 aMatrix;
				for (int i = 0; i < 3; i++)
					for (int j = 0; j < 3; j++)
						aMatrix.m[i][j] = r.F32();
				Rect aClip = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				Rect aSrc = ReadRect(r);
				bool aBlend = r.U8() != 0;
				if (!r.mError && anImage != nullptr) theScreen->BltMatrix(anImage, x, y, aMatrix, aClip, aColor, aMode, aSrc, aBlend);
				break;
			}
			case FC_TRIS:
			{
				Image* anImage = Lookup(r.U32());
				int aNum = r.U16();
				aTris.resize((size_t)aNum * 3);
				for (int i = 0; i < aNum * 3; i++)
				{
					TriVertex& tv = aTris[i];
					tv.x = r.F32(); tv.y = r.F32(); tv.u = r.F32(); tv.v = r.F32();
					tv.color = r.U32();
				}
				Rect aClip = ReadRect(r);
				Color aColor = ReadColor(r);
				int aMode = r.U8();
				float tx = r.F32(), ty = r.F32();
				bool aBlend = r.U8() != 0;
				if (!r.mError && anImage != nullptr && aNum > 0)
					theScreen->BltTrianglesTex(anImage, (const TriVertex(*)[3])aTris.data(), aNum, aClip, aColor, aMode, tx, ty, aBlend);
				break;
			}
			default:
				// Unknown op: the rest of this frame can't be parsed safely.
				return;
			}
		}
	}
}
