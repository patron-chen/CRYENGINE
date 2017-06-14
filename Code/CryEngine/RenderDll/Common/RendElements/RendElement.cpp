// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RendElement.cpp : common RE functions.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"

CRenderElement CRenderElement::m_RootGlobal(true);
CRenderElement *CRenderElement::m_pRootRelease[4];

//===============================================================

CryCriticalSection m_sREResLock;

int CRenderElement::s_nCounter;

//============================================================================

void CRenderElement::ShutDown()
{
	if (!CRenderer::CV_r_releaseallresourcesonexit)
		return;

	AUTO_LOCK(m_sREResLock); // Not thread safe without this

	CRenderElement* pRE;
	CRenderElement* pRENext;
	for (pRE = CRenderElement::m_RootGlobal.m_NextGlobal; pRE != &CRenderElement::m_RootGlobal; pRE = pRENext)
	{
		pRENext = pRE->m_NextGlobal;
		if (CRenderer::CV_r_printmemoryleaks)
			iLog->Log("Warning: CRenderElement::ShutDown: RenderElement %s was not deleted", pRE->mfTypeString());
		pRE->Release(true);
	}
}

void CRenderElement::Tick()
{
#ifndef STRIP_RENDER_THREAD
	assert(gRenDev->m_pRT->IsMainThread(true));
#endif
	int nFrameID = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nFillThreadID].m_nFrameUpdateID;
	int nFrame = nFrameID - 3;
	CRenderElement& Root = *CRenderElement::m_pRootRelease[nFrame & 3];
	CRenderElement* pRENext = NULL;

	for (CRenderElement* pRE = Root.m_NextGlobal; pRE != &Root; pRE = pRENext)
	{
		pRENext = pRE->m_NextGlobal;
		SAFE_DELETE(pRE);
	}
}

void CRenderElement::Cleanup()
{
	gRenDev->m_pRT->FlushAndWait();

	AUTO_LOCK(m_sREResLock); // Not thread safe without this

	for (int i = 0; i < 4; ++i)
	{
		CRenderElement& Root = *CRenderElement::m_pRootRelease[i];
		CRenderElement* pRENext = NULL;

		for (CRenderElement* pRE = Root.m_NextGlobal; pRE != &Root; pRE = pRENext)
		{
			pRENext = pRE->m_NextGlobal;
			SAFE_DELETE(pRE);
		}
	}
}

void CRenderElement::Release(bool bForce)
{
	m_Flags |= FCEF_DELETED;

#ifdef _DEBUG
	//if (gRenDev && gRenDev->m_pRT)
	//  assert(gRenDev->m_pRT->IsMainThread(true));
#endif
	m_Type = eDATA_Unknown;
	if (bForce)
	{
		//sDeleteRE(this);
		delete this;
		return;
	}
	int nFrame = gRenDev->GetFrameID(false);

	AUTO_LOCK(m_sREResLock);
	CRenderElement& Root = *CRenderElement::m_pRootRelease[nFrame & 3];
	UnlinkGlobal();
	LinkGlobal(&Root);
	//sDeleteRE(this);
}

CRenderElement::CRenderElement(bool bGlobal)
{
	m_Type = eDATA_Unknown;
	if (!m_RootGlobal.m_NextGlobal)
	{
		m_RootGlobal.m_NextGlobal = &m_RootGlobal;
		m_RootGlobal.m_PrevGlobal = &m_RootGlobal;
		for (int i = 0; i < 4; i++)
		{
			m_pRootRelease[i] = new CRenderElement(true);
			m_pRootRelease[i]->m_NextGlobal = m_pRootRelease[i];
			m_pRootRelease[i]->m_PrevGlobal = m_pRootRelease[i];
		}
	}

	m_Flags = 0;
	m_nFrameUpdated = 0xffff;
	m_CustomData = NULL;
	m_nID = CRenderElement::s_nCounter++;
	int i;
	for (i = 0; i < CRenderElement::MAX_CUSTOM_TEX_BINDS_NUM; i++)
		m_CustomTexBind[i] = -1;
}

CRenderElement::CRenderElement()
{
	m_Type = eDATA_Unknown;

	m_Flags = 0;
	m_nFrameUpdated = 0xffff;
	m_CustomData = NULL;
	m_NextGlobal = NULL;
	m_PrevGlobal = NULL;
	m_nID = CRenderElement::s_nCounter++;
	int i;
	for (i = 0; i < CRenderElement::MAX_CUSTOM_TEX_BINDS_NUM; i++)
		m_CustomTexBind[i] = -1;

	//sAddRE(this);

	AUTO_LOCK(m_sREResLock);
  LinkGlobal(&m_RootGlobal);
}
CRenderElement::~CRenderElement()
{
	assert(m_Type == eDATA_Unknown || m_Type == eDATA_Particle);

	//@TODO: Fix later, prevent crash on exit in single executable
	if (this == m_pRootRelease[0] || this == m_pRootRelease[1] || this == m_pRootRelease[2] || this == m_pRootRelease[3] || this == &m_RootGlobal)
		return;

	AUTO_LOCK(m_sREResLock);
	UnlinkGlobal();

	if ((m_Flags & FCEF_ALLOC_CUST_FLOAT_DATA) && m_CustomData)
	{
		delete[] ((float*)m_CustomData);
		m_CustomData = 0;
	}
}

void CRenderElement::mfPrepare(bool bCheckOverflow)
{
}

CRenderChunk*      CRenderElement::mfGetMatInfo()     { return NULL; }
TRenderChunkArray* CRenderElement::mfGetMatInfoList() { return NULL; }
int                CRenderElement::mfGetMatId()       { return -1; }
void               CRenderElement::mfReset()          {}

const char*        CRenderElement::mfTypeString()
{
	switch (m_Type)
	{
	case eDATA_Sky:
		return "Sky";
	case eDATA_Beam:
		return "Beam";
	case eDATA_ClientPoly:
		return "ClientPoly";
	case eDATA_Flare:
		return "Flare";
	case eDATA_Terrain:
		return "Terrain";
	case eDATA_SkyZone:
		return "SkyZone";
	case eDATA_Mesh:
		return "Mesh";
	case eDATA_Imposter:
		return "Imposter";
	case eDATA_LensOptics:
		return "LensOptics";
	case eDATA_FarTreeSprites:
		return "FarTreeSprites";
	case eDATA_OcclusionQuery:
		return "OcclusionQuery";
	case eDATA_Particle:
		return "Particle";
	case eDATA_PostProcess:
		return "PostProcess";
	case eDATA_HDRProcess:
		return "HDRProcess";
	case eDATA_Cloud:
		return "Cloud";
	case eDATA_HDRSky:
		return "HDRSky";
	case eDATA_FogVolume:
		return "FogVolume";
	case eDATA_WaterVolume:
		return "WaterVolume";
	case eDATA_WaterOcean:
		return "WaterOcean";
	case eDATA_VolumeObject:
		return "VolumeObject";
	case eDATA_DeferredShading:
		return "DeferredShading";
	case eDATA_GameEffect:
		return "GameEffect";
	case eDATA_BreakableGlass:
		return "BreakableGlass";
	case eDATA_GeomCache:
		return "GeomCache";
	default:
		{
			CRY_ASSERT(false);
			return "Unknown";
		}
	}
}

CRenderElement* CRenderElement::mfCopyConstruct(void)
{
	CRenderElement* re = new CRenderElement;
	*re = *this;
	return re;
}
void CRenderElement::mfCenter(Vec3& centr, CRenderObject* pObj)
{
	centr(0, 0, 0);
}
void CRenderElement::mfGetPlane(Plane& pl)
{
	pl.n = Vec3(0, 0, 1);
	pl.d = 0;
}

bool  CRenderElement::mfDraw(CShader* ef, SShaderPass* sfm)                                                   { return false; }
void* CRenderElement::mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags) { return NULL; }

//=============================================================================

void* SRendItem::mfGetPointerCommon(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags)
{
	int j;
	switch (ePT)
	{
	case eSrcPointer_Vert:
		*Stride = gRenDev->m_RP.m_StreamStride;
		return gRenDev->m_RP.m_StreamPtr.PtrB;

	case eSrcPointer_Color:
		*Stride = gRenDev->m_RP.m_StreamStride;
		return gRenDev->m_RP.m_StreamPtr.PtrB + gRenDev->m_RP.m_StreamOffsetColor;

	case eSrcPointer_Tex:
	case eSrcPointer_TexLM:
		*Stride = gRenDev->m_RP.m_StreamStride;
		j = ePT - eSrcPointer_Tex;
		return gRenDev->m_RP.m_StreamPtr.PtrB + gRenDev->m_RP.m_StreamOffsetTC + j * 16;
	}
	return NULL;
}
