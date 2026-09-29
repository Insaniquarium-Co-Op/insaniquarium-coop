// Pet Heroes - what the two sides say to each other, and the Link that carries it
// (the game's TCP session in a network match; an in-memory queue in practice and in
// the headless tests).

#ifndef __HEROES_NET_H__
#define __HEROES_NET_H__

#include "HeroesCore.h"
#include <cstring>
#include <deque>
#include <memory>

namespace Heroes
{
	enum HeroMsg : uint8_t
	{
		HM_HERO = 1,		// HeroSnap (20 Hz)
		HM_ARENA,			// ArenaSnap (15 Hz while the rival hero is in it, else 3 Hz)
		HM_HITS,			// u8 count, { EntityRef, Hit }
		HM_REWARDS,			// u8 count, Reward
		HM_EVENTS,			// u8 count, Event
		HM_WAVE,			// u8 from team, f32 mult, u8 count, kinds
		HM_STATUS,			// u8 status (ST_*): my core fell / I give up
	};
	enum HeroStatus : uint8_t { ST_CORE_DEAD = 1, ST_GAVE_UP };

	class Writer
	{
	public:
		std::vector<uint8_t> mData;
		void U8(uint8_t v) { mData.push_back(v); }
		void I8(int8_t v) { U8((uint8_t)v); }
		void U16(uint16_t v) { U8((uint8_t)v); U8((uint8_t)(v >> 8)); }
		void U32(uint32_t v) { U16((uint16_t)v); U16((uint16_t)(v >> 16)); }
		void F32(float v) { uint32_t u; std::memcpy(&u, &v, 4); U32(u); }
		void Pos(Vec v) { F32(v.x); F32(v.y); }
		void Str(const std::string& s) { U8((uint8_t)std::min<size_t>(s.size(), 255)); for (size_t i = 0; i < std::min<size_t>(s.size(), 255); i++) U8((uint8_t)s[i]); }
	};

	class Reader
	{
	public:
		const uint8_t*	mData;
		size_t			mSize, mAt = 0;
		bool			mBad = false;
		Reader(const uint8_t* theData, size_t theSize) : mData(theData), mSize(theSize) {}
		explicit Reader(const std::vector<uint8_t>& v) : mData(v.data()), mSize(v.size()) {}
		uint8_t U8() { if (mAt >= mSize) { mBad = true; return 0; } return mData[mAt++]; }
		int8_t I8() { return (int8_t)U8(); }
		uint16_t U16() { uint16_t a = U8(); return (uint16_t)(a | (U8() << 8)); }
		uint32_t U32() { uint32_t a = U16(); return a | ((uint32_t)U16() << 16); }
		float F32() { uint32_t u = U32(); float v; std::memcpy(&v, &u, 4); return v; }
		Vec Pos() { float x = F32(); return Vec(x, F32()); }
		std::string Str() { size_t n = U8(); std::string s; for (size_t i = 0; i < n && !mBad; i++) s.push_back((char)U8()); return s; }
		bool Done() const { return mAt >= mSize; }
	};

	void	Put(Writer& w, const EntityRef& v);
	void	Put(Writer& w, const Hit& v);
	void	Put(Writer& w, const Reward& v);
	void	Put(Writer& w, const Event& v);
	void	Put(Writer& w, const HeroSnap& v);
	void	Put(Writer& w, const ArenaSnap& v);
	bool	Get(Reader& r, EntityRef& v);
	bool	Get(Reader& r, Hit& v);
	bool	Get(Reader& r, Reward& v);
	bool	Get(Reader& r, Event& v);
	bool	Get(Reader& r, HeroSnap& v);
	bool	Get(Reader& r, ArenaSnap& v);

	struct Packet
	{
		uint8_t					mType = 0;
		std::vector<uint8_t>	mData;
	};

	// One direction pair between two sides.
	class Link
	{
	public:
		virtual ~Link() {}
		virtual void	Send(uint8_t theType, const std::vector<uint8_t>& theData) = 0;
		virtual bool	Receive(Packet& thePacket) = 0;		// false: nothing waiting
	};

	// Two ends of an in-memory connection, with optional delay (ms of the clock
	// passed to SetNow) to test lag.
	class MemoryLink : public Link
	{
	public:
		struct Pipe { std::deque<std::pair<uint32_t, Packet>> mQueue; };
		static void	MakePair(std::unique_ptr<MemoryLink>& a, std::unique_ptr<MemoryLink>& b, uint32_t theDelayMs = 0);
		void		SetNow(uint32_t theNow) { mNow = theNow; }
		void		Send(uint8_t theType, const std::vector<uint8_t>& theData) override;
		bool		Receive(Packet& thePacket) override;
		size_t		mBytesSent = 0;
	private:
		std::shared_ptr<Pipe>	mOut, mIn;
		uint32_t				mDelay = 0, mNow = 0;
	};
}

#endif
