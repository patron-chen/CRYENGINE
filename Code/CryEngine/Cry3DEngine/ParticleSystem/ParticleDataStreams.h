// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleMath.h"

namespace pfx2
{

#ifdef CRY_PFX2_USE_SSE
  #define CRY_PFX2_PARTICLESGROUP_STRIDE 4 // can be 8 for AVX or 64 forGPU
#else
  #define CRY_PFX2_PARTICLESGROUP_STRIDE 1
#endif
#define CRY_PFX2_PARTICLESGROUP_LOWER(id) ((id) & ~((CRY_PFX2_PARTICLESGROUP_STRIDE - 1)))
#define CRY_PFX2_PARTICLESGROUP_UPPER(id) ((id) | ((CRY_PFX2_PARTICLESGROUP_STRIDE - 1)))

//////////////////////////////////////////////////////////////////////////
// TIStream

template<typename T, typename Tv = T>
struct TIStream
{
public:
	explicit TIStream(const T* pStream = nullptr, T defaultVal = T());
	bool IsValid() const { return m_safeMask != 0; }
	T    Load(TParticleId pId) const;
	T    Load(TParticleId pId, T defaultVal) const;
	T    SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Tv   Load(TParticleGroupId pgId) const;
	Tv   Load(TParticleIdv pIdv, T defaultVal) const;
	Tv   SafeLoad(TParticleGroupId pId) const;
#endif
protected:
	const T* __restrict m_pStream;
	Tv                  m_safeSink;
	uint32              m_safeMask;
};

template<typename T, typename Tv = T>
struct TIOStream : public TIStream<T, Tv>
{
private:
	using TIStream<T, Tv>::m_pStream;
public:
	explicit TIOStream(const T* pStream = nullptr, T defaultVal = T()) : TIStream<T, Tv>(pStream, defaultVal) {}
	void Store(TParticleId pId, T value);
#ifdef CRY_PFX2_USE_SSE
	void Store(TParticleGroupId pgId, Tv value);
#endif
};

typedef TIStream<float, floatv>             IFStream;
typedef TIOStream<float, floatv>            IOFStream;
typedef TIStream<UCol, UColv>               IColorStream;
typedef TIOStream<UCol, UColv>              IOColorStream;
typedef TIStream<uint32, uint32v>           IUintStream;
typedef TIOStream<uint32>                   IOUintStream;
typedef TIStream<TParticleId, TParticleIdv> IPidStream;
typedef TIOStream<TParticleId>              IOPidStream;

//////////////////////////////////////////////////////////////////////////

struct IVec3Stream
{
public:
	IVec3Stream(const float* pX, const float* pY, const float* pZ, Vec3 defaultVal = Vec3());
	Vec3  Load(TParticleId pId) const;
	Vec3  SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Vec3v Load(TParticleGroupId pgId) const;
	Vec3v Load(TParticleIdv pIdv, Vec3 defaultVal = Vec3(ZERO)) const;
	Vec3v SafeLoad(TParticleGroupId pgId) const;
#endif
protected:
	const Vec3v             m_safeSink;
	const float* __restrict m_pXStream;
	const float* __restrict m_pYStream;
	const float* __restrict m_pZStream;
	const uint32            m_safeMask;
};

struct IOVec3Stream : public IVec3Stream
{
public:
	IOVec3Stream(float* pX, float* pY, float* pZ, Vec3 defaultVal = Vec3()) : IVec3Stream(pX, pY, pZ, defaultVal) {}
	void  Store(TParticleId pId, Vec3 value);
#ifdef CRY_PFX2_USE_SSE
	void  Store(TParticleGroupId pgId, const Vec3v& value) const;
#endif
};

struct IQuatStream
{
public:
	IQuatStream(const float* pX, const float* pY, const float* pZ, const float* pW, Quat defaultVal = Quat());
	Quat  Load(TParticleId pId) const;
	Quat  SafeLoad(TParticleId pId) const;
#ifdef CRY_PFX2_USE_SSE
	Quatv Load(TParticleGroupId pgId) const;
	Quatv Load(TParticleIdv pIdv, Quat defaultVal = Quat(IDENTITY)) const;
	Quatv SafeLoad(TParticleGroupId pgId) const;
#endif
protected:
	const Quat              m_safeSink;
	const float* __restrict m_pXStream;
	const float* __restrict m_pYStream;
	const float* __restrict m_pZStream;
	const float* __restrict m_pWStream;
	const uint32            m_safeMask;
};

struct IOQuatStream : public IQuatStream
{
public:
	IOQuatStream(float* pX, float* pY, float* pZ, float* pW, Quat defaultVal = Quat()) : IQuatStream(pX, pY, pZ, pW, defaultVal) {}
	void  Store(TParticleId pId, Quat value);
#ifdef CRY_PFX2_USE_SSE
	void  Store(TParticleGroupId pgId, const Quatv& value) const;
#endif
};

//////////////////////////////////////////////////////////////////////////

struct STempVec3Stream
{
public:
	STempVec3Stream(TParticleHeap* pMemHeap, SUpdateRange range);
	void Clear(SUpdateRange context);
	IOVec3Stream GetIOVec3Stream() const { return m_stream; }
private:
	TFloatArray  m_xBuffer;
	TFloatArray  m_yBuffer;
	TFloatArray  m_zBuffer;
	IOVec3Stream m_stream;
};

}

#include "ParticleDataStreamsImpl.h"
