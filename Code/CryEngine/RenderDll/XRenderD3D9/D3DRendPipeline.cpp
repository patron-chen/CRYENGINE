// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryMovie/IMovieSystem.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryGame/IGameFramework.h>

#include "D3DPostProcess.h"
#include "D3DStereo.h"
#include "D3DHWShader.h"
#include "D3DTiledShading.h"
#include "../Common/Shaders/RemoteCompiler.h"
#include "../Common/ReverseDepth.h"
#ifdef ENABLE_BENCHMARK_SENSOR
#include <IBenchmarkFramework.h>
#include <IBenchmarkRendererSensorManager.h>
#include "BenchmarkCustom/BenchmarkRendererSensor.h"
#endif
#include "../Common/ComputeSkinningStorage.h"
#if defined(FEATURE_SVO_GI)
#include "D3D_SVO.h"
#endif

#include "Gpu/Particles/GpuParticleManager.h"
#include "GraphicsPipeline/Common/GraphicsPipelineStage.h"
#include "GraphicsPipeline/Common/SceneRenderPass.h"
#include "GraphicsPipeline/ComputeSkinning.h"
#include "Common/RenderView.h"
#include "CompiledRenderObject.h"

#pragma warning(disable: 4244)

extern SHWOccZBuffer HWZBuffer;

//============================================================================================
// Shaders rendering
//============================================================================================

//============================================================================================
// Init Shaders rendering
void SRenderPipeline::InitWaveTables()
{
	int i;

	//Init wave Tables
	for (i = 0; i < sSinTableCount; i++)
	{
		float f = (float)i;

		m_tSinTable[i] = sin_tpl(f * (360.0f / (float)sSinTableCount) * (float)M_PI / 180.0f);
	}
}

// build vertex declarations on demand (for programmable pipeline)
void SRenderPipeline::OnDemandVertexDeclaration(SOnDemandD3DVertexDeclaration& out,
  const int nStreamMask, const int vertexformat, const bool bMorph, const bool bInstanced)
{
	//	iLog->Log("OnDemandVertexDeclaration %d %d %d (DEBUG test - shouldn't log too often)",nStreamMask,vertexformat,bMorph?1:0);

	if (!m_D3DVertexDeclaration[vertexformat].m_Declaration.Num())
		return;

	uint32 j;

	if (bInstanced)
	{
		// Create instanced vertex declaration
		for (j = 0; j < m_D3DVertexDeclaration[vertexformat].m_Declaration.Num(); j++)
		{
			D3D11_INPUT_ELEMENT_DESC elem = m_D3DVertexDeclaration[vertexformat].m_Declaration[j];
			elem.InputSlotClass       = D3D11_INPUT_PER_INSTANCE_DATA;
			elem.InstanceDataStepRate = 1;
			out.m_Declaration.AddElem(elem);
		}
	}
	else
	{
		for (j = 0; j < m_D3DVertexDeclaration[vertexformat].m_Declaration.Num(); j++)
		{
			out.m_Declaration.AddElem(m_D3DVertexDeclaration[vertexformat].m_Declaration[j]);
		}
	}

	for (j = 1; j < VSF_NUM; j++)
	{
		if (!(nStreamMask & (1 << (j - 1))))
			continue;
		int n;
		for (n = 0; n < m_D3DStreamProperties[j].m_nNumElements; n++)
		{
			out.m_Declaration.AddElem(m_D3DStreamProperties[j].m_pElements[n]);
		}
	}

	if (bMorph)
	{
		uint32 dwNumWithoutMorph = out.m_Declaration.Num();

		for (j = 0; j < dwNumWithoutMorph; j++)
		{
			D3D11_INPUT_ELEMENT_DESC El = out.m_Declaration[j];
			El.InputSlot     += VSF_MORPHBUDDY;
			El.SemanticIndex += 8;
			out.m_Declaration.AddElem(El);
		}
		D3D11_INPUT_ELEMENT_DESC El = {"BLENDWEIGHT", 1, DXGI_FORMAT_R32G32_FLOAT, VSF_MORPHBUDDY_WEIGHTS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};         // BlendWeight
		out.m_Declaration.AddElem(El);
	}
}

EVertexFormat SRenderPipeline::AddD3DVertexDeclaration(size_t numDescs, const D3D11_INPUT_ELEMENT_DESC* inputLayout)
{
	EVertexFormat newVF = MaxD3DVertexDeclaration();

	m_D3DVertexDeclaration.GrowReset(1);
	{
		TArray<D3D11_INPUT_ELEMENT_DESC>& decl = m_D3DVertexDeclaration[newVF].m_Declaration;

		for (int n = 0; n < numDescs; ++n)
			decl.AddElem(inputLayout[n]);
	}

	for (int n = 0; n < 1 << VSF_NUM; ++n)
	{
		m_D3DVertexDeclarationCache[n][0].GrowReset(1);
		m_D3DVertexDeclarationCache[n][1].GrowReset(1);
	}

	return newVF;
}

void SRenderPipeline::InitD3DVertexDeclarations()
{
	SBufInfoTable* pOffs;
	int nFormat = 0;

	//========================================================================================
	// base stream declarations (stream 0)
	D3D11_INPUT_ELEMENT_DESC elemPosHalf = {"POSITION", 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
	D3D11_INPUT_ELEMENT_DESC elemTCHalf  = {"TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};

	D3D11_INPUT_ELEMENT_DESC elemPos      = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
	D3D11_INPUT_ELEMENT_DESC elemPos2     = {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
	D3D11_INPUT_ELEMENT_DESC elemPosTR    = {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};           // position
	D3D11_INPUT_ELEMENT_DESC elemPos2Half = {"POSITION", 0, DXGI_FORMAT_R16G16_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};

	D3D11_INPUT_ELEMENT_DESC elemNormalB = {"NORMAL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
	D3D11_INPUT_ELEMENT_DESC elemTan     = {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};               // axis/size
	D3D11_INPUT_ELEMENT_DESC elemBitan   = {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};               // axis/size
	D3D11_INPUT_ELEMENT_DESC elemColor   = {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};                   // diffuse
	D3D11_INPUT_ELEMENT_DESC elemColorF  = {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};               // general color
	D3D11_INPUT_ELEMENT_DESC elemTC0     = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};                  // texture
	D3D11_INPUT_ELEMENT_DESC elemTC1     = {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};                  // texture
	D3D11_INPUT_ELEMENT_DESC elemTC1_3   = {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};               // texture
	D3D11_INPUT_ELEMENT_DESC elemTC0_4   = {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};            // texture
	D3D11_INPUT_ELEMENT_DESC elemTC0_1   = {"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};                     // texture

	m_D3DVertexDeclaration.Reserve(eVF_PreAllocated);
	for (nFormat = 0; nFormat < eVF_PreAllocated; ++nFormat)
	{
		TArray<D3D11_INPUT_ELEMENT_DESC>& decl = m_D3DVertexDeclaration[nFormat].m_Declaration;
		decl.Free();

		pOffs = &CRenderMesh::m_cBufInfoTable[nFormat];
		// Position
		if (nFormat == eVF_TP3F_C4B_T2F || nFormat == eVF_TP3F_T2F_T3F || nFormat == eVF_P3F)
			decl.AddElem(elemPosTR);
		else if (nFormat == eVF_P3S_C4B_T2S || nFormat == eVF_P3S_N4B_C4B_T2S)
			decl.AddElem(elemPosHalf);
		else if (nFormat == eVF_P2S_N4B_C4B_T1F)
			decl.AddElem(elemPos2Half);
		else if (nFormat != eVF_C4B_T2S)
			decl.AddElem(elemPos);

		// Normal
		if (pOffs->OffsNorm >= 0)
		{
			elemNormalB.AlignedByteOffset = pOffs->OffsNorm;
			decl.AddElem(elemNormalB);
		}

		if (nFormat == eVF_P3S_N4B_C4B_T2S)
		{
			elemNormalB.AlignedByteOffset = sizeof(Vec3f16);
			decl.AddElem(elemNormalB);
		}
#ifdef PARTICLE_MOTION_BLUR
		if (nFormat == eVF_P3F_C4B_T4B_N3F2)
		{
			elemTC0_4.AlignedByteOffset = (int)offsetof(SVF_P3F_C4B_T4B_N3F2, prevXaxis);
			elemTC0_4.SemanticIndex     = 0;
			decl.AddElem(elemTC0_4);
		}
#endif
		if (pOffs->OffsColor >= 0 || nFormat == eVF_C4B_T2S)
		{
			elemColor.AlignedByteOffset = pOffs->OffsColor;
			elemColor.SemanticIndex     = 0;
			decl.AddElem(elemColor);
		}
		if (nFormat == eVF_P3F_C4B_T4B_N3F2)
		{
#ifdef PARTICLE_MOTION_BLUR
			elemTC1_3.AlignedByteOffset = (int)offsetof(SVF_P3F_C4B_T4B_N3F2, prevPos);
			elemTC1_3.SemanticIndex     = 1;
			decl.AddElem(elemTC1_3);
#endif
			elemColor.AlignedByteOffset = (int)offsetof(SVF_P3F_C4B_T4B_N3F2, st);
			elemColor.SemanticIndex     = 1;
			decl.AddElem(elemColor);

			elemTan.AlignedByteOffset = (int)offsetof(SVF_P3F_C4B_T4B_N3F2, xaxis);
			decl.AddElem(elemTan);

			elemBitan.AlignedByteOffset = (int)offsetof(SVF_P3F_C4B_T4B_N3F2, yaxis);
			decl.AddElem(elemBitan);
		}

		if (nFormat == eVF_P2F_T4F_C4F)
		{
			decl.ClearArr();
			decl.AddElem(elemPos2);

			elemTC0_4.AlignedByteOffset = (int)offsetof(SVF_P2F_T4F_C4F, st);
			elemTC0_4.SemanticIndex     = 0;
			decl.AddElem(elemTC0_4);

			elemColorF.AlignedByteOffset = (int)offsetof(SVF_P2F_T4F_C4F, color);
			elemColorF.SemanticIndex     = 0;
			decl.AddElem(elemColorF);
		}

		if (pOffs->OffsTC >= 0)
		{
			elemTC0.AlignedByteOffset = pOffs->OffsTC;
			elemTC0.SemanticIndex     = 0;

			if (nFormat == eVF_P3S_C4B_T2S || nFormat == eVF_P3S_N4B_C4B_T2S || nFormat == eVF_C4B_T2S || nFormat == eVF_P3F_C4B_T2S)
			{
				elemTCHalf.AlignedByteOffset = pOffs->OffsTC;
				elemTCHalf.SemanticIndex     = 0;
				decl.AddElem(elemTCHalf);
			}
			else if (nFormat == eVF_P3F_T3F)
			{
				elemTC1_3.AlignedByteOffset = pOffs->OffsTC;
				elemTC1_3.SemanticIndex     = 0;
				decl.AddElem(elemTC1_3);
			}
			else if (nFormat == eVF_P2S_N4B_C4B_T1F)
			{
				elemTC0_1.AlignedByteOffset = pOffs->OffsTC;
				elemTC0_1.SemanticIndex     = 0;
				decl.AddElem(elemTC0_1);
			}
			else
			{
				decl.AddElem(elemTC0);
			}
			if (nFormat == eVF_TP3F_T2F_T3F || nFormat == eVF_P3F_T2F_T3F)
			{
				elemTC1_3.AlignedByteOffset = pOffs->OffsTC + 8;
				elemTC1_3.SemanticIndex     = 1;
				decl.AddElem(elemTC1_3);
			}
		}
		decl.Shrink();
	}

	for (int n = 0; n < 1 << VSF_NUM; ++n)
	{
		m_D3DVertexDeclarationCache[n][0].Reserve(eVF_PreAllocated);
		m_D3DVertexDeclarationCache[n][1].Reserve(eVF_PreAllocated);
	}

	//=============================================================================
	// Additional streams declarations:

	// Tangents stream
	static D3D11_INPUT_ELEMENT_DESC VElemTangents[] =
	{
#ifdef TANG_FLOATS
		{"TANGENT",  0,  DXGI_FORMAT_R32G32B32A32_FLOAT,  VSF_TANGENTS, 0,  D3D11_INPUT_PER_VERTEX_DATA,  0 },  // Binormal
		{"BINORMAL", 0,  DXGI_FORMAT_R32G32B32A32_FLOAT,  VSF_TANGENTS, 16, D3D11_INPUT_PER_VERTEX_DATA,  0 },        // Tangent
#else
		{"TANGENT",  0,  DXGI_FORMAT_R16G16B16A16_SNORM,  VSF_TANGENTS, 0,  D3D11_INPUT_PER_VERTEX_DATA,  0 },  // Binormal
		{"BINORMAL", 0,  DXGI_FORMAT_R16G16B16A16_SNORM,  VSF_TANGENTS, 8,  D3D11_INPUT_PER_VERTEX_DATA,  0 },  // Tangent
#endif
	};
	// Tangents stream
	static D3D11_INPUT_ELEMENT_DESC VElemQTangents[] =
	{
#ifdef TANG_FLOATS
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, VSF_QTANGENTS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},             // Binormal
#else
		{"TANGENT", 0, DXGI_FORMAT_R16G16B16A16_SNORM, VSF_QTANGENTS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},             // Binormal
#endif
	};

	//HW Skin stream
	static D3D11_INPUT_ELEMENT_DESC VElemHWSkin[] =
	{
		{"BLENDWEIGHT",  0,  DXGI_FORMAT_R8G8B8A8_UNORM,     VSF_HWSKIN_INFO,    0,    D3D11_INPUT_PER_VERTEX_DATA,   0}, // BlendWeight
		{"BLENDINDICES", 0,  DXGI_FORMAT_R16G16B16A16_SINT,  VSF_HWSKIN_INFO,    4,    D3D11_INPUT_PER_VERTEX_DATA,   0}, // BlendIndices
	};

#if ENABLE_NORMALSTREAM_SUPPORT
	static D3D11_INPUT_ELEMENT_DESC VElemNormals[] =
	{
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, VSF_NORMALS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
#endif

	static D3D11_INPUT_ELEMENT_DESC VElemVelocity[] =
	{
		{"POSITION", 3, DXGI_FORMAT_R32G32B32_FLOAT, VSF_VERTEX_VELOCITY, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},         // Velocity
	};

	// stream 1 (Tangent basis vectors)
	// stream 2 (QTangents info)
	// stream 3 (HW skin info)
	// stream 4 (Velocity)
	// stream 5 (Normals)
	m_D3DStreamProperties[VSF_GENERAL].m_pElements            = NULL;
	m_D3DStreamProperties[VSF_GENERAL].m_nNumElements         = 0;
	m_D3DStreamProperties[VSF_TANGENTS].m_pElements           = VElemTangents;
	m_D3DStreamProperties[VSF_TANGENTS].m_nNumElements        = sizeof(VElemTangents) / sizeof(D3D11_INPUT_ELEMENT_DESC);
	m_D3DStreamProperties[VSF_QTANGENTS].m_pElements          = VElemQTangents;
	m_D3DStreamProperties[VSF_QTANGENTS].m_nNumElements       = sizeof(VElemQTangents) / sizeof(D3D11_INPUT_ELEMENT_DESC);
	m_D3DStreamProperties[VSF_HWSKIN_INFO].m_pElements        = VElemHWSkin;
	m_D3DStreamProperties[VSF_HWSKIN_INFO].m_nNumElements     = sizeof(VElemHWSkin) / sizeof(D3D11_INPUT_ELEMENT_DESC);
	m_D3DStreamProperties[VSF_VERTEX_VELOCITY].m_pElements    = VElemVelocity;
	m_D3DStreamProperties[VSF_VERTEX_VELOCITY].m_nNumElements = sizeof(VElemVelocity) / sizeof(D3D11_INPUT_ELEMENT_DESC);
#if ENABLE_NORMALSTREAM_SUPPORT
	m_D3DStreamProperties[VSF_NORMALS].m_pElements    = VElemNormals;
	m_D3DStreamProperties[VSF_NORMALS].m_nNumElements = sizeof(VElemNormals) / sizeof(D3D11_INPUT_ELEMENT_DESC);
#endif
}

inline static void* sAlign0x20(byte* vrts)
{
	return (void*)(((INT_PTR)vrts + 0x1f) & ~0x1f);
}

// Init shaders pipeline
void CD3D9Renderer::EF_Init()
{
	bool nv = 0;

	if (CV_r_logTexStreaming && !m_LogFileStr)
	{
		m_LogFileStr = fxopen ("Direct3DLogStreaming.txt", "w");
		if (m_LogFileStr)
		{
			iLog->Log("Direct3D texture streaming log file '%s' opened", "Direct3DLogStreaming.txt");
			char time[128];
			char date[128];

			_strtime(time);
			_strdate(date);

			fprintf(m_LogFileStr, "\n==========================================\n");
			fprintf(m_LogFileStr, "Direct3D Textures streaming Log file opened: %s (%s)\n", date, time);
			fprintf(m_LogFileStr, "==========================================\n");
		}
	}

	m_RP.m_MaxVerts = 16384;
	m_RP.m_MaxTris  = 16384 * 3;

	iLog->Log("Allocate render buffer for particles (%d verts, %d tris)...", m_RP.m_MaxVerts, m_RP.m_MaxTris);

	int n = 0;

	int nSizeV = sizeof(SVF_P3F_C4B_T4B_N3F2);
	for (int i = 0; i < eVF_MaxRenderMesh; i++)
		nSizeV = max(nSizeV, CRenderMesh::m_cSizeVF[i]);

	n += nSizeV * m_RP.m_MaxVerts + 32;

	n += sizeof(SPipTangents) * m_RP.m_MaxVerts + 32;

	//m_RP.mRendIndices;
	n += sizeof(uint16) * 3 * m_RP.m_MaxTris + 32;

	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Renderer Particles Buffer");

		byte* buf = new byte[n];
		m_RP.m_SizeSysArray = n;
		m_RP.m_SysArray     = buf;
		if (!buf)
			iConsole->Exit("Can't allocate buffers for RB");

		memset(buf, 0, n);

		m_RP.m_StreamPtr.Ptr = sAlign0x20(buf);
		buf += sizeof(SVF_P3F_C4B_T4B_N3F2) * m_RP.m_MaxVerts + 32;

		m_RP.m_StreamPtrTang.Ptr = sAlign0x20(buf);
		buf += sizeof(SPipTangents) * m_RP.m_MaxVerts + 32;

		m_RP.m_RendIndices    = (uint16*)sAlign0x20(buf);
		m_RP.m_SysRendIndices = m_RP.m_RendIndices;
		buf += sizeof(uint16) * 3 * m_RP.m_MaxTris + 32;
	}

	EF_Restore();

	m_RP.InitWaveTables();
	m_RP.InitD3DVertexDeclarations();
	CHWShader_D3D::mfInit();

	m_CurVertBufferSize = 0;
	m_CurIndexBufferSize = 0;

	//==================================================
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_D3D, 0, "Renderer TempObjects");

		if (m_RP.m_ObjectsPool)
		{
			CryModuleMemalignFree(m_RP.m_ObjectsPool);
			m_RP.m_ObjectsPool = NULL;
		}
		m_RP.m_nNumObjectsInPool = TEMP_REND_OBJECTS_POOL;

		// we use a plain allocation and placement new here to guarantee the alignment, when using array new, the compiler can store it's size and break the alignment
		m_RP.m_ObjectsPool = (CRenderObject*)CryModuleMemalign(sizeof(CRenderObject) * (m_RP.m_nNumObjectsInPool * RT_COMMAND_BUF_COUNT), 16);
		for (int j = 0; j < (int)(m_RP.m_nNumObjectsInPool * RT_COMMAND_BUF_COUNT); j++)
		{
			CRenderObject* pRendObj = new(&m_RP.m_ObjectsPool[j])CRenderObject();
			pRendObj->Init();
		}

		CRenderObject* arrPrefill[TEMP_REND_OBJECTS_POOL] = {};
		for (int j = 0; j < RT_COMMAND_BUF_COUNT; j++)
		{
			for (int k = 0; k < m_RP.m_nNumObjectsInPool; ++k)
				arrPrefill[k] = &m_RP.m_ObjectsPool[j * m_RP.m_nNumObjectsInPool + k];

			m_RP.m_TempObjects[j].PrefillContainer(arrPrefill, m_RP.m_nNumObjectsInPool);
			m_RP.m_TempObjects[j].resize(0);
		}
	}

	// Init identity RenderObject
	SAFE_DELETE(m_RP.m_pIdendityRenderObject);
	m_RP.m_pIdendityRenderObject = new CRenderObject();
	m_RP.m_pIdendityRenderObject->Init();
	m_RP.m_pIdendityRenderObject->m_II.m_Matrix.SetIdentity();
	m_RP.m_pIdendityRenderObject->m_ObjFlags |= FOB_RENDERER_IDENDITY_OBJECT;

	// Init compiled objects pool
	{
		m_RP.m_renderObjectsPools.reset(new CRenderObjectsPools);
		// Initialize fast access global pointer
		CCompiledRenderObject::SetStaticPools(m_RP.m_renderObjectsPools.get());
		CPermanentRenderObject::SetStaticPools(m_RP.m_renderObjectsPools.get());
	}

	// create hdr element
	m_RP.m_pREHDR = (CREHDRProcess*)EF_CreateRE(eDATA_HDRProcess);
	// create deferred shading element
	m_RP.m_pREDeferredShading = (CREDeferredShading*)EF_CreateRE(eDATA_DeferredShading);

	// Create post process render element
	m_RP.m_pREPostProcess = (CREPostProcess*)EF_CreateRE(eDATA_PostProcess);

	// Initialize posteffects manager
	if (!m_pPostProcessMgr)
	{
		m_pPostProcessMgr = new CPostEffectsMgr;
		m_pPostProcessMgr->Init();
	}

	if (!m_pWaterSimMgr)
		m_pWaterSimMgr = new CWater;

	//SDynTexture::CreateShadowPool();

	m_RP.m_fLastWaterFOVUpdate    = 0;
	m_RP.m_LastWaterViewdirUpdate = Vec3(0, 0, 0);
	m_RP.m_LastWaterUpdirUpdate   = Vec3(0, 0, 0);
	m_RP.m_LastWaterPosUpdate     = Vec3(0, 0, 0);
	m_RP.m_fLastWaterUpdate       = 0;
	m_RP.m_nLastWaterFrameID      = 0;
	m_RP.m_nCommitFlags           = FC_ALL;

	m_RP.m_nSPIUpdateFrameID = -1;

	m_nPointState                 = CTexture::GetTexState(STexState(FILTER_POINT, true));
	m_nMaterialAnisoHighSampler   = CTexture::GetTexState(STexState(FILTER_ANISO16X, false));
	m_nMaterialAnisoLowSampler    = CTexture::GetTexState(STexState(FILTER_ANISO4X, false));
	m_nMaterialAnisoSamplerBorder = CTexture::GetTexState(STexState(FILTER_ANISO16X, TADDR_BORDER, TADDR_BORDER, TADDR_BORDER, 0x0));
	m_nPointClampSampler          = CTexture::GetTexState(STexState(FILTER_POINT, TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP, 0x0));
	m_nPointWrapSampler           = CTexture::GetTexState(STexState(FILTER_POINT, TADDR_WRAP, TADDR_WRAP, TADDR_WRAP, 0x0));
	STexState texState(FILTER_LINEAR, TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP, 0x0);
	texState.SetComparisonFilter(true);
	m_nLinearClampComparisonSampler = CTexture::GetTexState(texState);
	m_nBilinearWrapSampler = CTexture::GetTexState(STexState(FILTER_BILINEAR, TADDR_WRAP, TADDR_WRAP, TADDR_WRAP, 0x0));
	m_nBilinearClampSampler = CTexture::GetTexState(STexState(FILTER_BILINEAR, TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP, 0x0));
	m_nBilinearBorderSampler = CTexture::GetTexState(STexState(FILTER_BILINEAR, TADDR_BORDER, TADDR_BORDER, TADDR_BORDER, 0x0));
	m_nTrilinearWrapSampler = CTexture::GetTexState(STexState(FILTER_TRILINEAR, TADDR_WRAP, TADDR_WRAP, TADDR_WRAP, 0x0));
	m_nTrilinearClampSampler = CTexture::GetTexState(STexState(FILTER_TRILINEAR, TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP, 0x0));
	m_nTrilinearBorderSampler = CTexture::GetTexState(STexState(FILTER_TRILINEAR, TADDR_BORDER, TADDR_BORDER, TADDR_BORDER, 0x0));

	CDeferredShading::CreateDeferredShading();

	if (m_pStereoRenderer)
	{
		m_pStereoRenderer->CreateResources();
		m_pStereoRenderer->Update();
	}

	assert(m_pBackBuffer == m_pBackBuffers[CD3D9Renderer::GetCurrentBackBufferIndex(m_pSwapChain)]);
	if (m_pBackBuffer != m_pBackBuffers[CD3D9Renderer::GetCurrentBackBufferIndex(m_pSwapChain)])
		abort();

	D3DSurface* pBackbufferView = m_pBackBuffer;
	GetDeviceContext().OMSetRenderTargets(1, &pBackbufferView, m_DepthBufferNative.pSurface);

	ResetToDefault();
#ifdef ENABLE_BENCHMARK_SENSOR
	if (!m_benchmarkRendererSensor)
	{
		m_benchmarkRendererSensor = new BenchmarkRendererSensor(this);
	}
#endif
}

// Invalidate shaders pipeline
void CD3D9Renderer::FX_Invalidate()
{
	m_RP.m_lightVolumeBuffer.Release();
	m_RP.m_particleBuffer.Release();
}

void CD3D9Renderer::FX_UnbindBuffer(D3DBuffer* buffer)
{
	IF (!buffer, 0)
	return;

	for (int i = 0; i < MAX_STREAMS; i++)
	{
		IF (m_RP.m_VertexStreams[i].pStream == buffer, 0)
		{
			D3DBuffer* pNullBuffer  = NULL;
			uint32 zeroStrideOffset = 0;
			m_DevMan.BindVB(i, 1, &pNullBuffer, &zeroStrideOffset, &zeroStrideOffset);
			m_RP.m_VertexStreams[i].pStream = NULL;
		}
	}
	IF (m_RP.m_pIndexStream == buffer, 0)
	{
		m_DevMan.BindIB(NULL, 0, DXGI_FORMAT_R16_UINT);
		m_RP.m_pIndexStream = NULL;
	}

	// commit state changes a second time to really unbind right now, not during the next DrawXXX or Commit
	m_DevMan.CommitDeviceStates();
}

// Restore shaders pipeline
void CD3D9Renderer::EF_Restore()
{
	if (!m_RP.m_MaxTris)
		return;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_D3D, 0, "D3D Restore");

	FX_Invalidate();

	SyncComputeVerticesJobs();

	// preallocate light volume buffer
	m_RP.m_lightVolumeBuffer.Create();

	// preallocate video memory buffer for particles when using the job system
	m_RP.m_particleBuffer.Create(CV_r_ParticleVerticePoolSize);
}

// Shutdown shaders pipeline
void CD3D9Renderer::FX_PipelineShutdown(bool bFastShutdown)
{
	FX_Invalidate();

	SAFE_DELETE_ARRAY(m_RP.m_SysArray);
	const uint32 n = m_RP.MaxD3DVertexDeclaration();

	for (uint32 j = 0; j < n; j++)
	{
		m_RP.m_D3DVertexDeclaration[j].m_Declaration.Free();
	}

	for (uint32 i = 0; i < (1U << VSF_NUM); i++)
	{
		for (uint32 j = 0; j < n; j++)
		{
			SAFE_RELEASE(m_RP.m_D3DVertexDeclarationCache[i][0][j].m_pDeclaration);
			SAFE_RELEASE(m_RP.m_D3DVertexDeclarationCache[i][1][j].m_pDeclaration);
		}

		m_RP.m_D3DVertexDeclarationCache[i][0].Free();
		m_RP.m_D3DVertexDeclarationCache[i][1].Free();
	}

	m_RP.m_D3DVertexDeclaration.Free();

	SAFE_RELEASE(m_RP.m_pREHDR);
	SAFE_RELEASE(m_RP.m_pREDeferredShading);
	SAFE_RELEASE(m_RP.m_pREPostProcess);
	SAFE_DELETE(m_pPostProcessMgr);
	SAFE_DELETE(m_pWaterSimMgr);

	//if (m_pStereoRenderer)
	//	m_pStereoRenderer->ReleaseResources();

#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		m_pRenderAuxGeomD3D->ReleaseShader();
#endif

	if (!bFastShutdown)
		CHWShader_D3D::ShutDown();

	m_RP.m_pCurTechnique = 0;

	CryModuleMemalignFree(m_RP.m_ObjectsPool);
	m_RP.m_ObjectsPool = NULL;
	for (int k = 0; k < RT_COMMAND_BUF_COUNT; ++k)
		m_RP.m_TempObjects[k].clear();

	m_RP.m_renderObjectsPools.reset();

	// Reset states
	if (GetDeviceContext().IsValid())
	{
		m_DevMan.SetDepthStencilState(nullptr, 0);
		m_DevMan.SetBlendState(nullptr, nullptr, 0);
		m_DevMan.SetRasterState(nullptr);

		m_DevMan.CommitDeviceStates();
	}

	for (uint32 i = 0; i < m_StatesDP.Num(); ++i)
		SAFE_RELEASE(m_StatesDP[i].pState);
	for (uint32 i = 0; i < m_StatesRS.Num(); ++i)
		SAFE_RELEASE(m_StatesRS[i].pState);
	for (uint32 i = 0; i < m_StatesBL.Num(); ++i)
		SAFE_RELEASE(m_StatesBL[i].pState);
	m_StatesBL.Free();
	m_StatesRS.Free();
	m_StatesDP.Free();
	m_nCurStateRS = ~0U;
	m_nCurStateDP = ~0U;
	m_nCurStateBL = ~0U;

	CDeferredShading::DestroyDeferredShading();

	for (unsigned int a = 0; a < m_OcclQueries.size(); a++)
		m_OcclQueries[a].Release();

#ifdef ENABLE_BENCHMARK_SENSOR
	SAFE_DELETE(m_benchmarkRendererSensor);
#endif
}

void CD3D9Renderer::RT_GraphicsPipelineShutdown()
{
	SAFE_DELETE(CTexture::s_pMipperWaterVolumeDDN);
	SAFE_DELETE(CTexture::s_pMipperWaterVolumeRefl[0]);
	SAFE_DELETE(CTexture::s_pMipperWaterVolumeRefl[1]);
	SAFE_DELETE(CTexture::s_pMipperWaterRipplesDDN);
	
	SAFE_DELETE(m_pGraphicsPipeline);
	CCryDeviceWrapper::GetObjectFactory().ReleaseResources();
}

void CD3D9Renderer::FX_ResetPipe()
{
	int i;

	FX_SetState(GS_NODEPTHTEST);
	D3DSetCull(eCULL_None);
	m_RP.m_FlagsStreams_Decl   = 0;
	m_RP.m_FlagsStreams_Stream = 0;
	m_RP.m_FlagsPerFlush       = 0;
	m_RP.m_FlagsShader_RT      = 0;
	m_RP.m_FlagsShader_MD      = 0;
	m_RP.m_FlagsShader_MDV     = 0;
	m_RP.m_FlagsShader_PipelineState = 0;
	m_RP.m_FlagsShader_LT      = 0;
	m_RP.m_nCommitFlags        = FC_ALL;
	m_RP.m_PersFlags2         |= RBPF2_COMMIT_PF | RBPF2_COMMIT_CM;

	m_RP.m_nZOcclusionProcess = 0;
	m_RP.m_nZOcclusionReady   = 1;

	m_RP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;

	HRESULT h = FX_SetIStream(NULL, 0, Index16);

	EF_Scissor(false, 0, 0, 0, 0);
	m_RP.m_pShader       = NULL;
	m_RP.m_pCurTechnique = NULL;
	for (i = 1; i < VSF_NUM; i++)
	{
		if (m_RP.m_PersFlags1 & (RBPF1_USESTREAM << i))
		{
			m_RP.m_PersFlags1 &= ~(RBPF1_USESTREAM << i);
			h                  = FX_SetVStream(i, NULL, 0, 0);
		}
	}

	CHWShader_D3D::mfSetGlobalParams();
}

void DrawFullScreenQuad(float fLeftU, float fTopV, float fRightU, float fBottomV);

//==========================================================================
// Calculate current scene node matrices
void CD3D9Renderer::EF_SetCameraInfo()
{
	m_pRT->RC_SetCamera();
}

void CD3D9Renderer::RT_SetCameraInfo()
{
	GetModelViewMatrix(&m_ViewMatrix(0, 0));
	m_CameraMatrix = m_ViewMatrix;

	GetProjectionMatrix(&m_ProjMatrix(0, 0));
	GetCameraZeroMatrix(&m_CameraZeroMatrix(0, 0));

	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	if (pShaderThreadInfo->m_PersFlags & RBPF_OBLIQUE_FRUSTUM_CLIPPING)
	{
		Matrix44A mObliqueProjMatrix;
		mObliqueProjMatrix.SetIdentity();

		mObliqueProjMatrix.m02 = pShaderThreadInfo->m_pObliqueClipPlane.n[0];
		mObliqueProjMatrix.m12 = pShaderThreadInfo->m_pObliqueClipPlane.n[1];
		mObliqueProjMatrix.m22 = pShaderThreadInfo->m_pObliqueClipPlane.n[2];
		mObliqueProjMatrix.m32 = pShaderThreadInfo->m_pObliqueClipPlane.d;

		m_ProjMatrix = m_ProjMatrix * mObliqueProjMatrix;
	}

	bool bApplySubpixelShift = !(m_RP.m_PersFlags2 & RBPF2_NOPOSTAA);
	bApplySubpixelShift &= !(pShaderThreadInfo->m_PersFlags & RBPF_DRAWTOTEXTURE);

	if (bApplySubpixelShift)
	{
		m_ProjMatrix.m20 += m_vProjMatrixSubPixoffset.x;
		m_ProjMatrix.m21 += m_vProjMatrixSubPixoffset.y;
	}

	m_CameraProjMatrix     = m_CameraMatrix * m_ProjMatrix;
	m_CameraProjZeroMatrix = m_CameraZeroMatrix * m_ProjMatrix;

	// specialized matrix inversion for enhanced precision
	Matrix44_tpl<f64> mProjInv;
	if (mathMatrixPerspectiveFovInverse(&mProjInv, &m_ProjMatrix))
	{
		Matrix44_tpl<f64> mViewInv;
		mathMatrixLookAtInverse(&mViewInv, &m_CameraMatrix);
		m_InvCameraProjMatrix = mProjInv * mViewInv;
	}
	else
	{
		m_InvCameraProjMatrix = m_CameraProjMatrix.GetInverted();
	}

	if (m_RP.m_ObjFlags & FOB_NEAREST)
		m_CameraMatrixNearest = m_CameraMatrix;

	pShaderThreadInfo->m_PersFlags |= RBPF_FP_DIRTY;
	m_RP.m_ObjFlags                 = FOB_TRANS_MASK;

	m_NewViewport.fMinZ = pShaderThreadInfo->m_cam.GetZRangeMin();
	m_NewViewport.fMaxZ = pShaderThreadInfo->m_cam.GetZRangeMax();
	m_bViewportDirty    = true;

	CHWShader_D3D::mfSetCameraParams();
}

// Set object transform for fixed pipeline shader
void CD3D9Renderer::FX_SetObjectTransform(CRenderObject* obj, CShader* pSH, uint64 nObjFlags)
{
	assert(m_pRT->IsRenderThread());

	if (nObjFlags & FOB_TRANS_MASK)
		m_ViewMatrix = (Matrix44A(obj->m_II.m_Matrix).GetTransposed() * m_CameraMatrix);
	else
		m_ViewMatrix = m_CameraMatrix;

	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);
	pShaderThreadInfo->m_matView->LoadMatrix(&m_ViewMatrix);
	pShaderThreadInfo->m_PersFlags |= RBPF_FP_MATRIXDIRTY;
}

//==============================================================================
// Shader Pipeline
//=======================================================================

void CD3D9Renderer::EF_SetFogColor(const ColorF& Color)
{
	const int nThreadID = m_pRT->GetThreadList();

	m_uLastBlendFlagsPassGroup = PackBlendModeAndPassGroup();

	m_RP.m_TI[nThreadID].m_FS.m_CurColor = Color;
}

// Set current texture color op modes (used in fixed pipeline shaders)
void CD3D9Renderer::SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa)
{
	EF_SetColorOp(eCo, eAo, eCa, eAa);
}

void CD3D9Renderer::EF_SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa)
{
	const int nThreadID = m_pRT->GetThreadList();
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[nThreadID]);

	if (eCo != 255 && pShaderThreadInfo->m_eCurColorOp != eCo)
	{
		pShaderThreadInfo->m_eCurColorOp = eCo;
		pShaderThreadInfo->m_PersFlags  |= RBPF_FP_DIRTY;
	}
	if (eAo != 255 && pShaderThreadInfo->m_eCurAlphaOp != eAo)
	{
		pShaderThreadInfo->m_eCurAlphaOp = eAo;
		pShaderThreadInfo->m_PersFlags  |= RBPF_FP_DIRTY;
	}
	if (eCa != 255 && pShaderThreadInfo->m_eCurColorArg != eCa)
	{
		pShaderThreadInfo->m_eCurColorArg = eCa;
		pShaderThreadInfo->m_PersFlags   |= RBPF_FP_DIRTY;
	}
	if (eAa != 255 && pShaderThreadInfo->m_eCurAlphaArg != eAa)
	{
		pShaderThreadInfo->m_eCurAlphaArg = eAa;
		pShaderThreadInfo->m_PersFlags   |= RBPF_FP_DIRTY;
	}
}

#if !CRY_PLATFORM_ORBIS
// <DEPRECATED>
void CD3D9Renderer::CopyFramebufferDX11(CTexture* pDst, D3DResource* pSrcResource, D3DFormat srcFormat)
{
	// Simulated texture copy to overcome the format dismatch issue for texture-blit.
	// TODO: use partial update.
	CShader* pShader = CShaderMan::s_shPostEffects;
	static CCryNameTSCRC techName("TextureToTexture");
	pShader->FXSetTechnique(techName);

	// Try get the pointer to the actual backbuffer
	D3DTexture* pBackBufferTex = (D3DTexture*)pSrcResource;

	// create the shader res view on the fly
	D3DShaderResource* shaderResView;               // released at the end of this func
	D3D11_SHADER_RESOURCE_VIEW_DESC svDesc;
	ZeroStruct(svDesc);
	svDesc.Format = srcFormat;
	svDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
	svDesc.Texture2D.MipLevels       = 1;
	svDesc.Texture2D.MostDetailedMip = 0;
	HRESULT hr;
	if (!SUCCEEDED(hr = GetDevice().CreateShaderResourceView(pBackBufferTex, &svDesc, &shaderResView)))
		iLog->LogError("Creating shader resource view has failed.  Code: %d", hr);

	// render
	uint32 nPasses = 0;
	pShader->FXBegin(&nPasses, FEF_DONTSETTEXTURES);
	FX_PushRenderTarget(0, pDst, NULL);
	FX_SetActiveRenderTargets();
	D3DSurface* pNullRTV = NULL;
	GetDeviceContext().OMSetRenderTargets(1, &pNullRTV, NULL);
	pShader->FXBeginPass(0);

	// Set shader resource
	m_DevMan.BindSRV(CDeviceManager::TYPE_PS, shaderResView, 0);

	// Set sampler state:
	int tsIdx = CTexture::GetTexState(STexState(FILTER_LINEAR, true));                            // get the sampler state cache line index
	ID3D11SamplerState* linearSampler = static_cast<ID3D11SamplerState*> (CTexture::s_TexStates[tsIdx].m_pDeviceState);
	m_DevMan.BindSampler(CDeviceManager::TYPE_PS, &linearSampler, 0, 1);
	SPostEffectsUtils::DrawFullScreenTri(pDst->GetWidth(), pDst->GetHeight());
	// unbind backbuffer:
	D3DShaderResource* pNullSTV = NULL;
	m_DevMan.BindSRV(CDeviceManager::TYPE_PS, pNullSTV, 0);
	CTexture::s_TexStages[0].m_DevTexture = NULL;

	pShader->FXEndPass();
	FX_PopRenderTarget(0);
	FX_SetActiveRenderTargets();
	pShader->FXEnd();

	GetDeviceContext();                                                                        // explicit flush as temp target gets released in next line
	SAFE_RELEASE(shaderResView);
	CTexture::ResetTMUs();                                                                     // Due to PSSetSamplers call state caching will be broken
}
#endif

// <DEPRECATED> This function must be refactored post C3
void CD3D9Renderer::FX_ScreenStretchRect(CTexture* pDst, CTexture* pHDRSrc)
{
	PROFILE_LABEL_SCOPE("SCREEN_STRETCH_RECT");
	if (CTexture::IsTextureExist(pDst))
	{
		int iTempX, iTempY, iWidth, iHeight;
		gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

		uint64 nPrevFlagsShaderRT = gRenDev->m_RP.m_FlagsShader_RT;
		gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE5]);

		{
			// update scene target before using it for water rendering
			CDeviceTexture* pDstResource = pDst->GetDevTexture();
			D3DSurface*  pOrigRT         = m_pNewTarget[0]->m_pTarget;
			D3DResource* pSrcResource;

			// This is a subrect to subrect copy with no resolving or stretching
			D3D11_BOX box;
			ZeroStruct(box);
			box.right  = pDst->GetWidth();
			box.bottom = pDst->GetHeight();
			box.back   = 1;

			//Allow for scissoring to happen
			int sX, sY, sWdt, sHgt;
			if (EF_GetScissorState(sX, sY, sWdt, sHgt))
			{
				box.left   = sX;
				box.right  = sX + sWdt;
				box.top    = sY;
				box.bottom = sY + sHgt;

				// Align the RECT boundaries to GPU memory layout
				box.left   = box.left & 0xfffffff8;
				box.top    = box.top & 0xfffffff8;
				box.right  = (box.right + 8) & 0xfffffff8;
				box.bottom = (box.bottom + 8) & 0xfffffff8;
			}

			D3D11_RENDER_TARGET_VIEW_DESC backbufferDesc;
			if (pOrigRT)
			{
				pOrigRT->GetResource(&pSrcResource);
				pOrigRT->GetDesc(&backbufferDesc);
				if (backbufferDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS || pHDRSrc)
				{
					// No API side for ResolveSubresourceRegion from MS target to non-ms. Need to perform custom resolve step
					if (CRenderer::CV_r_HDRRendering && (CTexture::s_ptexSceneTarget && (CTexture::s_ptexHDRTarget || pHDRSrc) && CTexture::s_ptexCurrentSceneDiffuseAccMap))
					{
						if (backbufferDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
							gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

						CTexture* pHDRTarget = pHDRSrc ? pHDRSrc : CTexture::s_ptexHDRTarget;
						pHDRTarget->SetResolved(true);

						gcpRendD3D->FX_SetMSAAFlagsRT();
						FX_PushRenderTarget(0, pDst, 0);
						FX_SetActiveRenderTargets();

						RT_SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());

						static CCryNameTSCRC pTechName("TextureToTexture");
						SPostEffectsUtils::ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
						FX_SetState(GS_NODEPTHTEST);

						pHDRTarget->Apply(0, CTexture::GetTexState(STexState(FILTER_POINT, true)), EFTT_UNKNOWN, -1, m_RP.m_MSAAData.Type ? SResourceView::DefaultViewMS : SResourceView::DefaultView);

						SPostEffectsUtils::DrawFullScreenTri(pDst->GetWidth(), pDst->GetHeight());
						SPostEffectsUtils::ShEndPass();

						// Restore previous viewport
						FX_PopRenderTarget(0);
						RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

						pHDRTarget->SetResolved(false);
					}
					else
					{
						GetDeviceContext().ResolveSubresource(pDstResource->Get2DTexture(), 0, pSrcResource, 0, backbufferDesc.Format);
					}
				}
				else
				{
#if CRY_PLATFORM_ORBIS
					GetDeviceContext().CopySubresourceRegion(pDstResource->Get2DTexture(), 0, box.left, box.top, 0, pSrcResource, 0, &box);
#else
					// Check if the format match (or the copysubregionresource call would fail)
					const D3DFormat dstFmt = CTexture::DeviceFormatFromTexFormat(pDst->GetDstFormat());
					const D3DFormat srcFmt = backbufferDesc.Format;
					if (dstFmt == srcFmt)
					{
#if !defined(_RELEASE)
						D3D11_RESOURCE_DIMENSION type;
						pSrcResource->GetType(&type);
						if (type != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
							__debugbreak();
#endif
						D3DTexture* pSrcTex2D = (D3DTexture*) pSrcResource;
						D3D11_TEXTURE2D_DESC srcTex2desc;
						pSrcTex2D->GetDesc(&srcTex2desc);

						box.left   = min(box.left, srcTex2desc.Width);
						box.right  = min(box.right, srcTex2desc.Width);
						box.top    = min(box.top, srcTex2desc.Height);
						box.bottom = min(box.bottom, srcTex2desc.Height);

						// guard against a TDR
						if (box.left < box.right && box.top < box.bottom)
							GetDeviceContext().CopySubresourceRegion(pDstResource->Get2DTexture(), 0, box.left, box.top, 0, pSrcResource, 0, &box);
#if !defined(_RELEASE)
						else
							__debugbreak();
#endif
					}
					else
					{
						// deal with format mismatch case:
						EF_Scissor(false, 0, 0, 0, 0);     // TODO: optimize. dont use full screen pass.
						CopyFramebufferDX11(pDst, pSrcResource, backbufferDesc.Format);
						EF_Scissor(true, sX, sY, sWdt, sHgt);
					}
#endif
				}
#if CRY_PLATFORM_ORBIS
				D3DTexture* pSrcResourceTex2D = (D3DTexture*)pSrcResource;
				SAFE_RELEASE(pSrcResourceTex2D);
#else
				SAFE_RELEASE(pSrcResource);
#endif
			}
		}

		gRenDev->m_RP.m_FlagsShader_RT = nPrevFlagsShaderRT;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_SkinRendering(bool bEnable)
{
	if (bEnable)
	{
		FX_ScreenStretchRect(CTexture::s_ptexCurrentSceneDiffuseAccMap, CTexture::s_ptexHDRTarget);

		RT_SetViewport(0, 0, CTexture::s_ptexSceneTarget->GetWidth(), CTexture::s_ptexSceneTarget->GetHeight());
	}
	else
	{
		FX_ResetPipe();
		gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::FX_ProcessSkinRenderLists(int nList, void (* RenderFunc)(), bool bLighting)
{
	// Forward SSS completely disabled, except for the character editor where we just do a simple forward pass
	if (m_RP.m_PersFlags2 & RBPF2_ALLOW_DEFERREDSHADING)
		return;

	const bool bUseDeferredSkin = ((m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS) && (!IsRecursiveRenderView())) && CV_r_DeferredShadingDebug != 2 && CV_r_measureoverdraw == 0;
	const bool bMSAA            = (FX_GetMSAAMode() == 1);

	//if ((m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS) && nR <= 0) && CV_r_DeferredShadingDebug != 2)
	{
		uint32 nBatchMask = SRendItem::BatchFlags(nList);
		if (nBatchMask & FB_SKIN)
		{
#ifdef DO_RENDERLOG
			if (CV_r_log)
				Logv("*** Begin skin pass ***\n");
#endif

			{
				PROFILE_LABEL_SCOPE("SKIN_GEN_PASS");

				if (bUseDeferredSkin)
					m_RP.m_PersFlags2 |= RBPF2_SKIN;

				FX_ProcessRenderList(nList, RenderFunc, bLighting);

#ifdef SUPPORTS_MSAA
				if (bMSAA)
				{
					PROFILE_LABEL_SCOPE("SKIN_GEN_PASS_SAMPLE_FREQ_PASSES");
					FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
					FX_ProcessRenderList(nList, RenderFunc, bLighting);
					FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel  freq
				}
#endif

				if (bUseDeferredSkin)
					m_RP.m_PersFlags2 &= ~RBPF2_SKIN;
			}

			if (bUseDeferredSkin)
			{
				PROFILE_LABEL_SCOPE("SKIN_APPLY_PASS");

#ifdef SUPPORTS_MSAA
				if (bMSAA)
					FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
#endif

				FX_SkinRendering(true);

#ifdef SUPPORTS_MSAA
				if (bMSAA)
					FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel freq
#endif

				FX_ProcessRenderList(nList, RenderFunc, bLighting);

#ifdef SUPPORTS_MSAA
				if (bMSAA)
				{
					PROFILE_LABEL_SCOPE("SKIN_APPLY_PASS_SAMPLE_FREQ_PASSES");
					FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
					FX_ProcessRenderList(nList, RenderFunc, bLighting);
					FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel freq
				}
#endif

				FX_SkinRendering(false);
			}

#ifdef DO_RENDERLOG
			if (CV_r_log)
				Logv("*** End skin pass ***\n");
#endif
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::FX_ProcessEyeOverlayRenderLists(int nList, void (* RenderFunc)(), bool bLighting)
{
	if (IsRecursiveRenderView())
		return;

	const bool bMSAA = (FX_GetMSAAMode() == 1);
	if (m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS)
	{
		int iTempX, iTempY, iWidth, iHeight;
		gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

		PROFILE_LABEL_SCOPE("EYE_OVERLAY");

		SDepthTexture* pCurrDepthBuffer = (gRenDev->m_RP.m_MSAAData.Type) ? &gcpRendD3D->m_DepthBufferOrigMSAA : &gcpRendD3D->m_DepthBufferOrig;

		FX_PushRenderTarget(0, CTexture::s_ptexSceneDiffuse, pCurrDepthBuffer);

		FX_ProcessRenderList(nList, RenderFunc, bLighting);

#ifdef SUPPORTS_MSAA
		if (bMSAA)
		{
			PROFILE_LABEL_SCOPE("EYE_OVERLAY_SAMPLE_FREQ_PASSES");
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
			FX_ProcessRenderList(nList, RenderFunc, bLighting);
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel  freq
		}
#endif

		FX_PopRenderTarget(0);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::FX_ProcessHalfResParticlesRenderList(CRenderView* pRenderView, int nList, void (* RenderFunc)(), bool bLighting)
{
	if (IsRecursiveRenderView())
		return;

	if (m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS)
	{
		auto& rendItems = pRenderView->GetRenderItems(nList);
		if (!rendItems.empty())
		{
			const SRendItem& ri    = rendItems[0];
			const bool bAlphaBased = CV_r_ParticlesHalfResBlendMode == 0;

#ifdef DO_RENDERLOG
			if (CV_r_log)
				Logv("*** Begin half res transparent pass ***\n");
#endif

			CTexture* pHalfResTarget = CTexture::s_ptexHDRTargetScaled[CV_r_ParticlesHalfResAmount];
			assert(CTexture::IsTextureExist(pHalfResTarget));
			if (CTexture::IsTextureExist(pHalfResTarget))
			{
				const int nHalfWidth  = pHalfResTarget->GetWidth();
				const int nHalfHeight = pHalfResTarget->GetHeight();

				PROFILE_LABEL_SCOPE("TRANSP_HALF_RES_PASS");

				// Get current viewport
				int iTempX, iTempY, iWidth, iHeight;
				GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

				FX_ClearTarget(pHalfResTarget, Clr_Empty);
				FX_PushRenderTarget(0, pHalfResTarget, NULL);
				RT_SetViewport(0, 0, nHalfWidth, nHalfHeight);

				m_RP.m_PersFlags2 |= RBPF2_HALFRES_PARTICLES;
				const uint32 nOldForceStateAnd = m_RP.m_ForceStateAnd;
				const uint32 nOldForceStateOr  = m_RP.m_ForceStateOr;
				m_RP.m_ForceStateOr = GS_NODEPTHTEST;
				if (bAlphaBased)
				{
					m_RP.m_ForceStateAnd = GS_BLSRC_SRCALPHA;
					m_RP.m_ForceStateOr |= GS_BLSRC_SRCALPHA_A_ZERO;
				}
				FX_ProcessRenderList(nList, RenderFunc, bLighting);
				m_RP.m_ForceStateAnd = nOldForceStateAnd;
				m_RP.m_ForceStateOr  = nOldForceStateOr;
				m_RP.m_PersFlags2   &= ~RBPF2_HALFRES_PARTICLES;

				FX_PopRenderTarget(0);

				{
					PROFILE_LABEL_SCOPE("UPSAMPLE_PASS");
					CShader*  pSH            = CShaderMan::s_shPostEffects;
					CTexture* pHalfResSrc    = pHalfResTarget;
					CTexture* pZTarget       = CTexture::s_ptexZTarget;
					CTexture* pZTargetScaled = CV_r_ParticlesHalfResAmount > 0 ? CTexture::s_ptexZTargetScaled2 : CTexture::s_ptexZTargetScaled;

					uint32 nStates = GS_NODEPTHTEST | GS_COLMASK_RGB;
					if (bAlphaBased)
						nStates |= GS_BLSRC_ONE | GS_BLDST_SRCALPHA;
					else
						nStates |= GS_BLSRC_ONE | GS_BLDST_ONE;

					RT_SetViewport(iTempX, iTempY, iWidth, iHeight);
					static const CCryNameTSCRC pTechNameNearestDepth("NearestDepthUpsample");
					PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechNameNearestDepth, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

					static CCryNameR pParam0Name("texToTexParams0");
					Vec4 vParam0(pZTarget->GetWidth(), pZTarget->GetHeight(), pZTargetScaled->GetWidth(), pZTargetScaled->GetHeight());
					CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &vParam0, 1);

					PostProcessUtils().SetTexture(pHalfResSrc, 1, FILTER_LINEAR);
					PostProcessUtils().SetTexture(pZTarget, 2, FILTER_POINT);
					PostProcessUtils().SetTexture(pZTargetScaled, 3, FILTER_POINT);

					FX_SetState(nStates);
					PostProcessUtils().DrawFullScreenTri(m_width, m_height);

					PostProcessUtils().ShEndPass();
				}
			}

#ifdef DO_RENDERLOG
			if (CV_r_log)
				Logv("*** End half res transparent pass ***\n");
#endif
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef SUPPORTS_MSAA

void CD3D9Renderer::FX_MSAACustomResolve()
{
	// unbind any resource
	FX_ResetPipe();
	RT_UnbindTMUs();

	PROFILE_LABEL_SCOPE("MSAA CUSTOM RESOLVE");

	// Resolve pass outputs minZ/corresponding normal reusing samples to tag depth discontinuities for MSAA  (this can also be adapted for different resolve schemes)
	m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);

	const int32 nTexStatePoint = CTexture::GetTexState(STexState(FILTER_POINT, true));
	const int32 nWidth         = CTexture::s_ptexZTarget->GetWidth();
	const int32 nHeight        = CTexture::s_ptexZTarget->GetHeight();
	const bool  bReverseDepth  = !!(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH);

	FX_SetMSAAFlagsRT();

	// Setup shader resources first, to avoid implicit resolves (d3dfxpipeline.cpp ln 301) & unsetting of rendertargets (d3dtexture.cpp ln 4222).
	CTexture::s_ptexSceneNormalsMap->SetResolved(true);
	CTexture::s_ptexSceneNormalsMap->SetUseMultisampledRTV(false);
	CTexture::s_ptexZTarget->SetResolved(true);
	CTexture::s_ptexZTarget->SetUseMultisampledRTV(false);

	const SResourceView::KeyType nResourceViewID = SResourceView::DefaultViewMS;
	CTexture::s_ptexZTarget->Apply(0, nTexStatePoint, EFTT_UNKNOWN, -1, nResourceViewID);
	CTexture::s_ptexSceneNormalsMap->Apply(1, nTexStatePoint, EFTT_UNKNOWN, -1, nResourceViewID);

	CTexture::s_ptexSceneDiffuse->SetResolved(true);
	CTexture::s_ptexSceneDiffuse->SetUseMultisampledRTV(false);
	CTexture::s_ptexSceneDiffuse->Apply(2, nTexStatePoint, EFTT_UNKNOWN, -1, nResourceViewID);
	CTexture::s_ptexSceneSpecular->SetResolved(true);
	CTexture::s_ptexSceneSpecular->SetUseMultisampledRTV(false);
	CTexture::s_ptexSceneSpecular->Apply(3, nTexStatePoint, EFTT_UNKNOWN, -1, nResourceViewID);

	CTexture* pDepthBufferRT           = CTexture::s_ptexZTarget;
	D3DShaderResource* pZTargetOrigSRV = pDepthBufferRT->GetShaderResourceView(SResourceView::DefaultViewMS);
	pDepthBufferRT->SetShaderResourceView(gcpRendD3D->m_DepthBufferOrigMSAA.pTexture->GetDeviceDepthReadOnlySRV(0, -1, true), true);  // override shader resource view with device depth
	pDepthBufferRT->Apply(4, nTexStatePoint, EFTT_UNKNOWN, -1, nResourceViewID);

	// Stencil initialized to 1 - 0 is reserved for MSAAed samples
	FX_ClearTarget(&m_DepthBufferOrig, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_R.r, 1);

	FX_PushRenderTarget(0, CTexture::s_ptexZTarget, &m_DepthBufferOrig);
	FX_PushRenderTarget(1, CTexture::s_ptexSceneNormalsMap, NULL);
	FX_PushRenderTarget(2, CTexture::s_ptexBackBuffer, NULL);

	FX_PushRenderTarget(3, CTexture::s_ptexSceneDiffuse, NULL);
	FX_PushRenderTarget(4, CTexture::s_ptexSceneSpecular, NULL);

	RT_SetViewport(0, 0, nWidth, nHeight);

	static CCryNameTSCRC pszTechMSAACustomResolve("MSAACustomResolve");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shDeferredShading, pszTechMSAACustomResolve, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	int32 nState = GS_DEPTHFUNC_LEQUAL;
	nState &= ~GS_NODEPTHTEST;
	nState |= GS_DEPTHWRITE;
	FX_SetState(nState);

	const Vec4 vMSAAResolveParams(0, 0, CV_r_msaa_threshold_depth, CV_r_msaa_threshold_normal);
	static CCryNameR szMSAAParamsName("MSAAResolveParams");
	CShaderMan::s_shDeferredShading->FXSetPSFloat(szMSAAParamsName, &vMSAAResolveParams, 1);

	D3DSetCull(eCULL_None);
	GetUtils().DrawQuadFS(CShaderMan::s_shDeferredShading, true, nWidth, nHeight);
	GetUtils().ShEndPass();

	FX_PopRenderTarget(0);
	FX_PopRenderTarget(1);
	FX_PopRenderTarget(2);
	FX_PopRenderTarget(3);
	FX_PopRenderTarget(4);

	CTexture::s_ptexSceneDiffuse->SetUseMultisampledRTV(true);
	CTexture::s_ptexSceneDiffuse->SetResolved(true);
	CTexture::s_ptexSceneSpecular->SetUseMultisampledRTV(true);
	CTexture::s_ptexSceneSpecular->SetResolved(true);

	pDepthBufferRT->SetShaderResourceView(pZTargetOrigSRV, true);  // restore shader resource view
	CTexture::s_ptexZTarget->SetUseMultisampledRTV(true);
	CTexture::s_ptexSceneNormalsMap->SetUseMultisampledRTV(true);
	CTexture::s_ptexZTarget->SetResolved(true);
	CTexture::s_ptexSceneNormalsMap->SetResolved(true);

	FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_MASK_CLEAR_STENCIL | MSAA_SAMPLEFREQ_MASK_SET);
	m_RP.m_PersFlags2 |= RBPF2_WRITEMASK_RESERVED_STENCIL_BIT;

	FX_ResetMSAAFlagsRT();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::FX_MSAARestoreSampleMask(bool bForceRestore)
{
	// If a stencil clear occurred we have to restore the per-sample mask
	if ((m_RP.m_PersFlags2 & RBPF2_MSAA_RESTORE_SAMPLE_MASK) || bForceRestore)
	{
		m_RP.m_PersFlags2 &= ~(RBPF2_MSAA_RESTORE_SAMPLE_MASK | RBPF2_WRITEMASK_RESERVED_STENCIL_BIT);
		FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_MASK_SET);
		m_RP.m_PersFlags2 |= RBPF2_WRITEMASK_RESERVED_STENCIL_BIT;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::FX_MSAASampleFreqStencilSetup(const uint32 nMSAAFlags, const uint8 nStencilRef)
{
	m_RP.m_PersFlags2 &= ~(RBPF2_MSAA_SAMPLEFREQ_PASS | RBPF2_MSAA_STENCILCULL);
	m_RP.m_PersFlags2 |= (nMSAAFlags & MSAA_SAMPLEFREQ_PASS) ? RBPF2_MSAA_SAMPLEFREQ_PASS : 0;
	m_RP.m_PersFlags2 |= (nMSAAFlags & MSAA_STENCILCULL) ? RBPF2_MSAA_STENCILCULL : 0;

	FX_SetMSAAFlagsRT();

	const ECull nPrevCull = m_RP.m_eCull;
	int32 sX              = 0, sY = 0, sWdt = 0, sHgt = 0;
	float fDepthBoundsMin = 0.0f, fDepthBoundsMax = 1.0f;
	bool  bScissorState   = false, bDepthBoundsState = false;

	if (nMSAAFlags & (MSAA_SAMPLEFREQ_MASK_CLEAR_STENCIL | MSAA_STENCILMASK_SET | MSAA_STENCILMASK_RESET_BIT | MSAA_SAMPLEFREQ_MASK_SET))
	{
		// Ensure no scissoring/depthbounds enabled, we want to full control for mask setup
		bScissorState     = EF_GetScissorState(sX, sY, sWdt, sHgt);
		bDepthBoundsState = GetDepthBoundTestState(fDepthBoundsMin, fDepthBoundsMax);
		EF_Scissor(false, sX, sY, sWdt, sHgt);
		SetDepthBoundTest(fDepthBoundsMin, fDepthBoundsMax, false);
		D3DSetCull(eCULL_Back);
	}

	const int32 nWidth  = CTexture::s_ptexZTarget->GetWidth();
	const int32 nHeight = CTexture::s_ptexZTarget->GetHeight();
	if (nMSAAFlags & (MSAA_SAMPLEFREQ_MASK_CLEAR_STENCIL))
	{
		PROFILE_LABEL_SCOPE("CLEAR RESERVED STENCIL");
		// Clear stencil reserved bit
		static CCryNameTSCRC pszTechClearReservedStencil("ClearReservedStencil");
		PostProcessUtils().ShBeginPass(CShaderMan::s_shDeferredShading, pszTechClearReservedStencil, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		static const int32 nStState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_REPLACE) | STENCOP_ZFAIL(FSS_STENCOP_REPLACE) | STENCOP_PASS(FSS_STENCOP_REPLACE);
		m_RP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;
		FX_SetStencilState(nStState, 0, 0xFF, BIT_STENCIL_RESERVED, true);
		FX_SetState(GS_STENCIL | GS_NODEPTHTEST | GS_COLMASK_NONE);
		GetUtils().DrawQuadFS(CShaderMan::s_shDeferredShading, false, nWidth, nHeight);
		GetUtils().ShEndPass();
		m_RP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;
	}

	if (nMSAAFlags & (MSAA_STENCILMASK_SET | MSAA_STENCILMASK_RESET_BIT | MSAA_SAMPLEFREQ_MASK_SET))
	{
		PROFILE_LABEL_SCOPE("SAMPLEFREQPASS_SETUP");

		// This pass writes depth discontinuities mask to stencil
		static CCryNameTSCRC pszTechMSAASampleFreqStencilMask("MSAASampleFreqStencilMask");
		PostProcessUtils().ShBeginPass(CShaderMan::s_shDeferredShading, pszTechMSAASampleFreqStencilMask, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		uint32 nStRef       = (nMSAAFlags & MSAA_STENCILMASK_RESET_BIT) != 0 ? nStencilRef : gcpRendD3D->m_nStencilMaskRef;
		uint32 nStWriteMask = 0xFF;
		if (nMSAAFlags & MSAA_SAMPLEFREQ_MASK_SET)
		{
			m_RP.m_PersFlags2 &= ~RBPF2_MSAA_RESTORE_SAMPLE_MASK;
			m_RP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;
			nStRef             = BIT_STENCIL_RESERVED;
			nStWriteMask       = BIT_STENCIL_RESERVED;
		}

		static const int32 nStState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_REPLACE) | STENCOP_ZFAIL(FSS_STENCOP_REPLACE) | STENCOP_PASS(FSS_STENCOP_REPLACE);
		FX_SetStencilState(nStState, nStRef, 0xFF, nStWriteMask);
		FX_SetState(GS_STENCIL | GS_NODEPTHTEST | GS_COLMASK_NONE);

		CTexture::s_ptexBackBuffer->Apply(0, CTexture::GetTexState(STexState(FILTER_POINT, true)));
		GetUtils().DrawQuadFS(CShaderMan::s_shDeferredShading, false, nWidth, nHeight);
		GetUtils().ShEndPass();

		if (nMSAAFlags & MSAA_SAMPLEFREQ_MASK_SET)
			m_RP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;
	}

	if ((nMSAAFlags & MSAA_STENCILCULL))
	{
		const bool bStFuncEq = (m_RP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS) ? true : false;
		FX_StencilTestCurRef((nMSAAFlags & MSAA_STENCILCULL) != 0, true, bStFuncEq);
	}

	if (nMSAAFlags & (MSAA_SAMPLEFREQ_MASK_CLEAR_STENCIL | MSAA_STENCILMASK_SET | MSAA_STENCILMASK_RESET_BIT | MSAA_SAMPLEFREQ_MASK_SET))
	{
		EF_Scissor(bScissorState, sX, sY, sWdt, sHgt);
		SetDepthBoundTest(fDepthBoundsMin, fDepthBoundsMax, bDepthBoundsState);
		D3DSetCull(nPrevCull);
	}
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Output g-buffer

bool CD3D9Renderer::FX_ZScene(bool bEnable, bool bUseHDR, bool bClearZBuffer, bool bRenderNormalsOnly, bool bZPrePass)
{
	uint32 nDiffuseTargetID              = 1;
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	if (bEnable)
	{
		if (m_LogFile)
			Logv(" +++ Start Z scene +++ \n");

		const int nWidth  = m_MainViewport.nWidth;
		const int nHeight = m_MainViewport.nHeight;
		RECT rect         = { 0, 0, nWidth, nHeight };

		int nStates = GS_DEPTHWRITE;

		FX_PushRenderTarget(0, CTexture::s_ptexSceneNormalsMap, &m_DepthBufferOrigMSAA, -1, true);

		if (!bZPrePass)
		{
			FX_PushRenderTarget(nDiffuseTargetID, CTexture::s_ptexSceneDiffuse, NULL);

			CTexture* pSceneSpecular = CTexture::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
			pSceneSpecular = CTexture::s_ptexSceneSpecularESRAM;
#endif

			FX_PushRenderTarget(nDiffuseTargetID + 1, pSceneSpecular, NULL);
		}

		FX_SetState(nStates);

		RT_SetViewport(0, 0, nWidth, nHeight);

		FX_SetActiveRenderTargets();

		pShaderThreadInfo->m_PersFlags |= RBPF_ZPASS;
		if (CTexture::s_eTFZ == eTF_R32F || CTexture::s_eTFZ == eTF_R16G16F || CTexture::s_eTFZ == eTF_R16G16B16A16F || CTexture::s_eTFZ == eTF_D24S8 || CTexture::s_eTFZ == eTF_D32FS8)
		{
			m_RP.m_PersFlags2 |= RBPF2_NOALPHABLEND | (bZPrePass ? (RBPF2_ZPREPASS | RBPF2_DISABLECOLORWRITES) : RBPF2_NOALPHATEST);
			m_RP.m_StateAnd   &= ~(GS_BLEND_MASK | GS_ALPHATEST);
			m_RP.m_StateAnd   |= bZPrePass ? GS_ALPHATEST : 0;
		}
	}
	else if (pShaderThreadInfo->m_PersFlags & RBPF_ZPASS)
	{
		if (m_LogFile)
			Logv(" +++ End Z scene +++ \n");

		pShaderThreadInfo->m_PersFlags &= ~RBPF_ZPASS;
		if (CTexture::s_eTFZ == eTF_R16G16F || CTexture::s_eTFZ == eTF_R32F || CTexture::s_eTFZ == eTF_R16G16B16A16F || CTexture::s_eTFZ == eTF_D24S8 || CTexture::s_eTFZ == eTF_D32FS8)
		{
			m_RP.m_PersFlags2 &= ~(RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST | RBPF2_ZPREPASS | RBPF2_DISABLECOLORWRITES);
			m_RP.m_StateAnd   |= (GS_BLEND_MASK | GS_ALPHATEST);
		}

		FX_PopRenderTarget(0);

		if (!bZPrePass)
		{
			FX_PopRenderTarget(nDiffuseTargetID);
			FX_PopRenderTarget(nDiffuseTargetID + 1);
			if (m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)
			{
				FX_PopRenderTarget(nDiffuseTargetID + 2);
				m_RP.m_PersFlags2 &= ~RBPF2_MOTIONBLURPASS;
			}
		}

		if (bRenderNormalsOnly)
		{
#ifdef SUPPORTS_MSAA
			if (m_RP.m_MSAAData.Type)
			{
				FX_Commit();
				// Resolve depth/normals/setup edges
				FX_MSAACustomResolve();
			}
#endif

			CTexture::s_ptexZTarget->Resolve();
		}
	}
	else
	{
		if (!CV_r_usezpass)
			CTexture::DestroyZMaps();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::FX_RenderForwardOpaque(void (* RenderFunc)(), const bool bLighting, const bool bAllowDeferred)
{
	// Note: MSAA for deferred lighting requires extra pass using per-sample frequency for tagged undersampled regions
	//  for future: this could be avoided (while maintaining current architecture), by using MRT output then a composite step
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	if (CV_r_measureoverdraw == 4)
	{
		// TODO: remove redundancy
		SetClearColor(Vec3Constants<float>::fVec3_Zero);
		EF_ClearTargetsLater(FRT_CLEAR_COLOR, Clr_Empty);
	}

	PROFILE_LABEL_SCOPE("OPAQUE_PASSES");

	const bool bMSAA = (FX_GetMSAAMode() == 1);
#ifdef SUPPORTS_MSAA
	if (bMSAA)
		FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_STENCILMASK_SET);   // set stencil mask and enable stencil culling for pixel freq
#endif

	const bool bShadowGenSpritePasses = (pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE) != 0;

	if ((m_RP.m_PersFlags2 & RBPF2_ALLOW_DEFERREDSHADING) && !bShadowGenSpritePasses && (!IsRecursiveRenderView()) && !m_wireframe_mode)
		m_RP.m_PersFlags2 |= RBPF2_FORWARD_SHADING_PASS;

	if (!bShadowGenSpritePasses)
	{
		// Note: Eye overlay writes to diffuse color buffer for eye shader reading
		FX_ProcessEyeOverlayRenderLists(EFSLIST_EYE_OVERLAY, RenderFunc, bLighting);
	}

	{
		PROFILE_LABEL_SCOPE("FORWARD_OPAQUE");

		GetTiledShading().BindForwardShadingResources(NULL);

		FX_ProcessRenderList(EFSLIST_FORWARD_OPAQUE, RenderFunc, bLighting);

		GetTiledShading().UnbindForwardShadingResources();

#ifdef SUPPORTS_MSAA
		if (bMSAA)
		{
			PROFILE_LABEL_SCOPE("FORWARD_OPAQUE_SAMPLE_FREQ_PASSES");
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
			FX_ProcessRenderList(EFSLIST_FORWARD_OPAQUE, RenderFunc, bLighting);
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel freq
		}
#endif
	}

	{
		PROFILE_LABEL_SCOPE("TERRAINLAYERS");

		FX_ProcessRenderList(EFSLIST_TERRAINLAYER, RenderFunc, bLighting);

#ifdef SUPPORTS_MSAA
		if (bMSAA)
		{
			PROFILE_LABEL_SCOPE("TERRAINLAYERS_SAMPLE_FREQ_PASSES");
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
			FX_ProcessRenderList(EFSLIST_TERRAINLAYER, RenderFunc, bLighting);
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel freq
		}
#endif
	}

	{
		PROFILE_LABEL_SCOPE("FORWARD_DECALS");
		FX_ProcessRenderList(EFSLIST_DECAL, RenderFunc, bLighting);

#ifdef SUPPORTS_MSAA
		if (bMSAA)
		{
			PROFILE_LABEL_SCOPE("FORWARD_DECALS_SAMPLE_FREQ_PASSES");
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL | MSAA_SAMPLEFREQ_PASS);   // sample freq
			FX_ProcessRenderList(EFSLIST_DECAL, RenderFunc, bLighting);
			FX_MSAASampleFreqStencilSetup(MSAA_STENCILCULL);                        // pixel freq
		}
#endif
	}

	if (!bShadowGenSpritePasses)
	{
		// Note: Do not swap render order with decals it breaks light acc buffer.
		//	-	PC could actually work via accumulation into light acc target
		{
			FX_ProcessSkinRenderLists(EFSLIST_SKIN, RenderFunc, bLighting);
		}
	}

#ifdef SUPPORTS_MSAA
	FX_MSAASampleFreqStencilSetup();                                            // disable msaa passes setup
#endif

	m_RP.m_PersFlags2 &= ~RBPF2_FORWARD_SHADING_PASS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::FX_LinearizeDepth()
{
	PROFILE_LABEL_SCOPE("LINEARIZE_DEPTH");

#ifdef SUPPORTS_MSAA
	if (FX_GetMSAAMode())
		FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_PASS);
#endif

	FX_PushRenderTarget(0, CTexture::s_ptexZTarget, NULL);

#if DEVICE_MANAGER_IMMEDIATE_STATE_WRITE
	// OMSetRenderTargets must occur before PSSetShaderResources
	FX_SetActiveRenderTargets();
#endif

	static const CCryNameTSCRC pTechName("LinearizeDepth");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	FX_SetState(GS_NODEPTHTEST);

	D3DShaderResource* depthReadOnlySRV = gcpRendD3D->m_DepthBufferOrigMSAA.pTexture->GetDeviceDepthReadOnlySRV(0, -1, false);


	gcpRendD3D->m_DepthBufferOrigMSAA.pTexture->ApplyTexture(0);
#if defined(OPENGL_ES)
	auto ts = CTexture::GetTexState(STexState(FILTER_POINT, true));
	gcpRendD3D->m_DepthBufferOrigMSAA.pTexture->ApplySamplerState(0, eHWSC_Pixel, ts);
#endif
	static CCryNameR pParamName0("NearProjection");

	I3DEngine* pEng = gEnv->p3DEngine;

	float zn = DRAW_NEAREST_MIN;
	float zf = CV_r_DrawNearFarPlane;

	float fNearZRange = CV_r_DrawNearZRange;
	float fCamScale   = (zf / pEng->GetMaxViewDistance());

	const bool bReverseDepth = (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

	Vec4 NearProjectionParams;
	NearProjectionParams.x = bReverseDepth ? 1.0f - zf / (zf - zn) * fNearZRange      : zf / (zf - zn) * fNearZRange;
	NearProjectionParams.y = bReverseDepth ? zn / (zf - zn) * fNearZRange * fCamScale : zn / (zn - zf) * fNearZRange * fCamScale;
	NearProjectionParams.z = bReverseDepth ? 1.0 - (fNearZRange - 0.001f)      : fNearZRange - 0.001f;
	NearProjectionParams.w = 1.0f;
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName0, &NearProjectionParams, 1);

	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexZTarget->GetWidth(), CTexture::s_ptexZTarget->GetHeight());

	D3DShaderResource* pNullSRV[1] = { NULL };
	m_DevMan.BindSRV(CDeviceManager::TYPE_PS, pNullSRV, 16, 1);

	PostProcessUtils().ShEndPass();

	FX_PopRenderTarget(0);

#ifdef SUPPORTS_MSAA
	FX_MSAASampleFreqStencilSetup();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::FX_DepthFixupMerge()
{
	PROFILE_LABEL_SCOPE("MERGE_DEPTH");

	// Merge linear depth with depth values written for transparent objects
	FX_PushRenderTarget(0, CTexture::s_ptexZTarget, NULL);
	RT_SetViewport(0, 0, CTexture::s_ptexZTarget->GetWidth(), CTexture::s_ptexZTarget->GetHeight());
	static const CCryNameTSCRC pTechName("TranspDepthFixupMerge");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	PostProcessUtils().SetTexture(CTexture::s_ptexHDRTarget, 0, FILTER_POINT);
	FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLEND_OP_MIN);
	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexZTarget->GetWidth(), CTexture::s_ptexZTarget->GetHeight());
	PostProcessUtils().ShEndPass();
	FX_PopRenderTarget(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_HDRScene(bool bEnableHDR, bool bClear)
{
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	if (bEnableHDR)
	{
		if (m_LogFile)
			Logv(" +++ Start HDR scene +++ \n");

		if (!CTexture::s_ptexHDRTarget || CTexture::s_ptexHDRTarget->IsMSAAChanged() || CTexture::s_ptexHDRTarget->GetWidth() != GetWidth() || CTexture::s_ptexHDRTarget->GetHeight() != GetHeight())
			CTexture::GenerateHDRMaps();

		bool bEmpty = SRendItem::IsListEmpty(EFSLIST_HDRPOSTPROCESS);
		if (bEmpty)
			return false;

		FX_HDRRangeAdaptUpdate();

		if (bClear || (pShaderThreadInfo->m_PersFlags & RBPF_MIRRORCULL) || (m_RP.m_nRendFlags & SHDF_CUBEMAPGEN))
		{
			FX_ClearTarget(CTexture::s_ptexHDRTarget);
			FX_ClearTarget(&m_DepthBufferOrigMSAA);
		}

		FX_PushRenderTarget(0, CTexture::s_ptexHDRTarget, &m_DepthBufferOrigMSAA, -1, true);

		pShaderThreadInfo->m_PersFlags |= RBPF_HDR;
	}
	else if (!CV_r_HDRRendering && CTexture::s_ptexHDRTarget)
	{
		if (m_LogFile)
			Logv(" +++ End HDR scene +++ \n");
	}
	return true;
}

// Draw overlay geometry in wireframe mode
void CD3D9Renderer::FX_DrawWire()
{
	float fColor = 1.f;
	int nState   = GS_WIREFRAME;

	if (CV_r_showlines == 1)
		nState |= GS_NODEPTHTEST;

	if (CV_r_showlines == 3)
	{
		if (!gcpRendD3D->m_RP.m_pRE || !gcpRendD3D->m_RP.m_pRE->m_CustomData)
			return; // draw only terrain
		nState |= GS_BLSRC_DSTCOL | GS_BLDST_ONE;
		fColor  = .25f;
	}

	gcpRendD3D->FX_SetState(nState);
	gcpRendD3D->SetMaterialColor(fColor, fColor, fColor, 1.f);
	CTexture::s_ptexWhite->Apply(0);
	gcpRendD3D->EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, (eCA_Texture | (eCA_Constant << 3)), (eCA_Texture | (eCA_Constant << 3)));
	CRenderObject* pObj = gcpRendD3D->m_RP.m_pCurObject;
	gcpRendD3D->FX_SetFPMode();
	gcpRendD3D->m_RP.m_pCurObject = pObj;

	uint32 i;
	if (gcpRendD3D->m_RP.m_pCurPass)
	{
		for (int nRE = 0; nRE <= gcpRendD3D->m_RP.m_nLastRE; nRE++)
		{
			gcpRendD3D->m_RP.m_pRE = gcpRendD3D->m_RP.m_RIs[nRE][0]->pElem;
			if (gcpRendD3D->m_RP.m_pRE)
			{
				EDataType t = gcpRendD3D->m_RP.m_pRE->mfGetType();
				if (t != eDATA_Mesh && t != eDATA_Terrain && t != eDATA_ClientPoly)
					continue;
				gcpRendD3D->m_RP.m_pRE->mfPrepare(false);
				gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_eVertexFormat, 0, gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_nFrameUpdateID);
			}

			CHWShader_D3D* curVS = (CHWShader_D3D*)gcpRendD3D->m_RP.m_pCurPass->m_VShader;
			for (i = 0; i < gcpRendD3D->m_RP.m_RIs[nRE].Num(); i++)
			{
				SRendItem* pRI = gcpRendD3D->m_RP.m_RIs[nRE][i];
				gcpRendD3D->FX_SetObjectTransform(pRI->pObj, NULL, pRI->pObj->m_ObjFlags);
				curVS->mfSetParametersPI(NULL, gcpRendD3D->m_RP.m_pShader);
				gcpRendD3D->FX_Commit();
				gcpRendD3D->FX_DrawRE(gcpRendD3D->m_RP.m_pShader, NULL);
			}
		}
	}
}

// Draw geometry normal vectors
void CD3D9Renderer::FX_DrawNormals()
{
	HRESULT h = S_OK;

	const size_t maxBufferSize  = std::min((size_t)NextPower2(gRenDev->CV_r_transient_pool_size) << 20, (size_t)(4096 * 1024));
	CryScopedAllocWithSizeVector(SVF_P3F_C4B_T2F, maxBufferSize / sizeof(SVF_P3F_C4B_T2F), Verts, CDeviceBufferManager::AlignBufferSizeForStreaming);
	
	float len = CRenderer::CV_r_normalslength;
	int StrVrt, StrTan, StrNorm;
	//if (gcpRendD3D->m_RP.m_pRE)
	//  gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_VertexFormatId, SHPF_NORMALS);

	for (int nRE = 0; nRE <= gcpRendD3D->m_RP.m_nLastRE; nRE++)
	{
		gcpRendD3D->m_RP.m_pRE = gcpRendD3D->m_RP.m_RIs[nRE][0]->pElem;
		if (gcpRendD3D->m_RP.m_pRE)
		{
			if (nRE)
			{
				gcpRendD3D->m_RP.m_pRE->mfPrepare(false);
			}
			gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_eVertexFormat, -1, gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_nFrameUpdateID);
		}

		const byte* verts = (const byte*)gcpRendD3D->EF_GetPointer(eSrcPointer_Vert, &StrVrt, eType_FLOAT, eSrcPointer_Vert, 0);
		const byte* normals = (const byte*)gcpRendD3D->EF_GetPointer(eSrcPointer_Normal, &StrNorm, eType_FLOAT, eSrcPointer_Normal, 0);
		const byte* tangents = (const byte*)gcpRendD3D->EF_GetPointer(eSrcPointer_Tangent, &StrTan, eType_FLOAT, eSrcPointer_Tangent, 0);

		verts    = ((INT_PTR)verts > 256 && StrVrt >= sizeof(Vec3)) ? verts : 0;
		normals  = ((INT_PTR)normals > 256 && StrNorm >= sizeof(SPipNormal)) ? normals : 0;
		tangents = ((INT_PTR)tangents > 256 && (StrTan == sizeof(SPipQTangents) || StrTan == sizeof(SPipTangents))) ? tangents : 0;

		if (verts && (normals || tangents))
		{
			gcpRendD3D->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F);
			gcpRendD3D->EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse | (eCA_Diffuse << 3)), (eCA_Diffuse | (eCA_Diffuse << 3)));
			gcpRendD3D->FX_SetFPMode();
			CTexture::s_ptexWhite->Apply(0);
			int nStateFlags = 0;
			if (gcpRendD3D->m_wireframe_mode == R_SOLID_MODE)
				nStateFlags = GS_DEPTHWRITE;
			if (CV_r_shownormals == 2)
				nStateFlags = GS_NODEPTHTEST;
			gcpRendD3D->FX_SetState(nStateFlags);
			gcpRendD3D->D3DSetCull(eCULL_None);

			//gcpRendD3D->GetDevice().SetVertexShader(NULL);

			// We must limit number of vertices, because TempDynVB (see code below)
			// uses transient pool that has *limited* size. See DevBuffer.cpp for details.
			// Note that one source vertex produces *two* buffer vertices (endpoints of
			// a normal vector).
			const size_t maxVertexCount = maxBufferSize / (2 * sizeof(SVF_P3F_C4B_T2F));
			const int numVerts          = (int)(std::min)((size_t)gcpRendD3D->m_RP.m_RendNumVerts, maxVertexCount);

			uint32 col0 = 0x000000ff;
			uint32 col1 = 0x00ffffff;

			const bool bHasNormals = (normals != 0);

			for (int v = 0; v < numVerts; v++, verts += StrVrt, normals += StrNorm, tangents += StrTan)
			{
				const float* const fverts = (const float*)verts;

				Vec3 vNorm;
				if (bHasNormals)
				{
					vNorm = ((const SPipNormal*)normals)->GetN();
				}
				else if (StrTan == sizeof(SPipQTangents))
				{
					vNorm = ((const SPipQTangents*)tangents)->GetN();
				}
				else
				{
					vNorm = ((const SPipTangents*)tangents)->GetN();
				}
				vNorm.Normalize();

				Verts[v * 2].xyz          = Vec3(fverts[0], fverts[1], fverts[2]);
				Verts[v * 2].color.dcolor = col0;

				Verts[v * 2 + 1].xyz          = Vec3(fverts[0] + vNorm[0] * len, fverts[1] + vNorm[1] * len, fverts[2] + vNorm[2] * len);
				Verts[v * 2 + 1].color.dcolor = col1;
			}

			TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(Verts, numVerts * 2, 0);

			if (gcpRendD3D->m_RP.m_pCurPass)
			{
				CHWShader_D3D* curVS = (CHWShader_D3D*)gcpRendD3D->m_RP.m_pCurPass->m_VShader;
				for (uint32 i = 0; i < gcpRendD3D->m_RP.m_RIs[nRE].Num(); i++)
				{
					SRendItem* pRI = gcpRendD3D->m_RP.m_RIs[nRE][i];
					gcpRendD3D->FX_SetObjectTransform(pRI->pObj, NULL, pRI->pObj->m_ObjFlags);
					curVS->mfSetParametersPI(NULL, gcpRendD3D->m_RP.m_pShader);
					gcpRendD3D->FX_Commit();

					gcpRendD3D->FX_DrawPrimitive(eptLineList, 0, numVerts * 2);
				}
			}

			gcpRendD3D->m_RP.m_VertexStreams[0].pStream = NULL;
		}
	}
}

// Draw geometry tangent vectors
void CD3D9Renderer::FX_DrawTangents()
{
	HRESULT h = S_OK;

	const size_t maxBufferSize  = std::min((size_t)NextPower2(gRenDev->CV_r_transient_pool_size) << 20, (size_t)(4096 * 1024));
	CryScopedAllocWithSizeVector(SVF_P3F_C4B_T2F, maxBufferSize / sizeof(SVF_P3F_C4B_T2F), Verts, CDeviceBufferManager::AlignBufferSizeForStreaming);
	
	float len = CRenderer::CV_r_normalslength;
	//if (gcpRendD3D->m_RP.m_pRE)
	//  gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_VertexFormatId, SHPF_TANGENTS);
	for (int nRE = 0; nRE <= gcpRendD3D->m_RP.m_nLastRE; nRE++)
	{
		gcpRendD3D->m_RP.m_pRE = gcpRendD3D->m_RP.m_RIs[nRE][0]->pElem;
		if (gcpRendD3D->m_RP.m_pRE)
		{
			if (nRE)
				gcpRendD3D->m_RP.m_pRE->mfPrepare(false);
			gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_eVertexFormat, -1, gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_nFrameUpdateID);
		}

		int StrVrt, StrTan;

		const byte* verts = (const byte*)gcpRendD3D->EF_GetPointer(eSrcPointer_Vert, &StrVrt, eType_FLOAT, eSrcPointer_Vert, 0);
		const byte* tangents = (const byte*)gcpRendD3D->EF_GetPointer(eSrcPointer_Tangent, &StrTan, eType_FLOAT, eSrcPointer_Tangent, 0);

		verts    = ((INT_PTR)verts > 256 && StrVrt >= sizeof(Vec3)) ? verts : 0;
		tangents = ((INT_PTR)tangents > 256 && (StrTan == sizeof(SPipQTangents) || StrTan == sizeof(SPipTangents))) ? tangents : 0;

		if (verts && tangents)
		{
			CTexture::s_ptexWhite->Apply(0);
			gcpRendD3D->EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse | (eCA_Diffuse << 3)), (eCA_Diffuse | (eCA_Diffuse << 3)));
			int nStateFlags = 0;
			if (gcpRendD3D->m_wireframe_mode == R_SOLID_MODE)
				nStateFlags = GS_DEPTHWRITE;
			if (CV_r_shownormals == 2)
				nStateFlags = GS_NODEPTHTEST;
			gcpRendD3D->FX_SetState(nStateFlags);
			gcpRendD3D->D3DSetCull(eCULL_None);
			gcpRendD3D->FX_SetFPMode();
			gcpRendD3D->FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F);

			// We must limit number of vertices, because TempDynVB (see code below)
			// uses transient pool that has *limited* size. See DevBuffer.cpp for details.
			// Note that one source vertex produces *six* buffer vertices (three tangent space
			// vectors, two vertices per vector).
			const size_t maxVertexCount = maxBufferSize / (6 * sizeof(SVF_P3F_C4B_T2F));
			const int numVerts          = (int)(std::min)((size_t)gcpRendD3D->m_RP.m_RendNumVerts, maxVertexCount);

			for (int v = 0; v < numVerts; v++, verts += StrVrt, tangents += StrTan)
			{
				uint32 col0      = 0xffff0000;
				uint32 col1      = 0xffffffff;
				const Vec3& vPos = *(const Vec3*)verts;
				Vec3 vTan, vBiTan, vNorm;

				if (StrTan == sizeof(SPipQTangents))
				{
					const Quat q = ((const SPipQTangents*)tangents)->GetQ();
					vTan   = q.GetColumn0();
					vBiTan = q.GetColumn1();
					vNorm  = ((const SPipQTangents*)tangents)->GetN();
				}
				else
				{
					((const SPipTangents*)tangents)->GetTBN(vTan, vBiTan, vNorm);
				}

				Verts[v * 6 + 0].xyz          = vPos;
				Verts[v * 6 + 0].color.dcolor = col0;

				Verts[v * 6 + 1].xyz          = Vec3(vPos[0] + vTan[0] * len, vPos[1] + vTan[1] * len, vPos[2] + vTan[2] * len);
				Verts[v * 6 + 1].color.dcolor = col1;

				col0 = 0x0000ff00;
				col1 = 0x00ffffff;

				Verts[v * 6 + 2].xyz          = vPos;
				Verts[v * 6 + 2].color.dcolor = col0;

				Verts[v * 6 + 3].xyz          = Vec3(vPos[0] + vBiTan[0] * len, vPos[1] + vBiTan[1] * len, vPos[2] + vBiTan[2] * len);
				Verts[v * 6 + 3].color.dcolor = col1;

				col0 = 0x000000ff;
				col1 = 0x00ffffff;

				Verts[v * 6 + 4].xyz          = vPos;
				Verts[v * 6 + 4].color.dcolor = col0;

				Verts[v * 6 + 5].xyz          = Vec3(vPos[0] + vNorm[0] * len, vPos[1] + vNorm[1] * len, vPos[2] + vNorm[2] * len);
				Verts[v * 6 + 5].color.dcolor = col1;
			}

			TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(Verts, numVerts * 6, 0);

			if (gcpRendD3D->m_RP.m_pCurPass)
			{
				CHWShader_D3D* curVS = (CHWShader_D3D*)gcpRendD3D->m_RP.m_pCurPass->m_VShader;
				for (uint32 i = 0; i < gcpRendD3D->m_RP.m_RIs[nRE].Num(); i++)
				{
					SRendItem* pRI = gcpRendD3D->m_RP.m_RIs[nRE][i];
					gcpRendD3D->FX_SetObjectTransform(pRI->pObj, NULL, pRI->pObj->m_ObjFlags);
					curVS->mfSetParametersPI(NULL, gcpRendD3D->m_RP.m_pShader);
					gcpRendD3D->FX_Commit();

					gcpRendD3D->FX_DrawPrimitive(eptLineList, 0, numVerts * 6);
				}
			}

			gcpRendD3D->m_RP.m_VertexStreams[0].pStream = NULL;
		}
	}
}

struct SPreprocess
{
	int m_nPreprocess;
	int m_Num;
	CRenderObject* m_pObject;
	int m_nTech;
	CShader* m_Shader;
	CShaderResources* m_pRes;
	CRenderElement* m_RE;
};

struct Compare2
{
	bool operator()(const SPreprocess& a, const SPreprocess& b) const
	{
		return a.m_nPreprocess < b.m_nPreprocess;
	}
};

// Current scene preprocess operations (Rendering to RT, screen effects initializing, ...)
int CD3D9Renderer::EF_Preprocess(SRendItem* ri, uint32 nums, uint32 nume, RenderFunc pRenderFunc, const SRenderingPassInfo& passInfo) PREFAST_SUPPRESS_WARNING(6262)
{
	uint32 i, j;
	CShader* Shader;
	CShaderResources* Res;
	CRenderObject* pObject;
	int nTech;

	SPreprocess Procs[512];
	uint32 nProcs = 0;

	CTimeValue time0 = iTimer->GetAsyncTime();

	if (m_LogFile)
		gRenDev->Logv("*** Start preprocess frame ***\n");

	int DLDFlags = 0;
	int nReturn  = 0;

	for (i = nums; i < nume; i++)
	{
		if (nProcs >= 512)
			break;
		SRendItem::mfGet(ri[i].SortVal, nTech, Shader, Res);
		pObject = ri[i].pObj;
		if (!(ri[i].nBatchFlags & FSPR_MASK))
			break;
		nReturn++;
		if (nTech < 0)
			nTech = 0;
		if (nTech < (int)Shader->m_HWTechniques.Num())
		{
			SShaderTechnique* pTech = Shader->m_HWTechniques[nTech];
			for (j = SPRID_FIRST; j < 32; j++)
			{
				uint32 nMask = 1 << j;
				if (nMask >= FSPR_MAX || nMask > (ri[i].nBatchFlags & FSPR_MASK))
					break;
				if (nMask & ri[i].nBatchFlags)
				{
					Procs[nProcs].m_nPreprocess = j;
					Procs[nProcs].m_Num         = i;
					Procs[nProcs].m_Shader      = Shader;
					Procs[nProcs].m_pRes        = Res;
					Procs[nProcs].m_RE          = ri[i].pElem;
					Procs[nProcs].m_pObject     = pObject;
					Procs[nProcs].m_nTech       = nTech;
					nProcs++;
				}
			}
		}
	}
	if (!nProcs)
		return 0;
	std::sort(&Procs[0], &Procs[nProcs], Compare2());

	if (pRenderFunc != FX_FlushShader_General)
		return nReturn;

	bool bRes = true;
	for (i = 0; i < nProcs; i++)
	{
		SPreprocess* pr = &Procs[i];
		if (!pr->m_Shader)
			continue;
		switch (pr->m_nPreprocess)
		{
		case SPRID_GENSPRITES:
			m_pRT->RC_PreprGenerateFarTrees((CREFarTreeSprites*)pr->m_RE, passInfo);
			break;

		case SPRID_SCANTEX:
		case SPRID_SCANTEXWATER:
			if (!(m_RP.m_TI[m_RP.m_nFillThreadID].m_PersFlags & RBPF_DRAWTOTEXTURE))
			{
				bool bTryPreprocess = false;
				CRenderObject* pObj = pr->m_pObject;
				int nT              = pr->m_nTech;
				if (nT < 0)
					nT = 0;
				SShaderTechnique* pTech = pr->m_Shader->m_HWTechniques[nT];
				CShaderResources* pRes  = pr->m_pRes;
				for (j = 0; j < pTech->m_RTargets.Num(); j++)
				{
					SHRenderTarget* pTarg = pTech->m_RTargets[j];
					if (pTarg->m_eOrder == eRO_PreProcess)
					{
						bTryPreprocess = true;
						bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);
					}
				}
				if (pRes)
				{
					for (j = 0; j < pRes->m_RTargets.Num(); j++)
					{
						SHRenderTarget* pTarg = pRes->m_RTargets[j];
						if (pTarg->m_eOrder == eRO_PreProcess)
						{
							bTryPreprocess = true;
							bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);
						}
					}
				}

				// NOTE: try water reflection pre-process again because it wouldn't be executed when the shader uses only Texture and SamplerState instead of Sampler.
				const uint64 ENVIRONMENT_MAP_MASK = 0x4; // this is defined in Water.ext as %ENVIRONMENT_MAP.
				if (!bTryPreprocess
					&& pr->m_RE->mfGetType() == eDATA_WaterOcean
					&& ((pr->m_Shader->m_nMaskGenFX & ENVIRONMENT_MAP_MASK) == 0))
				{
					CREWaterOcean* pOcean = static_cast<CREWaterOcean*>(pr->m_RE);
					SHRenderTarget* pTarg = pOcean->GetReflectionRenderTarget();
					bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);

#if !defined(RELEASE) && defined(_DEBUG)
					static string ENVIRONMENT_MAP_NAME("%ENVIRONMENT_MAP");
					auto* pShaderGenBase = pr->m_Shader->GetGenerationParams();
					bool findParameter = false;
					if(pShaderGenBase)
					{
						for(unsigned nBaseBit(0); nBaseBit < pShaderGenBase->m_BitMask.size(); ++nBaseBit)
						{
							SShaderGenBit* pBaseBit = pShaderGenBase->m_BitMask[nBaseBit];

							if(!pBaseBit->m_ParamName.empty())
							{
								if(ENVIRONMENT_MAP_NAME == pBaseBit->m_ParamName)
								{
									if (ENVIRONMENT_MAP_MASK == pBaseBit->m_Mask)
									{
										findParameter = true;
										break;
									}
								}
							}
						}
					}
					CRY_ASSERT(findParameter);
#endif
				}
			}
			break;

		case SPRID_CUSTOMTEXTURE:
			if (!(m_RP.m_TI[m_RP.m_nFillThreadID].m_PersFlags & RBPF_DRAWTOTEXTURE))
			{
				CRenderObject* pObj = pr->m_pObject;
				int nT              = pr->m_nTech;
				if (nT < 0)
					nT = 0;
				SShaderTechnique* pTech = pr->m_Shader->m_HWTechniques[nT];
				CShaderResources* pRes  = pr->m_pRes;
				for (j = 0; j < pRes->m_RTargets.Num(); j++)
				{
					SHRenderTarget* pTarg = pRes->m_RTargets[j];
					if (pTarg->m_eOrder == eRO_PreProcess)
						bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);
				}
			}
			break;
		case SPRID_GENCLOUDS:
			break;

		default:
			assert(0);
		}
	}

	if (m_LogFile)
		gRenDev->Logv("*** End preprocess frame ***\n");

	m_RP.m_PS[m_RP.m_nFillThreadID].m_fPreprocessTime += iTimer->GetAsyncTime().GetDifferenceInSeconds(time0);

	return nReturn;
}

void CD3D9Renderer::EF_EndEf2D(const bool bSort)
{
}

//========================================================================================================

bool CRenderer::FX_TryToMerge(CRenderObject* pObjN, CRenderObject* pObjO, CRenderElement* pRE, bool bResIdentical)
{
#if !defined(_RELEASE)
	if (!CV_r_Batching)
		return false;
#endif

	if (!m_RP.m_pRE || pRE->mfGetType() != eDATA_Mesh)
		return false;

#if defined(FEATURE_SVO_GI)
	if (m_RP.m_nPassGroupID == EFSLIST_VOXELIZE)
		return false;
#endif

	// don't batch in thermal mode pass - too expensive to determine if valid batching
	// (need to fetch render resources for heat value)
	if (m_RP.m_PersFlags2 & RBPF2_THERMAL_RENDERMODE_PASS)
		return false;

	if(m_RP.m_PersFlags2 & RBPF2_CUSTOM_RENDER_PASS)
		return false;  

	if (!bResIdentical || pRE != m_RP.m_pRE)
	{
		if (m_RP.m_nLastRE + 1 >= MAX_REND_GEOMS_IN_BATCH)
			return false;
		if ((pObjN->m_ObjFlags ^ pObjO->m_ObjFlags) & FOB_MASK_AFFECTS_MERGING_GEOM)
			return false;
		if ((pObjN->m_ObjFlags | pObjO->m_ObjFlags) & (FOB_SKINNED | FOB_DECAL_TEXGEN_2D | FOB_REQUIRES_RESOLVE | FOB_BLEND_WITH_TERRAIN_COLOR | FOB_DISSOLVE | FOB_LIGHTVOLUME))
			return false;

		if (pObjN->m_nClipVolumeStencilRef != pObjO->m_nClipVolumeStencilRef)
			return false;

		m_RP.m_RIs[++m_RP.m_nLastRE].SetUse(0);
		m_RP.m_pRE = pRE;
		return true;
	}

	// Batching/Instancing case
	if ((pObjN->m_ObjFlags ^ pObjO->m_ObjFlags) & FOB_MASK_AFFECTS_MERGING)
		return false;

	if ((pObjN->m_ObjFlags | pObjO->m_ObjFlags) & (FOB_REQUIRES_RESOLVE | FOB_LIGHTVOLUME))
		return false;

	if (pObjN->m_nMaterialLayers != pObjO->m_nMaterialLayers)
		return false;

	if (pObjN->m_nTextureID != pObjO->m_nTextureID)
		return false;

	if (pObjN->m_nClipVolumeStencilRef != pObjO->m_nClipVolumeStencilRef)
		return false;

	m_RP.m_ObjFlags    |= pObjN->m_ObjFlags & FOB_SELECTED;
	m_RP.m_fMinDistance = min(pObjN->m_fDistance, m_RP.m_fMinDistance);

	return true;
}

// Note: When adding/removing batch flags/techniques, make sure to update sDescList / sBatchList
static char* sDescList[] =
{
	"NULL",
	"Preprocess",
	"General",
	"Z-Prepass",
	"TerrainLayer",
	"ShadowGen",
	"Decal",
	"WaterVolume",
	"Transparent",
	"Water",
	"HDRPostProcess",
	"AfterHDRPostProcess",
	"PostProcess",
	"AfterPostProcess",
	"ShadowPass",
	"DeferredPreprocess",
	"Skin",
	"HalfResParticles",
	"ParticlesThickness",
	"LensOptics",
	"Voxelize",
	"EyeOverlay",
	"FogVolume",
	"Nearest",
	"ForwardOpaque",
	"Custom",
};

static char* sBatchList[] =
{
	"FB_GENERAL",
	"FB_TRANSPARENT",
	"FB_SKIN",
	"FB_Z",
	"FB_ZPREPASS",
	"FB_PREPROCESS",
	"FB_MOTIONBLUR",
	"FB_POST_3D_RENDER",
	"FB_MULTILAYERS",
	"NULL"
	"FB_CUSTOM_RENDER",
	"FB_SOFTALPHATEST",
	"FB_LAYER_EFFECT",
	"FB_WATER_REFL",
	"FB_WATER_CAUSTIC",
	"FB_DEBUG",
	"FB_PARTICLES_THICKNESS",
	"FB_EYE_OVERLAY"
};

// Init states before rendering of the scene
void CD3D9Renderer::FX_PreRender(int Stage)
{
	uint32 i;

	if (Stage & 1)
	{
		// Before preprocess
		m_RP.m_pSunLight = NULL;

		m_RP.m_Flags       = 0;
		m_RP.m_pPrevObject = NULL;

		RT_SetCameraInfo();

		int numLights = m_RP.RenderView()->GetDynamicLightsCount();
		for (i = 0; i < numLights; i++)
		{
			SRenderLight* dl = &m_RP.RenderView()->GetDynamicLight(i);
			if (dl->m_Flags & DLF_FAKE)
				continue;

			if (dl->m_Flags & DLF_SUN)
				m_RP.m_pSunLight = dl;
		}
	}

	CHWShader_D3D::mfSetGlobalParams();
	m_RP.m_nCommitFlags = FC_ALL;
	FX_PushVP();
}

// Restore states after rendering of the scene
void CD3D9Renderer::FX_PostRender()
{
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	//FrameProfiler f("CD3D9Renderer:EF_PostRender", iSystem );
	FX_ObjectChange(NULL, NULL, m_RP.m_pIdendityRenderObject, NULL);
	m_RP.m_pRE = NULL;

	FX_ResetPipe();
	FX_PopVP();

	m_RP.m_nCurrResolveBounds[0] = m_RP.m_nCurrResolveBounds[1] = m_RP.m_nCurrResolveBounds[2] = m_RP.m_nCurrResolveBounds[3] = 0;
	m_RP.m_FlagsShader_MD        = 0;
	m_RP.m_FlagsShader_MDV       = 0;
	m_RP.m_FlagsShader_LT        = 0;
	m_RP.m_pCurObject            = m_RP.m_pIdendityRenderObject;

	pShaderThreadInfo->m_PersFlags |= RBPF_FP_DIRTY;
	m_RP.m_nCommitFlags             = FC_ALL;
}

// Object changing handling (skinning, shadow maps updating, initial states setting, ...)
bool CD3D9Renderer::FX_ObjectChange(CShader* Shader, CShaderResources* Res, CRenderObject* obj, CRenderElement* pRE)
{
	FUNCTION_PROFILER_RENDER_FLAT

	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	if ((obj->m_ObjFlags & FOB_NEAREST) && CV_r_nodrawnear)
		return false;

	if (Shader)
	{
		if (pShaderThreadInfo->m_pIgnoreObject && pShaderThreadInfo->m_pIgnoreObject->m_pRenderNode == obj->m_pRenderNode)
			return false;
	}

	if (obj == m_RP.m_pPrevObject)
		return true;

	if (CRenderer::CV_r_RefractionPartialResolves == 2)
	{
		if ((m_RP.m_pCurObject == NULL) || obj->m_pRenderNode == NULL ||
		  ((obj->m_pRenderNode != m_RP.m_pCurObject->m_pRenderNode) &&
		  !((obj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) && (obj->m_nMaterialLayers == m_RP.m_pCurObject->m_nMaterialLayers)))
		  )
		{
			m_RP.m_nCurrResolveBounds[0] = m_RP.m_nCurrResolveBounds[1] = m_RP.m_nCurrResolveBounds[2] = m_RP.m_nCurrResolveBounds[3] = 0;
		}
	}

	m_RP.m_pCurObject = obj;

	int flags = 0;
	if (obj != m_RP.m_pIdendityRenderObject) // Non-default object
	{
		if (obj->m_ObjFlags & FOB_NEAREST)
			flags |= RBF_NEAREST;

		if ((flags ^ m_RP.m_Flags) & RBF_NEAREST)
		{
			UpdateNearestChange(flags);
		}
	}
	else
	{
		HandleDefaultObject();
	}

	const uint32 nPerfFlagsExcludeMask  = RBPF_ZPASS;
	const uint32 nPerfFlags2ExcludeMask = (RBPF2_THERMAL_RENDERMODE_PASS | RBPF2_MOTIONBLURPASS | RBPF2_THERMAL_RENDERMODE_TRANSPARENT_PASS | RBPF2_CUSTOM_RENDER_PASS);

	if ((m_RP.m_nPassGroupID == EFSLIST_TRANSP)
	  && (obj->m_ObjFlags & FOB_REQUIRES_RESOLVE)
	  // for cloak, in transition the illum objects are rendered before cloak but with same render object.
	  // Don't refract until the multilayers pass to get correct blend order
	  && ((obj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) == 0 || m_RP.m_nBatchFilter == FB_MULTILAYERS)
	  && !(pShaderThreadInfo->m_PersFlags & nPerfFlagsExcludeMask)
	  && !(m_RP.m_PersFlags2 & nPerfFlags2ExcludeMask))
	{
		if (CRenderer::CV_r_RefractionPartialResolves)
		{
			if (!IsRecursiveRenderView())
				gcpRendD3D->FX_RefractionPartialResolve();
		}
	}

	m_RP.m_fMinDistance   = obj->m_fDistance;
	m_RP.m_pPrevObject    = obj;
	m_RP.m_CurPassBitMask = 0;

	return true;
}

void CD3D9Renderer::UpdateNearestChange(int flags)
{
	const int nProcessThread             = m_RP.m_nProcessThreadID;
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[nProcessThread]);

	if (m_drawNearFov > 0.0f)
	{
		if (flags & RBF_NEAREST)
		{
			CCamera Cam = pShaderThreadInfo->m_cam;
			m_RP.m_PrevCamera = Cam;
			if (m_LogFile)
				Logv("*** Prepare nearest Z range ***\n");
			// set nice fov for weapons

			float fFov = Cam.GetFov();
			if (m_drawNearFov > 1.0f && m_drawNearFov < 179.0f)
				fFov = DEG2RAD(m_drawNearFov);

			float fNearRatio = DRAW_NEAREST_MIN / Cam.GetNearPlane();
			Cam.SetAsymmetry(Cam.GetAsymL() * fNearRatio, Cam.GetAsymR() * fNearRatio, Cam.GetAsymB() * fNearRatio, Cam.GetAsymT() * fNearRatio);
			Cam.SetFrustum(Cam.GetViewSurfaceX(), Cam.GetViewSurfaceZ(), fFov, DRAW_NEAREST_MIN, CV_r_DrawNearFarPlane, Cam.GetPixelAspectRatio());

			SetCamera(Cam);
			m_NewViewport.fMaxZ = CV_r_DrawNearZRange;
			m_RP.m_Flags       |= RBF_NEAREST;
		}
		else
		{
			if (m_LogFile)
				Logv("*** Restore Z range ***\n");

			SetCamera(m_RP.m_PrevCamera);
			m_NewViewport.fMaxZ = m_RP.m_PrevCamera.GetZRangeMax();
			m_RP.m_Flags       &= ~RBF_NEAREST;
		}

		m_bViewportDirty = true;
	}
	m_RP.m_nCurrResolveBounds[0] = m_RP.m_nCurrResolveBounds[1] = m_RP.m_nCurrResolveBounds[2] = m_RP.m_nCurrResolveBounds[3] = 0;
}

void CD3D9Renderer::HandleDefaultObject()
{
	if (m_RP.m_Flags & (RBF_NEAREST))
	{
		if (m_LogFile)
			Logv("*** Restore Z range/camera ***\n");
		SetCamera(m_RP.m_PrevCamera);
		m_NewViewport.fMaxZ = 1.0f;
		m_bViewportDirty    = true;
		m_RP.m_Flags       &= ~(RBF_NEAREST);
	}
	m_ViewMatrix = m_CameraMatrix;
	// Restore transform
	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);
	pShaderThreadInfo->m_matView->LoadMatrix(&m_CameraMatrix);
	pShaderThreadInfo->m_PersFlags |= RBPF_FP_MATRIXDIRTY;
}

void CD3D9Renderer::UpdatePrevMatrix(bool bEnable)
{
	static int nLastUpdate         = -1;
	static Matrix44A arrAccumPM[4] =
	{
		Matrix44A(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		Matrix44A(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		Matrix44A(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		Matrix44A(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
	};
	static int nAccumIdx = 0;

	if (bEnable)
	{
		const int nAvgSamples = clamp_tpl(int(gEnv->pTimer->GetFrameRate() * 0.05f), 1, 4);

		if (nLastUpdate != GetFrameID())
		{
			nLastUpdate           = GetFrameID();
			nAccumIdx             = (nAccumIdx + 1) % nAvgSamples;
			arrAccumPM[nAccumIdx] = m_CameraProjMatrix + (m_CameraProjMatrixPrev * -1.f);
		}

		m_CameraProjMatrixPrevAvg = arrAccumPM[0];
		for (int i = 1; i < nAvgSamples; ++i)
			m_CameraProjMatrixPrevAvg = m_CameraProjMatrixPrevAvg + arrAccumPM[i];
		m_CameraProjMatrixPrevAvg = m_CameraProjMatrix + m_CameraProjMatrixPrevAvg * (-1.f / nAvgSamples);
	}
	else
		m_CameraProjMatrixPrevAvg = m_CameraProjMatrix;
}

//=================================================================================
// Check buffer overflow during geometry batching
void CRenderer::FX_CheckOverflow(int nVerts, int nInds, CRenderElement* re, int* nNewVerts, int* nNewInds)
{
	if (nNewVerts)
		*nNewVerts = nVerts;
	if (nNewInds)
		*nNewInds = nInds;

	if (m_RP.m_pRE || (m_RP.m_RendNumVerts + nVerts >= m_RP.m_MaxVerts || m_RP.m_RendNumIndices + nInds >= m_RP.m_MaxTris * 3))
	{
		m_RP.m_pRenderFunc();
		if (nVerts >= m_RP.m_MaxVerts)
		{
			// iLog->Log("CD3D9Renderer::EF_CheckOverflow: numVerts >= MAX (%d > %d)\n", nVerts, m_RP.m_MaxVerts);
			assert(nNewVerts);
			*nNewVerts = m_RP.m_MaxVerts;
		}
		if (nInds >= m_RP.m_MaxTris * 3)
		{
			// iLog->Log("CD3D9Renderer::EF_CheckOverflow: numIndices >= MAX (%d > %d)\n", nInds, m_RP.m_MaxTris*3);
			assert(nNewInds);
			*nNewInds = m_RP.m_MaxTris * 3;
		}
		FX_Start(m_RP.m_pShader, m_RP.m_nShaderTechnique, m_RP.m_pShaderResources, re);
		FX_StartMerging();
	}
}

// Start of the new shader pipeline (3D pipeline version)
void CRenderer::FX_Start(CShader* ef, int nTech, CShaderResources* Res, CRenderElement* re)
{
	FUNCTION_PROFILER_RENDER_FLAT
	  assert(ef);

	PrefetchLine(&m_RP.m_pCurObject, 64);
	PrefetchLine(&m_RP.m_Frame, 0);

	if (!ef)     // should not be 0, check to prevent crash
		return;

	PrefetchLine(&ef->m_eVertexFormat, 0);

	m_RP.m_nNumRendPasses       = 0;
	m_RP.m_FirstIndex           = 0;
	m_RP.m_FirstVertex          = 0;
	m_RP.m_RendNumIndices       = 0;
	m_RP.m_RendNumVerts         = 0;
	m_RP.m_RendNumGroup         = -1;
	m_RP.m_pShader              = ef;
	m_RP.m_nShaderTechnique     = nTech;
	m_RP.m_nShaderTechniqueType = -1;
	m_RP.m_pShaderResources     = Res;
	m_RP.m_FlagsPerFlush        = 0;

	m_RP.m_FlagsStreams_Decl   = 0;
	m_RP.m_FlagsStreams_Stream = 0;
	m_RP.m_FlagsShader_RT      = 0;
	m_RP.m_FlagsShader_MD      = 0;
	m_RP.m_FlagsShader_MDV     = 0;

	const uint64 hdrMode = g_HWSR_MaskBit[HWSR_HDR_MODE];
	const uint64 sample0 = g_HWSR_MaskBit[HWSR_SAMPLE0];
	const uint64 sample1 = g_HWSR_MaskBit[HWSR_SAMPLE1];
	const uint64 sample4 = g_HWSR_MaskBit[HWSR_SAMPLE4];
	const uint64 tiled   = g_HWSR_MaskBit[HWSR_TILED_SHADING];

	FX_ApplyShaderQuality(ef->m_eShaderType);

	FX_SetMSAAFlagsRT();

	const uint32 nPersFlags2 = m_RP.m_PersFlags2;
	if ((nPersFlags2 & RBPF2_HDR_FP16) && !(m_RP.m_nBatchFilter & (FB_Z)))
		m_RP.m_FlagsShader_RT |= hdrMode;   // deprecated: redundant flag, will be dropped (rendering always HDR)

	static const uint32 nPFlags2Mask = (RBPF2_THERMAL_RENDERMODE_PASS | RBPF2_SKIN);
	if (nPersFlags2 & nPFlags2Mask)
	{
		if (nPersFlags2 & RBPF2_THERMAL_RENDERMODE_PASS)
			m_RP.m_FlagsShader_RT |= sample1;

		if (nPersFlags2 & RBPF2_SKIN)
			m_RP.m_FlagsShader_RT |= sample0;
	}

	// Set shader flag for tiled forward shading
	if (CV_r_DeferredShadingTiled > 0)
		m_RP.m_FlagsShader_RT |= tiled;

	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);
	if (pShaderThreadInfo->m_PersFlags & RBPF_REVERSE_DEPTH)
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	m_RP.m_fCurOpacity = 1.0f;
	m_RP.m_CurVFormat  = ef->m_eVertexFormat;
	m_RP.m_ObjFlags    = m_RP.m_pCurObject->m_ObjFlags;
	m_RP.m_RIs[0].SetUse(0);
	m_RP.m_nLastRE = 0;

	m_RP.m_pRE = NULL;
	m_RP.m_Frame++;
}

//==============================================================================================

static void sBatchFilter(uint32 nFilter, char* sFilt)
{
	static_assert((1 << ((CRY_ARRAY_COUNT(sBatchList)) - 1) <= FB_MASK), "Batch techniques/flags list mismatch");

	sFilt[0] = 0;
	int n = 0;
	for (int i = 0; i < CRY_ARRAY_COUNT(sBatchList); i++)
	{
		if (nFilter & (1 << i))
		{
			if (n)
				strcat(sFilt, "|");
			strcat(sFilt, sBatchList[i]);
			n++;
		}
	}
}

void CD3D9Renderer::FX_StartBatching()
{
	m_RP.m_nCommitFlags = FC_ALL;
}

void CD3D9Renderer::OldPipeline_ProcessBatchesList(CRenderView::RenderItems& renderItems, int nums, int nume, uint32 nBatchFilter, uint32 nBatchExcludeFilter)
{
	PROFILE_FRAME(ProcessBatchesList);

	if (nume - nums == 0)
		return;

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	int nList     = rRP.m_nPassGroupID;
	int nThreadID = rRP.m_nProcessThreadID;

	FX_StartBatching();

	auto& RESTRICT_REFERENCE RI = renderItems;
	assert(nums < RI.size());
	assert(nume <= RI.size());

	SRendItem* pPrefetchPlainPtr = &RI[0];

	rRP.m_nBatchFilter = nBatchFilter;

	// make sure all all jobs which are computing particle vertices/indices
	// have finished and their vertex/index buffers are unlocked
	// before starting rendering of those
	if (rRP.m_nPassGroupID == EFSLIST_TRANSP || rRP.m_nPassGroupID == EFSLIST_HALFRES_PARTICLES || rRP.m_nPassGroupID == EFSLIST_PARTICLES_THICKNESS)
	{
		// [GPU Particles Note]  - last chance to do the gpu particles update, but maybe there is a better place
		SyncComputeVerticesJobs();
		UnLockParticleVideoMemory();
	}

#ifdef DO_RENDERLOG
	static_assert(((CRY_ARRAY_COUNT(sDescList)) == EFSLIST_NUM), "Batch techniques/flags list mismatch");

	if (CV_r_log)
	{
		char sFilt[256];
		sBatchFilter(nBatchFilter, sFilt);
		Logv("\n*** Start batch list %s (Filter: %s) ***\n", sDescList[nList], sFilt);
	}
#endif

	uint32 prevSortVal        = -1;
	CShader* pShader          = NULL;
	CShaderResources* pCurRes = NULL;
	CRenderObject* pCurObject = NULL;
	CShader* pCurShader       = NULL;
	int nTech;

	for (int i = nums; i < nume; i++)
	{
		SRendItem& ri = RI[i];
		if (!(ri.nBatchFlags & nBatchFilter))
			continue;

		if (ri.nBatchFlags & nBatchExcludeFilter)
			continue;

		CRenderObject* pObject = ri.pObj;
		CRenderElement* pRE  = ri.pElem;
		bool bChangedShader    = false;
		bool bResIdentical     = true;
		if (prevSortVal != ri.SortVal)
		{
			CShaderResources* pRes;
			SRendItem::mfGet(ri.SortVal, nTech, pShader, pRes);
			if (pShader != pCurShader || !pRes || !pCurRes || pRes->m_IdGroup != pCurRes->m_IdGroup || (pObject->m_ObjFlags & (FOB_SKINNED | FOB_DECAL)))                  // Additional check for materials batching
				bChangedShader = true;
			bResIdentical = (pRes == pCurRes);
			pCurRes       = pRes;
			prevSortVal   = ri.SortVal;
		}
		if (!bChangedShader && FX_TryToMerge(pObject, pCurObject, pRE, bResIdentical))
		{
			rRP.m_RIs[rRP.m_nLastRE].AddElem(&ri);
			continue;
		}
		// when not doing main pass rendering, need to flush the shader for each data part since the external VMEM buffers are laied out only for the main pass
		if (pObject && pObject != pCurObject)
		{
			if (pCurShader)
			{
				rRP.m_pRenderFunc();
				pCurShader     = NULL;
				bChangedShader = true;
			}
			if (!FX_ObjectChange(pShader, pCurRes, pObject, pRE))
			{
				prevSortVal = ~0;
				continue;
			}
			pCurObject = pObject;
		}

		if (bChangedShader)
		{
			if (pCurShader)
			{
				rRP.m_pRenderFunc();
			}

			pCurShader = pShader;
			FX_Start(pShader, nTech, pCurRes, pRE);
		}

		pRE->mfPrepare(true);

		if (rRP.m_RIs[0].size() == 0)
			rRP.m_RIs[0].AddElem(&ri);
	}
	if (pCurShader)
		rRP.m_pRenderFunc();

#ifdef DO_RENDERLOG
	if (CV_r_log)
		Logv("*** End batch list ***\n\n");
#endif
}

void CD3D9Renderer::DrawRenderItems(const SGraphicsPipelinePassContext& passContext)
{
	PROFILE_FRAME(DrawRenderItems);

	if (CRenderer::CV_r_NoDraw == 1)
	{
		// Skip drawing objects
		return;
	}

	if (passContext.rendItems.IsEmpty())
		return;

	// This can be multi-threaded
	passContext.pRenderView->DrawCompiledRenderItems(passContext);
}

void CD3D9Renderer::OldPipeline_ProcessRenderList(CRenderView::RenderItems& renderItems, int nums, int nume, int nList, void (* RenderFunc)(), bool bLighting, uint32 nBatchFilter, uint32 nBatchExcludeFilter)
{
	if (nums < 0 && nume < 0)
	{
		nums = 0;
		nume = renderItems.size();
	}

	if (nume - nums < 1)
		return;

	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	const bool bTranspPass = (nList == EFSLIST_TRANSP) || (nList == EFSLIST_HALFRES_PARTICLES);
	if (bTranspPass && !CV_r_TransparentPasses)
		return;

	EF_PushMatrix();
	pShaderThreadInfo->m_matProj->Push();

	void (* pPrevRenderFunc)();

	// Remember current render function
	pPrevRenderFunc    = m_RP.m_pRenderFunc;
	m_RP.m_pRenderFunc = RenderFunc;

	m_RP.m_pCurObject  = m_RP.m_pIdendityRenderObject;
	m_RP.m_pPrevObject = m_RP.m_pCurObject;

	FX_PreRender(3);

	int nPrevGroup  = m_RP.m_nPassGroupID;
	int nPrevGroup2 = m_RP.m_nPassGroupDIP;

	m_RP.m_nPassGroupID  = nList;
	m_RP.m_nPassGroupDIP = nList;

#ifdef SUPPORTS_MSAA
	if ((m_RP.m_PersFlags2 & RBPF2_MSAA_STENCILCULL) && CV_r_msaa_debug != 1)
		m_RP.m_ForceStateOr |= GS_STENCIL;
	else
		m_RP.m_ForceStateOr &= ~GS_STENCIL;
#endif

	OldPipeline_ProcessBatchesList(renderItems, nums, nume, nBatchFilter, nBatchExcludeFilter);

	if (bLighting)
	{
		FX_ProcessPostGroups(nums, nume);
	}

	FX_PostRender();

	// Restore previous render function
	m_RP.m_pRenderFunc = pPrevRenderFunc;

	EF_PopMatrix();
	pShaderThreadInfo->m_matProj->Pop();

	m_RP.m_nPassGroupID  = nPrevGroup;
	m_RP.m_nPassGroupDIP = nPrevGroup2;
}

void CD3D9Renderer::FX_ProcessRenderList(int nList, uint32 nBatchFilter)
{
	auto& renderItems = m_RP.m_pCurrentRenderView->GetRenderItems(nList);
	OldPipeline_ProcessRenderList(renderItems, -1, -1, nList, m_RP.m_pRenderFunc, false, nBatchFilter);
}

void CD3D9Renderer::FX_ProcessRenderList(int nList, void (* RenderFunc)(), bool bLighting, uint32 nBatchFilter, uint32 nBatchExcludeFilter)
{
	auto& renderItems = m_RP.m_pCurrentRenderView->GetRenderItems(nList);
	OldPipeline_ProcessRenderList(renderItems, -1, -1, nList, RenderFunc, bLighting, nBatchFilter, nBatchExcludeFilter);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::FX_ProcessCharDeformation(CRenderView* pRenderView)
{
	if (pRenderView->IsRecursive())
		return;

	GetGraphicsPipeline().GetComputeSkinningStage()->Execute(pRenderView);
}

void CD3D9Renderer::FX_ProcessZPassRender_List(ERenderListID list, uint32 filter = FB_Z)
{
	auto& renderItems = m_RP.m_pCurrentRenderView->GetRenderItems(list);
	OldPipeline_ProcessRenderList(renderItems, -1, -1, list, m_RP.m_pRenderFunc, false, filter);
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::FX_ProcessZPassRenderLists()
{
	//PROFILE_LABEL_SCOPE("ZPASS");

	if (IsRecursiveRenderView())  // Do not use GBuffer for recursive views
		return;

	uint32 bfGeneral      = SRendItem::BatchFlags(EFSLIST_GENERAL);
	uint32 bfSkin         = SRendItem::BatchFlags(EFSLIST_SKIN);
	uint32 bfTransp       = SRendItem::BatchFlags(EFSLIST_TRANSP);
	uint32 bfDecal        = SRendItem::BatchFlags(EFSLIST_DECAL);
	uint32 bfTerrainLayer = SRendItem::BatchFlags(EFSLIST_TERRAINLAYER);
	bfTerrainLayer |= FB_Z;
	bfGeneral      |= FB_Z;

	if ((bfGeneral | bfSkin | bfTransp | bfDecal | bfTerrainLayer) & FB_Z)
	{
#ifdef DO_RENDERLOG
		if (CV_r_log)
			Logv("*** Start z-pass ***\n");
#endif

		const int nWidth  = m_MainViewport.nWidth;
		const int nHeight = m_MainViewport.nHeight;
		RECT rect         = { 0, 0, nWidth, nHeight };
		if (!CTexture::s_ptexZTarget
		  || CTexture::s_ptexZTarget->IsMSAAChanged()
		  || CTexture::s_ptexZTarget->GetDstFormat() != CTexture::s_eTFZ
		  || CTexture::s_ptexZTarget->GetWidth() != nWidth
		  || CTexture::s_ptexZTarget->GetHeight() != nHeight)
		{
			FX_Commit(); // Flush to unset the Z target before regenerating
			CTexture::GenerateZMaps();
		}

		const float depthClearValue   = (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 0.f : 1.f;
		const uint  stencilClearValue = 1;
		// Stencil initialized to 1 - 0 is reserved for MSAAed samples
		FX_ClearTarget(&m_DepthBufferOrigMSAA, CLEAR_ZBUFFER | CLEAR_STENCIL, depthClearValue, stencilClearValue, 1, &rect, true);
		m_nStencilMaskRef = 1;

		if (CV_r_MotionVectors && CV_r_MotionBlurGBufferVelocity)
		{
			FX_ClearTarget(GetUtils().GetVelocityObjectRT(), Clr_Transparent, 1, &rect, true);
		}
		if (CV_r_wireframe != 0)
		{
			FX_ClearTarget(CTexture::s_ptexSceneNormalsMap, Clr_Transparent, 1, &rect, true);
			FX_ClearTarget(CTexture::s_ptexSceneDiffuse, Clr_Empty, 1, &rect, true);
#if defined(DURANGO_USE_ESRAM)
			FX_ClearTarget(CTexture::s_ptexSceneSpecularESRAM, Clr_Empty, 1, &rect, true);
#else
			FX_ClearTarget(CTexture::s_ptexSceneSpecular, Clr_Empty, 1, &rect, true);
#endif
		}

		FX_PreRender(3);

		m_RP.m_pRenderFunc = FX_FlushShader_ZPass;

		bool bClearZBuffer = true;

		{
			PROFILE_LABEL_SCOPE("GBUFFER");

			if (CRenderer::CV_r_usezpass == 2)
			{
				PROFILE_LABEL_SCOPE("ZPREPASS");

				if (bfGeneral & FB_ZPREPASS)
				{
					FX_ZScene(true, m_RP.m_bUseHDR, bClearZBuffer, false, true);

					FX_ProcessZPassRender_List(EFSLIST_GENERAL, FB_ZPREPASS);

					FX_ZScene(false, m_RP.m_bUseHDR, false, false, true);
					bClearZBuffer = false;
				}
			}

			FX_ZScene(true, m_RP.m_bUseHDR, bClearZBuffer);

			{
				PROFILE_LABEL_SCOPE("OPAQUE");
				if (bfGeneral & FB_Z) FX_ProcessZPassRender_List(EFSLIST_GENERAL);
				if (bfSkin & FB_Z) FX_ProcessZPassRender_List(EFSLIST_SKIN);
				if (bfTransp & FB_Z) FX_ProcessZPassRender_List(EFSLIST_TRANSP);
			}

			// PC special case: render terrain/decals/roads normals separately - disable mrt rendering, on consoles we always use single rt for output
			FX_ZScene(false, m_RP.m_bUseHDR, false);
			FX_ZScene(true, m_RP.m_bUseHDR, false, true);

			m_RP.m_PersFlags2 &= ~RBPF2_NOALPHABLEND;
			m_RP.m_StateAnd   |= GS_BLEND_MASK;

			// Add terrain/roads/decals normals into normal render target also
			if (bfTerrainLayer & FB_Z)
			{
				PROFILE_LABEL_SCOPE("TERRAIN_LAYERS");
				FX_ProcessZPassRender_List(EFSLIST_TERRAINLAYER);
			}
			if (bfDecal & FB_Z)
			{
				PROFILE_LABEL_SCOPE("DECALS");
				FX_ProcessZPassRender_List(EFSLIST_DECAL);
			}

			FX_ZScene(false, m_RP.m_bUseHDR, false, true);
		}

		// Reset current object so we don't end up with RBF_NEAREST states in FX_LinearizeDepth
		FX_ObjectChange(NULL, NULL, m_RP.m_pIdendityRenderObject, NULL);

		FX_LinearizeDepth();
#if CRY_PLATFORM_DURANGO
		GetUtils().DownsampleDepth(NULL, CTexture::s_ptexZTargetScaled, true);    // On Durango reading device depth is faster since it is in ESRAM
#else
		GetUtils().DownsampleDepth(CTexture::s_ptexZTarget, CTexture::s_ptexZTargetScaled, true);
#endif
		GetUtils().DownsampleDepth(CTexture::s_ptexZTargetScaled, CTexture::s_ptexZTargetScaled2, false);

		FX_ZScene(true, m_RP.m_bUseHDR, false, true);
		m_RP.m_PersFlags2 &= ~RBPF2_NOALPHABLEND;
		m_RP.m_StateAnd   |= GS_BLEND_MASK;

		FX_PostRender();
		RT_SetViewport(0, 0, GetWidth(), GetHeight());

		if (m_RP.m_PersFlags2 & RBPF2_ALLOW_DEFERREDSHADING)
			m_bDeferredDecals = FX_DeferredDecals();

		m_RP.m_PersFlags2 |= RBPF2_NOALPHABLEND;
		m_RP.m_StateAnd   &= ~GS_BLEND_MASK;

		FX_ZScene(false, m_RP.m_bUseHDR, false, true);

		FX_ZTargetReadBack();

		m_RP.m_pRenderFunc = FX_FlushShader_General;

#ifdef DO_RENDERLOG
		if (CV_r_log)
			Logv("*** End z-pass ***\n");
#endif
	}
}

//////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::FX_ProcessSoftAlphaTestRenderLists()
{
	if (IsRecursiveRenderView())
		return;

	int32 nList = EFSLIST_GENERAL;

	if ((m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS))
	{
#ifdef DO_RENDERLOG
		if (CV_r_log)
			Logv("*** Begin soft alpha test pass ***\n");
#endif

		uint32 nBatchMask = SRendItem::BatchFlags(nList);
		if (nBatchMask & FB_SOFTALPHATEST)
		{
			m_RP.m_PersFlags2 |= RBPF2_NOALPHATEST;

			FX_ProcessRenderList(nList, FB_SOFTALPHATEST);

			m_RP.m_PersFlags2 &= ~RBPF2_NOALPHATEST;
		}

#ifdef DO_RENDERLOG
		if (CV_r_log)
			Logv("*** End soft alpha test pass ***\n");
#endif
	}
}

void CD3D9Renderer::FX_ProcessPostRenderLists(uint32 nBatchFilter)
{
	if (IsRecursiveRenderView())
		return;

	if ((m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS))
	{
		int nList         = EFSLIST_GENERAL;
		uint32 nBatchMask = SRendItem::BatchFlags(EFSLIST_GENERAL) | SRendItem::BatchFlags(EFSLIST_TRANSP);
		nBatchMask |= SRendItem::BatchFlags(EFSLIST_DECAL);
		nBatchMask |= SRendItem::BatchFlags(EFSLIST_SKIN);
		if (nBatchMask & nBatchFilter)
		{
			if (nBatchFilter == FB_CUSTOM_RENDER || nBatchFilter == FB_POST_3D_RENDER)
			{
				FX_CustomRenderScene(true);
			}

			FX_ProcessRenderList(EFSLIST_GENERAL, nBatchFilter);
			FX_ProcessRenderList(EFSLIST_SKIN, nBatchFilter);

			if (nBatchFilter != FB_MOTIONBLUR)
				FX_ProcessRenderList(EFSLIST_DECAL, nBatchFilter);

			FX_ProcessRenderList(EFSLIST_TRANSP, nBatchFilter);

			if (nBatchFilter == FB_CUSTOM_RENDER || nBatchFilter == FB_POST_3D_RENDER)
			{
				FX_CustomRenderScene(false);
			}
		}
	}
}

void CD3D9Renderer::FX_ProcessPostGroups(int nums, int nume)
{
	const uint32 nPrevPersFlags2 = m_RP.m_PersFlags2;
	m_RP.m_PersFlags2 &= ~RBPF2_FORWARD_SHADING_PASS;

	uint32 nBatchMask = SRendItem::BatchFlags(m_RP.m_nPassGroupID);
	int nRenderList   = m_RP.m_nPassGroupID;

	auto& renderItems = m_RP.m_pCurrentRenderView->GetRenderItems(nRenderList);

	if (nBatchMask & FB_MULTILAYERS && CV_r_usemateriallayers)
		OldPipeline_ProcessBatchesList(renderItems, nums, nume, FB_MULTILAYERS);
	if ((nBatchMask & FB_LAYER_EFFECT) && CV_r_detailtextures)
		OldPipeline_ProcessBatchesList(renderItems, nums, nume, FB_LAYER_EFFECT);
	if (0 != (nBatchMask & FB_DEBUG))
		OldPipeline_ProcessBatchesList(renderItems, nums, nume, FB_DEBUG);

	m_RP.m_PersFlags2 = nPrevPersFlags2;
}

void CD3D9Renderer::FX_ApplyThreadState(SThreadInfo& TI, SThreadInfo* pOldTI)
{
	if (pOldTI)
		*pOldTI = m_RP.m_TI[m_RP.m_nProcessThreadID];

	m_RP.m_TI[m_RP.m_nProcessThreadID] = TI;
}

int CD3D9Renderer::GetOcclusionBuffer(uint16* pOutOcclBuffer, int32 nSizeX, int32 nSizeY, Matrix44* pmViewProj, Matrix44* pmCamBuffer)
{
	m_occlusionRequestedSizeX = nSizeX;
	m_occlusionRequestedSizeY = nSizeY;
	if (nSizeX != m_occlusionDownsampleSizeX || nSizeY != m_occlusionDownsampleSizeY)
		return 0;//not ready

	if (m_occlusionBuffer < 4)
		return 0;

	const bool bUseNativeDepth = CRenderer::CV_r_CBufferUseNativeDepth && !gEnv->IsEditor();
#if CRY_PLATFORM_DURANGO
	if (CV_r_ReadZBufferDirectlyFromVMEM != 0)
	{
		// sync fence and read directly from GPU memory
		gRenDev->m_DevMan.SyncFence(m_occlusionFence[m_RP.m_nFillThreadID], true);
		float* pData = reinterpret_cast<float*>(m_occlusionGPUData[m_RP.m_nFillThreadID]);
		if (pData == NULL)
			return 0;
		int nPitch = m_occlusionDataPitch;
		const float zn = m_occlusionZNear[m_RP.m_nFillThreadID];
		const float zf = m_occlusionZFar[m_RP.m_nFillThreadID];
		int nCameraID = -1;
		const float ProjRatioX = zf / (zf - zn);
		const float ProjRatioY = zn / (zn - zf);

		uint32 nBufferSize = m_occlusionDownsampleSizeY * m_occlusionDownsampleSizeX;
		if (bUseNativeDepth)
		{
			float x = floorf(pData[0] * 0.5f);      // Decode the ID from the first pixel
			m_occlusionZBuffer[0] = pData[0] - (x * 2.0f);
			nCameraID             = (int)(x);

			for (uint32 x = 1; x < nBufferSize; x++)
			{
				reinterpret_cast<float*>(pOutOcclBuffer)[x] = max(pData[x], FLT_EPSILON);
			}
		}
		else
		{
			for (uint32 x = 0; x < nBufferSize; x++)
			{
				reinterpret_cast<float*>(pOutOcclBuffer)[x] = max(ProjRatioY / max(pData[x], FLT_EPSILON) + ProjRatioX, FLT_EPSILON);
			}
		}

		if (bUseNativeDepth)
		{
			nCameraID           = max((int)0, min(nCameraID, (int)(CULLER_MAX_CAMS - 1)));
			m_occlusionViewProj = m_RP.m_OcclusionCameraBuffer[nCameraID];
		}
		*pmCamBuffer = m_occlusionViewProjBuffer[m_RP.m_nFillThreadID];
	}
	else
#endif
	{
		// use the data prepared by the renderthread (with 1 frame latency)
		for (size_t a = 0, S = nSizeX * nSizeY; a < S; a++)
			reinterpret_cast<float*>(pOutOcclBuffer)[a] = m_occlusionZBuffer[a];

		*pmCamBuffer = m_occlusionViewProj;
	}

	*pmViewProj = m_RP.m_newOcclusionCameraView * m_RP.m_newOcclusionCameraProj;
	return 1;
}

void CD3D9Renderer::FX_ZTargetReadBack()
{
	PROFILE_LABEL_SCOPE("DEPTH READBACK");
	PROFILE_FRAME(FX_ZTargetReadBack);

	static ICVar* pCVCheckOcclusion           = gEnv->pConsole->GetCVar("e_CheckOcclusion");
	static ICVar* pCVStatObjBufferRenderTasks = gEnv->pConsole->GetCVar("e_StatObjBufferRenderTasks");
	static ICVar* pCVCoverageBufferReproj     = gEnv->pConsole->GetCVar("e_CoverageBufferReproj");
	if ((pCVCheckOcclusion && pCVCheckOcclusion->GetIVal() == 0) ||
	  (pCVStatObjBufferRenderTasks && pCVStatObjBufferRenderTasks->GetIVal() == 0) ||
	  (pCVCoverageBufferReproj && pCVCoverageBufferReproj->GetIVal() == 4))
	{
		return;
	}

	const bool bUseNativeDepth = (CRenderer::CV_r_CBufferUseNativeDepth || CVrProjectionManager::IsMultiResEnabledStatic()) && !gEnv->IsEditor();
	const bool bReverseDepth   = (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

	bool bDownSampleUpdate = false;

	int sourceWidth  = CTexture::s_ptexZTarget->GetWidth();
	int sourceHeight = CTexture::s_ptexZTarget->GetHeight();

	if ((m_occlusionDownsampleSizeX && m_occlusionDownsampleSizeY) &&
	  (sourceWidth != m_occlusionSourceSizeX || sourceHeight != m_occlusionSourceSizeY))
	{
		bDownSampleUpdate = true;
	}

	if (m_occlusionRequestedSizeX != m_occlusionDownsampleSizeX ||
	  m_occlusionRequestedSizeY != m_occlusionDownsampleSizeY ||
	  bDownSampleUpdate ||
	  m_occlusionRequestedSizeX * m_occlusionRequestedSizeY != m_occlusionZBuffer.size() ||
	  !CTexture::s_ptexZTargetReadBack[0])
	{
		m_bOcclusionTexturesValid = true;

		m_occlusionZBuffer.resize(m_occlusionRequestedSizeX * m_occlusionRequestedSizeY);

		for (size_t y = 0; y < m_occlusionDownsampleSizeY; y++)      // Clear CPU-side buffer
			for (size_t x = 0; x < m_occlusionDownsampleSizeX; x++)
				m_occlusionZBuffer[x + y * m_occlusionDownsampleSizeX] = 1.0f;

		m_occlusionDownsampleSizeX = m_occlusionRequestedSizeX;
		m_occlusionDownsampleSizeY = m_occlusionRequestedSizeY;
		const uint32 nFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_STAGE_READBACK;

		for (size_t a = 0; a < 4; a++)
		{
			if (CTexture::s_ptexZTargetReadBack[a])
			{
				CTexture::s_ptexZTargetReadBack[a]->m_nFlags = nFlags;
				CTexture::s_ptexZTargetReadBack[a]->SetWidth(m_occlusionDownsampleSizeX);
				CTexture::s_ptexZTargetReadBack[a]->SetHeight(m_occlusionDownsampleSizeY);

				CTexture::s_ptexZTargetReadBack[a]->CreateRenderTarget(CTexture::s_eTFZ, Clr_FarPlane_R);
				CTexture::s_ptexZTargetReadBack[a]->Clear();
			}
			else
			{
				CTexture::s_ptexZTargetReadBack[a] = CTexture::CreateRenderTarget("$ZTargetReadBack", gcpRendD3D->m_occlusionDownsampleSizeX, gcpRendD3D->m_occlusionDownsampleSizeY, Clr_FarPlane_R, eTT_2D, nFlags, CTexture::s_eTFZ);
				CTexture::s_ptexZTargetReadBack[a]->Clear();
			}

			//			gcpRendD3D->FX_ClearTarget(CTexture::s_ptexZTargetReadBack[a]);
		}

		m_occlusionSourceSizeX = sourceWidth;
		m_occlusionSourceSizeY = sourceHeight;

		int downSampleX = max(0, 1 + IntegerLog2((uint16)((m_occlusionSourceSizeX * m_RP.m_CurDownscaleFactor.x) / m_occlusionDownsampleSizeX)));
		int downSampleY = max(0, 1 + IntegerLog2((uint16)((m_occlusionSourceSizeY * m_RP.m_CurDownscaleFactor.y) / m_occlusionDownsampleSizeY)));
		m_numOcclusionDownsampleStages = min(4, max(downSampleX, downSampleY));

		for (int a = 0; a < m_numOcclusionDownsampleStages; a++)
		{
			int width  = m_occlusionDownsampleSizeX << (m_numOcclusionDownsampleStages - a - 1);
			int height = m_occlusionDownsampleSizeY << (m_numOcclusionDownsampleStages - a - 1);

			if (CTexture::s_ptexZTargetDownSample[a])
			{
				CTexture::s_ptexZTargetDownSample[a]->m_nFlags = nFlags;
				CTexture::s_ptexZTargetDownSample[a]->SetWidth(width);
				CTexture::s_ptexZTargetDownSample[a]->SetHeight(height);

				CTexture::s_ptexZTargetDownSample[a]->CreateRenderTarget(CTexture::s_eTFZ, Clr_FarPlane_R);
			}
			else
			{
				assert(CTexture::s_ptexZTargetDownSample[a]);
			}
		}
	}

	if ((!m_occlusionDownsampleSizeX || !m_occlusionDownsampleSizeY) || !m_bOcclusionTexturesValid)
		return;

	++m_occlusionBuffer;
	const size_t Idx           = m_RP.m_nProcessThreadID;
	Matrix44 occlusionViewProj = m_occlusionViewProjBuffer[Idx];
	Matrix44 mCurView, mCurProj;
	mCurView.SetIdentity();
	mCurProj.SetIdentity();
	GetModelViewMatrix(reinterpret_cast<f32*>(&mCurView));
	GetProjectionMatrix(reinterpret_cast<f32*>(&mCurProj));

	if (bReverseDepth)
		mCurProj = ReverseDepthHelper::Convert(mCurProj);

	m_occlusionViewProjBuffer[Idx] = mCurView * mCurProj;

	m_RP.m_nZOcclusionBufferID = ((m_RP.m_nZOcclusionBufferID + 1) < CULLER_MAX_CAMS) ? (m_RP.m_nZOcclusionBufferID + 1) : 0;

	m_RP.m_OcclusionCameraBuffer[m_RP.m_nZOcclusionBufferID] = mCurView * mCurProj;

	int nCameraID = -1;

	if (!CTexture::s_ptexZTargetReadBack[Idx] || !CTexture::s_ptexZTargetReadBack[Idx]->GetDevTexture())
		return;

#if CRY_PLATFORM_DURANGO
	bool bReadZBufferDirectlyFromVMEM = (CRenderer::CV_r_ReadZBufferDirectlyFromVMEM == 1);
#else
	bool bReadZBufferDirectlyFromVMEM = false;
#endif

	// Read data from previous frame
	// There is a slight chance of a race condition when the main thread reads from the occlusion buffer during the following update
	if (bReadZBufferDirectlyFromVMEM == false)
	{
		CTexture::s_ptexZTargetReadBack[Idx]->GetDevTexture()->AccessCurrStagingResource(0, false, [ =, &mCurProj, &nCameraID](void* pData, uint32 rowPitch, uint32 slicePitch)
		{
			float* pDepths = reinterpret_cast<float*>(pData);
			const CRenderCamera& rc = GetRCamera();
			const float zn = rc.fNear;
			const float zf = rc.fFar;
			const float ProjRatioX = zf / (zf - zn);
			const float ProjRatioY = zn / (zn - zf);

			uint32 nBufferSize = m_occlusionDownsampleSizeY * m_occlusionDownsampleSizeX;

			if (bUseNativeDepth)
			{
				float x = floorf(pDepths[0] * 0.5f);      // Decode the ID from the first pixel
				m_occlusionZBuffer[0] = pDepths[0] - (x * 2.0f);
				nCameraID = (int)(x);

				for (uint32 x = 1; x < nBufferSize; x++)
				{
					const float fDepthVal = bReverseDepth ? 1.0f - pDepths[x] : pDepths[x];
					m_occlusionZBuffer[x] = max(fDepthVal, FLT_EPSILON);
				}
			}
			else
			{
				for (uint32 x = 0; x < nBufferSize; x++)
				{
					m_occlusionZBuffer[x] = max(ProjRatioY / max(pDepths[x], FLT_EPSILON) + ProjRatioX, FLT_EPSILON);
				}
			}

			m_occlusionViewProj = occlusionViewProj;

			return true;
		});
	}
	m_occlusionViewProjBuffer[Idx] = mCurView * mCurProj;

	if (bUseNativeDepth)
	{
		nCameraID           = max((int)0, min(nCameraID, (int)(CULLER_MAX_CAMS - 1)));
		m_occlusionViewProj = m_RP.m_OcclusionCameraBuffer[nCameraID];
	}

	// downsample on GPU
	RECT srcRect;
	srcRect.top    = srcRect.left = 0;
	srcRect.right  = LONG(CTexture::s_ptexZTargetDownSample[0]->GetWidth() * m_RP.m_CurDownscaleFactor.x);
	srcRect.bottom = LONG(CTexture::s_ptexZTargetDownSample[0]->GetHeight() * m_RP.m_CurDownscaleFactor.y);

	RECT* srcRegion = &srcRect;

	bool bMSAA = m_RP.m_MSAAData.Type ? true : false;

	D3DShaderResource* pZTargetOrigSRV = CTexture::s_ptexZTarget->GetShaderResourceView(bMSAA ? SResourceView::DefaultViewMS : SResourceView::DefaultView);
	if (bUseNativeDepth)
	{
		// Read native depth, rather than linear. TODO: Check me, this may be slow on ATI MSAA
		CTexture::s_ptexZTarget->SetShaderResourceView(gcpRendD3D->m_DepthBufferOrigMSAA.pTexture->GetDeviceDepthReadOnlySRV(0, -1, bMSAA), bMSAA);

		int vpX, vpY, vpWidth, vpHeight;
		GetViewport(&vpX, &vpY, &vpWidth, &vpHeight);

		srcRect.right  = LONG(srcRect.right * vpWidth / float(m_width));
		srcRect.bottom = LONG(srcRect.bottom * vpHeight / float(m_height));
	}
	else
	{
		bMSAA = false;
	}

	CTexture* pSrc = CTexture::s_ptexZTarget;
	CTexture* pDst = CTexture::s_ptexZTarget;

	if (CVrProjectionManager::IsMultiResEnabledStatic())
		pSrc = CVrProjectionManager::Instance()->GetZTargetFlattened();

	bool bUseMSAA = bMSAA;
	const SPostEffectsUtils::EDepthDownsample downsampleMode = (bUseNativeDepth && bReverseDepth)
	  ? SPostEffectsUtils::eDepthDownsample_Min
	  : SPostEffectsUtils::eDepthDownsample_Max;

	for (int i = 0; i < m_numOcclusionDownsampleStages; i++)
	{
		pDst = CTexture::s_ptexZTargetDownSample[i];
		GetUtils().StretchRect(pSrc, pDst, false, false, false, false, downsampleMode, false, srcRegion);
		pSrc      = pDst;
		srcRegion = NULL;
		bUseMSAA  = false;
	}

	pSrc = pDst;
	pDst = CTexture::s_ptexZTargetReadBack[Idx];
	PostProcessUtils().StretchRect(pSrc, pDst, false, false, false, false, downsampleMode);

	//  Blend ID into top left pixel of readback buffer
	gcpRendD3D->FX_PushRenderTarget(0, pDst, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, 1, 1);

	CShader* pSH     = CShaderMan::s_ShaderCommon;
	uint32   nPasses = 0;
	pSH->FXSetTechnique("ClearUniform");
	pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	pSH->FXBeginPass(0);

	static CCryNameR pClearParams("vClearParam");
	Vec4 vFrameID = Vec4((float)(m_RP.m_nZOcclusionBufferID * 2.0f), 0, 0, 0);
	pSH->FXSetPSFloat(pClearParams, &vFrameID, 1);

	FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
	D3DSetCull(eCULL_None);
	float fX = (float)m_CurViewport.nWidth;
	float fY = (float)m_CurViewport.nHeight;
	ColorF col = Col_Black;
	DrawQuad(-0.5f, -0.5f, fX - 0.5f, fY - 0.5f, col, 1.0f, fX, fY, fX, fY);

	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(0, 0, GetWidth(), GetHeight());

	// Copy to CPU accessible memory
	if (bReadZBufferDirectlyFromVMEM == false)
	{
		CTexture::s_ptexZTargetReadBack[Idx]->GetDevTexture()->DownloadToStagingResource(0);
	}

	if (bUseNativeDepth)
	{
		CTexture::s_ptexZTarget->SetShaderResourceView(pZTargetOrigSRV, bMSAA);
	}

#if CRY_PLATFORM_DURANGO && defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	if (bReadZBufferDirectlyFromVMEM == true)
	{
		// get camera settings for reprojection
		const CRenderCamera& rc = GetRCamera();
		m_occlusionZNear[m_RP.m_nProcessThreadID] = rc.fNear;
		m_occlusionZFar[m_RP.m_nProcessThreadID]  = rc.fFar;

		// create resources if needed, we allocate direclty in VMEM for reading
		// but we need to create a texture on this data to that the GPU can copy the data into it
		if (m_occlusionGPUData[m_RP.m_nProcessThreadID] == NULL)
		{
			gRenDev->m_DevMan.CreateFence(m_occlusionFence[m_RP.m_nProcessThreadID]);

			D3D11_TEXTURE2D_DESC subResDesc;
			CTexture::s_ptexZTargetReadBack[m_RP.m_nProcessThreadID]->GetDevTexture()->Get2DTexture()->GetDesc(&subResDesc);

			HRESULT hr = D3DAllocateGraphicsMemory(subResDesc.Width * subResDesc.Height * sizeof(float), 0, 0, D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_NONCOHERENT_GPU_READONLY, &m_occlusionGPUData[m_RP.m_nProcessThreadID]);
			CHECK_HRESULT(hr);

			hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementTexture2D(&subResDesc, XG_TILE_MODE_LINEAR, 0, m_occlusionGPUData[m_RP.m_nProcessThreadID], &m_occlusionReadBackTexture[m_RP.m_nProcessThreadID]);
			CHECK_HRESULT(hr);

			m_occlusionDataPitch = subResDesc.Width;
		}

		// copy data from last downsampling state to our read buffer
		// afterwards add a GPU flush and a fence for synchronization
		CTexture* pLastDownSampleLevel = CTexture::s_ptexZTargetDownSample[m_numOcclusionDownsampleStages - 1];
		gcpRendD3D->GetDeviceContext().CopyResource(m_occlusionReadBackTexture[m_RP.m_nProcessThreadID], pLastDownSampleLevel->GetDevTexture()->Get2DTexture());
		gcpRendD3D->GetPerformanceDeviceContext().FlushGpuCaches(m_occlusionReadBackTexture[m_RP.m_nProcessThreadID]);
		gRenDev->m_DevMan.IssueFence(m_occlusionFence[m_RP.m_nProcessThreadID]);
	}
#endif
}

void CD3D9Renderer::FX_UpdateCharCBs()
{
	PROFILE_FRAME(FX_UpdateCharCBs);
	unsigned poolId = (m_nPoolIndexRT) % 3;
	for (util::list<SCharacterInstanceCB>* iter = m_CharCBActiveList[poolId].next; iter != &m_CharCBActiveList[poolId]; iter = iter->next)
	{
		SCharacterInstanceCB* cb = iter->item<& SCharacterInstanceCB::list>();
		if (cb->updated)
			continue;
		SSkinningData* pSkinningData = cb->m_pSD;

		// make sure all sync jobs filling the buffers have finished
		if (pSkinningData->pAsyncJobs)
		{
			PROFILE_FRAME(FX_UpdateCharCBs_ASYNC_WAIT);
			gEnv->pJobManager->WaitForJob(*pSkinningData->pAsyncJobs);
		}

		// NOTE: The pointers and the size is 16 byte aligned
		size_t boneQuatsSSize   = Align(pSkinningData->nNumBones        * sizeof(DualQuat                       ), CRY_PLATFORM_ALIGNMENT);
		size_t activeMorphsSize = Align(pSkinningData->nNumActiveMorphs * sizeof(compute_skinning::SActiveMorphs), CRY_PLATFORM_ALIGNMENT);

		cb->boneTransformsBuffer->UpdateBuffer(pSkinningData->pBoneQuatsS, boneQuatsSSize);
		if (pSkinningData->nNumActiveMorphs)
			cb->activeMorphsBuffer.UpdateBufferContent(pSkinningData->pActiveMorphs, activeMorphsSize);

		cb->updated = true;
	}

	// free a buffer each frame if we have an over-comittment of more than 75% compared
	// to our last 2 frames of rendering
	{
		int committed      = CryInterlockedCompareExchange((LONG*)&m_CharCBAllocated, 0, 0);
		int totalRequested = m_CharCBFrameRequired[poolId] + m_CharCBFrameRequired[(poolId - 1) % 3];
		WriteLock _lock(m_lockCharCB);
		if (totalRequested * 4 > committed * 3 && m_CharCBFreeList.empty() == false)
		{
			delete m_CharCBFreeList.prev->item<& SCharacterInstanceCB::list>();
			CryInterlockedDecrement(&m_CharCBAllocated);
		}
	}
}

void* CD3D9Renderer::FX_AllocateCharInstCB(SSkinningData* pSkinningData, uint32 frameId)
{
	PROFILE_FRAME(FX_AllocateCharInstCB);
	SCharacterInstanceCB* cb = NULL;
	{
		WriteLock _lock(m_lockCharCB);
		if (m_CharCBFreeList.empty() == false)
		{
			cb = m_CharCBFreeList.next->item<& SCharacterInstanceCB::list>();
			cb->list.erase();
		}
	}
	if (cb == NULL)
	{
		cb           = new SCharacterInstanceCB();
		cb->boneTransformsBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(768*sizeof(DualQuat), true, true);
		cb->activeMorphsBuffer.Create(768, sizeof(compute_skinning::SActiveMorphs), DXGI_FORMAT_UNKNOWN, DX11BUF_DYNAMIC | DX11BUF_STRUCTURED | DX11BUF_BIND_SRV, NULL);
		CryInterlockedIncrement(&m_CharCBAllocated);
	}
	cb->updated = false;
	cb->m_pSD   = pSkinningData;
	{
		WriteLock _lock(m_lockCharCB);
		cb->list.relink_tail(&m_CharCBActiveList[frameId % 3]);
	}
	CryInterlockedIncrement(&m_CharCBFrameRequired[frameId % 3]);
	return cb;
}

void CD3D9Renderer::FX_ClearCharInstCB(uint32 frameId)
{
	PROFILE_FRAME(FX_ClearCharInstCB);
	uint32 poolId = frameId % 3;
	WriteLock _lock(m_lockCharCB);
	m_CharCBFrameRequired[poolId] = 0;
	m_CharCBFreeList.splice_tail(&m_CharCBActiveList[poolId]);
}

// Render thread only scene rendering
void CD3D9Renderer::RT_RenderScene(CRenderView* pRenderView, int nFlags, SThreadInfo& TI, void (* RenderFunc)())
{
	PROFILE_LABEL_SCOPE(pRenderView->IsRecursive() ? "SCENE_REC" : "SCENE");

	if (GetS3DRend().IsStereoEnabled())
	{
		// Use current stereo eye target as backbuffer
		assert(m_nRTStackLevel[0] == 0);
		FX_SetRenderTarget(0, GetS3DRend().GetEyeTarget((nFlags & SHDF_STEREO_LEFT_EYE) ? LEFT_EYE : RIGHT_EYE), &m_DepthBufferOrig, false);
		GetS3DRend().SkipEyeTargetClears();
	}
	
	gcpRendD3D->SetCurDownscaleFactor(gcpRendD3D->m_CurViewportScale);

	// Skip scene rendering when device is lost
	if (m_bDeviceLost)
		return;

	// Update the character CBs (only active on D3D11 style platforms)
	// Needs to be done before objects are compiled
	FX_UpdateCharCBs();

	SThreadInfo* const pShaderThreadInfo = &(m_RP.m_TI[m_RP.m_nProcessThreadID]);

	// Assign current render view rendered by pipeline
	m_RP.m_pCurrentRenderView = pRenderView;

	CRenderMesh::FinalizeRendItems(m_RP.m_nProcessThreadID);
	CMotionBlur::InsertNewElements();
	CRenderMesh::UpdateModified();

	int nRecurse = pRenderView->IsRecursive() ? 1 : 0;
	FX_ApplyThreadState(TI, &m_RP.m_OldTI[nRecurse]);

	////////////////////////////////////////////////
	{
		PROFILE_FRAME(WaitForRenderView);
		pRenderView->SwitchUsageMode(CRenderView::eUsageModeReading);
	}

	CFlashTextureSourceSharedRT::SetupSharedRenderTargetRT();
	RT_RenderUITextures();

	CTimeValue Time = iTimer->GetAsyncTime();

	if (!nRecurse)
	{
		m_MainViewport.nX      = 0;
		m_MainViewport.nY      = 0;
		m_MainViewport.nWidth  = m_width;
		m_MainViewport.nHeight = m_height;

	}

	if (!nRecurse)
	{
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = m_MainViewport.nX;
		viewport.TopLeftY = m_MainViewport.nY;
		viewport.Width = m_MainViewport.nWidth;
		viewport.Height = m_MainViewport.nHeight;
		viewport.MinDepth = 0.f; // m_MainViewport.fMinZ and fMaxZ are zeros
		viewport.MaxDepth = 1.f;

		bool bRightEye = (nFlags & SHDF_STEREO_RIGHT_EYE) != 0;

		CVrProjectionManager::Instance()->Configure(viewport, bRightEye);
	}

	// invalidate object pointers
	m_RP.m_pCurObject = m_RP.m_pPrevObject = m_RP.m_pIdendityRenderObject;

	static int lightVolumeOldFrameID = -1;
	int newFrameID   = this->GetFrameID(false);

	// Update light volumes info
	const bool updateLightVolumes =
		lightVolumeOldFrameID != newFrameID &&
		nRecurse == 0 &&
		(nFlags & SHDF_ALLOWPOSTPROCESS) != 0;
	if (updateLightVolumes)
	{
		RT_UpdateLightVolumes();
		lightVolumeOldFrameID = newFrameID;
	}

	int nSaveDrawNear     = CV_r_nodrawnear;
	int nSaveDrawCaustics = CV_r_watercaustics;
	int nSaveStreamSync   = CV_r_texturesstreamingsync;
	if (nFlags & SHDF_NO_DRAWCAUSTICS)
		CV_r_watercaustics = 0;
	if (nFlags & SHDF_NO_DRAWNEAR)
		CV_r_nodrawnear = 1;
	if (nFlags & SHDF_STREAM_SYNC)
		CV_r_texturesstreamingsync = 1;

	m_bDeferredDecals = false;
	uint32 nSaveRendFlags = m_RP.m_nRendFlags;
	m_RP.m_nRendFlags = nFlags;

	const bool bHDRRendering = (nFlags & SHDF_ALLOWHDR) && IsHDRModeEnabled() && !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE);
	const bool bNewGraphicsPipeline = m_nGraphicsPipeline >= 1 && !pRenderView->IsRecursive() && (nFlags & SHDF_ALLOWPOSTPROCESS) && !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE);

	if (!IsHDRModeEnabled())
	{
		m_vSceneLuminanceInfo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		m_fAdaptedSceneScale  = m_fAdaptedSceneScaleLBuffer = m_fScotopicSceneScale = 1.0f;
	}

	if (!nRecurse && bHDRRendering)
	{
		m_RP.m_bUseHDR = true;
		if (FX_HDRScene(m_RP.m_bUseHDR, false))
			m_RP.m_PersFlags2 |= RBPF2_HDR_FP16;
	}
	else
	{
		m_RP.m_bUseHDR = false;
		FX_HDRScene(false);

		if ((pShaderThreadInfo->m_PersFlags & RBPF_DRAWTOTEXTURE) && bHDRRendering)
			m_RP.m_PersFlags2 |= RBPF2_HDR_FP16;
		else
			m_RP.m_PersFlags2 &= ~RBPF2_HDR_FP16;

		if (bNewGraphicsPipeline) // new graphics pipeline assumes hdr target is on bottom of stack
			FX_PushRenderTarget(0, CTexture::s_ptexHDRTarget, &m_DepthBufferOrigMSAA, -1, true);
	}

	// Prepare post processing
	bool bAllowPostProcess = (nFlags & SHDF_ALLOWPOSTPROCESS) && !nRecurse && (CV_r_PostProcess) && !CV_r_measureoverdraw &&
	  !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE);

	bool bAllowSubpixelShift = bAllowPostProcess
	  && (gcpRendD3D->FX_GetAntialiasingType() & eAT_REQUIRES_SUBPIXELSHIFT_MASK)
	  && (!gEnv->IsEditing() || CRenderer::CV_r_AntialiasingModeEditor)
	  && (GetWireframeMode() == R_SOLID_MODE)
	  && (CRenderer::CV_r_DeferredShadingDebugGBuffer == 0);

	m_vProjMatrixSubPixoffset = Vec2(0.0f, 0.0f);

	if (bAllowSubpixelShift)
	{
		static const Vec2 vNQAA4x[4] =
		{
			Vec2(3.0f / 8.0f,  1.0f / 8.0f),
			Vec2(1.0f / 8.0f,  -3.0f / 8.0f),
			Vec2(-1.0f / 8.0f, 3.0f / 8.0f),
			Vec2(-3.0f / 8.0f, -1.0f / 8.0f)
		};

		static const Vec2 vNQAA5x[5] =
		{
			Vec2(4.0f / 10.0f,  -4.0f / 10.0f),
			Vec2(2.0f / 10.0f,  2.0f / 10.0f),
			Vec2(0.0f / 10.0f,  -2.0f / 10.0f),
			Vec2(-2.0f / 10.0f, 4.0f / 10.0f),
			Vec2(-4.0f / 10.0f, 0.0f / 10.0f)
		};

		static const Vec2 vNQAA8x[8] =
		{
			Vec2(7.0f / 16.0f,  -7.0f / 16.0f),
			Vec2(5.0f / 16.0f,  5.0f / 16.0f),
			Vec2(3.0f / 16.0f,  1.0f / 16.0f),
			Vec2(1.0f / 16.0f,  7.0f / 16.0f),
			Vec2(-1.0f / 16.0f, -5.0f / 16.0f),
			Vec2(-3.0f / 16.0f, -1.0f / 16.0f),
			Vec2(-5.0f / 16.0f, 3.0f / 16.0f),
			Vec2(-7.0f / 16.0f, -3.0f / 16.0f)
		};

		static const Vec2 vSSAA2x[2] =
		{
			Vec2(-0.25f, 0.25f),
			Vec2(0.25f,  -0.25f)
		};

		static const Vec2 vSSAA3x[3] =
		{
			Vec2(-1.0f / 3.0f, -1.0f / 3.0f),
			Vec2(1.0f / 3.0f,  0.0f / 3.0f),
			Vec2(0.0f / 3.0f,  1.0f / 3.0f)
		};

		static const Vec2 vSSAA4x[4] =
		{
			Vec2(-0.125, -0.375), Vec2(0.375,  -0.125),
			Vec2(-0.375, 0.125),  Vec2(0.125,  0.375)
		};

		static const Vec2 vSMAA4x[2] =
		{
			Vec2(-0.125f, -0.125f),
			Vec2(0.125f,  0.125f)
		};

		static const Vec2 vSSAA8x[8] =
		{
			Vec2(0.0625,  -0.1875),  Vec2(-0.0625,   0.1875),
			Vec2(0.3125,  0.0625),   Vec2(-0.1875,   -0.3125),
			Vec2(-0.3125, 0.3125),   Vec2(-0.4375,   -0.0625),
			Vec2(0.1875,  0.4375),   Vec2(0.4375,    -0.4375)
		};

		static const Vec2 vSGSSAA8x8[8] =
		{
			Vec2(6.0f / 7.0f, 0.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(2.0f / 7.0f, 1.0f / 7.0f) - Vec2(0.5f, 0.5f),
			Vec2(4.0f / 7.0f, 2.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(0.0f / 7.0f, 3.0f / 7.0f) - Vec2(0.5f, 0.5f),
			Vec2(7.0f / 7.0f, 4.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(3.0f / 7.0f, 5.0f / 7.0f) - Vec2(0.5f, 0.5f),
			Vec2(5.0f / 7.0f, 6.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(1.0f / 7.0f, 7.0f / 7.0f) - Vec2(0.5f, 0.5f)
		};

		const int nSampleID = SPostEffectsUtils::m_iFrameCounter;
		Vec2 vCurrSubSample = Vec2(0, 0);
		switch (CRenderer::CV_r_AntialiasingTAAPattern)
		{
		case 1:
			vCurrSubSample = vSSAA2x[nSampleID % 2];
			break;
		case 2:
			vCurrSubSample = vSSAA3x[nSampleID % 3];
			break;
		case 3:
			vCurrSubSample = vSSAA4x[nSampleID % 4];
			break;
		case 4:
			vCurrSubSample = vSSAA8x[nSampleID % 8];
			break;
		case 5:
			vCurrSubSample = vSGSSAA8x8[nSampleID % 8];
			break;
		case 6:
			vCurrSubSample = Vec2(SPostEffectsUtils::srandf(), SPostEffectsUtils::srandf()) * 0.5f;
			break;
		case 7:
			vCurrSubSample = Vec2(SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 8, 2) - 0.5f,
				SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 8, 3) - 0.5f);
			break;
		case 8:
			vCurrSubSample = Vec2(SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 1024, 2) - 0.5f,
				SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 1024, 3) - 0.5f);
			break;
		}

		m_vProjMatrixSubPixoffset.x = (vCurrSubSample.x * 2.0f / (float)m_width) / m_RP.m_CurDownscaleFactor.x;
		m_vProjMatrixSubPixoffset.y = (vCurrSubSample.y * 2.0f / (float)m_height) / m_RP.m_CurDownscaleFactor.y;
	}

	FX_PostProcessScene(bAllowPostProcess);
	bool bAllowDeferred = (nFlags & SHDF_ZPASS) && !nRecurse && !CV_r_measureoverdraw && !(pShaderThreadInfo->m_PersFlags & (RBPF_MAKESPRITE));
	if (bAllowDeferred)
	{
		m_RP.m_PersFlags2 |= RBPF2_ALLOW_DEFERREDSHADING;
		FX_DeferredRendering(pRenderView, false, true);
	}
	else
	{
		m_RP.m_PersFlags2 &= ~RBPF2_ALLOW_DEFERREDSHADING;
	}

	{
		if (!nRecurse && (nFlags & SHDF_ALLOWHDR) && !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE))
		{
			ETEX_Format eTF = (m_RP.m_bUseHDR && m_nHDRType == 1) ? eTF_R16G16B16A16F : eTF_R8G8B8A8;
			int nW          = gcpRendD3D->GetWidth();  //m_d3dsdBackBuffem.Width;
			int nH          = gcpRendD3D->GetHeight(); //m_d3dsdBackBuffem.Height;
			if (!CTexture::s_ptexSceneTarget || CTexture::s_ptexSceneTarget->GetDstFormat() != eTF || CTexture::s_ptexSceneTarget->GetWidth() != nW || CTexture::s_ptexSceneTarget->GetHeight() != nH)
				CTexture::GenerateSceneMap(eTF);
		}
	}

	if (bNewGraphicsPipeline)
	{
		GetGraphicsPipeline().Prepare(pRenderView, EShaderRenderingFlags(nFlags));
		GetGraphicsPipeline().Execute();
	}
	else
	{
		if ((nFlags & SHDF_ALLOWPOSTPROCESS) && !nRecurse && !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE))
			FX_DeferredRainPreprocess();

		static ICVar* cvar_gd = gEnv->pConsole->GetCVar("r_ComputeSkinning");
		if (cvar_gd && cvar_gd->GetIVal())
			FX_ProcessCharDeformation(pRenderView);

		{
			bool bLighting = !nFlags;

			if ((nFlags & (SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS)) && !nRecurse && CV_r_usezpass)
			{
				FX_ProcessZPassRenderLists();

				FX_DeferredRainGBuffer();
				FX_DeferredSnowLayer();
			}

#if defined(FEATURE_SVO_GI)
			if ((nFlags & SHDF_ALLOWHDR) && !nRecurse && CSvoRenderer::GetInstance())
			{
				PROFILE_LABEL_SCOPE("SVOGI");
				CSvoRenderer::GetInstance()->UpdateCompute();
				CSvoRenderer::GetInstance()->UpdateRender();
			}
#endif

			bool bEmpty = SRendItem::IsListEmpty(EFSLIST_GENERAL);
			bEmpty &= SRendItem::IsListEmpty(EFSLIST_DEFERRED_PREPROCESS);
			if (!nRecurse && !bEmpty && pShaderThreadInfo->m_FS.m_bEnable && CV_r_usezpass)
				m_RP.m_PersFlags2 |= RBPF2_NOSHADERFOG;

			if (bAllowDeferred && !bEmpty)
			{
				PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");

				FX_ProcessRenderList(EFSLIST_DEFERRED_PREPROCESS, RenderFunc, false);         // Sorted list without preprocess of all deferred related passes and screen shaders
			}

			FX_RenderForwardOpaque(RenderFunc, bLighting, bAllowDeferred);

			const bool bShadowGenSpritePasses = (pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE) != 0;

			UpdatePrevMatrix(bAllowPostProcess);

			{
				PROFILE_LABEL_SCOPE("TRANSPARENT_BW");

#if defined(SUPPORTS_MSAA)                                              // Hide any minor resolve artifacts that show up very obvious on PC (bright red!)
				// temporary driver workaround for AMD harware with msaa + transfers
				if (!nRecurse && !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE))
				{
					bool isEmpty = SRendItem::IsListEmpty(EFSLIST_TRANSP);
					if (!isEmpty && CTexture::IsTextureExist(CTexture::s_ptexCurrSceneTarget))
						FX_ScreenStretchRect(CTexture::s_ptexCurrSceneTarget);
				}
#endif

				GetTiledShading().BindForwardShadingResources(NULL);
				FX_ProcessRenderList(EFSLIST_TRANSP, RenderFunc, bLighting, FB_BELOW_WATER, 0);
				GetTiledShading().UnbindForwardShadingResources();
			}

			{
				PROFILE_LABEL_SCOPE("TRANSPARENT_AW");

#if defined(SUPPORTS_MSAA)                                                   // Hide any minor resolve artifacts that show up very obvious on PC (bright red!)
				if (!nRecurse && !(pShaderThreadInfo->m_PersFlags & RBPF_MAKESPRITE))
				{
					bool isEmpty = SRendItem::IsListEmpty(EFSLIST_TRANSP);
					if (!isEmpty && CTexture::IsTextureExist(CTexture::s_ptexCurrSceneTarget))
						FX_ScreenStretchRect(CTexture::s_ptexCurrSceneTarget);
				}
#endif

				GetTiledShading().BindForwardShadingResources(NULL);
				FX_ProcessRenderList(EFSLIST_TRANSP, RenderFunc, true, FB_GENERAL, FB_BELOW_WATER);
				GetTiledShading().UnbindForwardShadingResources();

				if (bAllowPostProcess && !FX_GetMSAAMode() && CV_r_TranspDepthFixup) // TODO: fix for MSAA
				{
					FX_DepthFixupMerge();
				}
			}

			FX_ProcessHalfResParticlesRenderList(pRenderView, EFSLIST_HALFRES_PARTICLES, RenderFunc, bLighting);

			if (bAllowPostProcess)
				m_CameraProjMatrixPrev = m_CameraProjMatrix;

			// insert fence which is used on consoles to prevent overwriting VideoMemory
			InsertParticleVideoDataFence();

			if (bAllowDeferred && !nRecurse)
				FX_DeferredSnowDisplacement();

			if (!nRecurse)
			{
				gcpRendD3D->m_RP.m_PersFlags1 &= ~RBPF1_SKIP_AFTER_POST_PROCESS;

				FX_ProcessRenderList(EFSLIST_HDRPOSTPROCESS, RenderFunc, false);         // Sorted list without preprocess of all fog passes and screen shaders
				FX_ProcessRenderList(EFSLIST_AFTER_HDRPOSTPROCESS, RenderFunc, false);   // for specific cases where rendering after tone mapping is needed
				FX_ProcessRenderList(EFSLIST_POSTPROCESS, RenderFunc, false);            // Sorted list without preprocess of all fog passes and screen shaders

				bool bDrawAfterPostProcess = !(gcpRendD3D->m_RP.m_PersFlags1 & RBPF1_SKIP_AFTER_POST_PROCESS);

				RT_SetViewport(0, 0, GetWidth(), GetHeight());

				// HACK - Crysis 2 DevTrack issue 60284 X360 - SP: GLOBAL - HUD: The red dot of the mounted HMG crosshair remains clearly visible when the user pauses the game
				if (bDrawAfterPostProcess && (!gEnv->pTimer || !gEnv->pTimer->IsTimerPaused(ITimer::ETIMER_GAME)))
				{
					PROFILE_LABEL_SCOPE("AFTER_POSTPROCESS");                            // for specific cases where rendering after all post effects is needed
					if (GetS3DRend().IsPostStereoEnabled())
					{
						m_pStereoRenderer->BeginRenderingTo(LEFT_EYE);
						FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, RenderFunc, false);
						m_pStereoRenderer->EndRenderingTo(LEFT_EYE);

						if (GetS3DRend().RequiresSequentialSubmission())
						{
							m_pStereoRenderer->BeginRenderingTo(RIGHT_EYE);
							FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, RenderFunc, false);
							m_pStereoRenderer->EndRenderingTo(RIGHT_EYE);
						}
					}
					else
					{
						FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, RenderFunc, false);
					}
				}

				if (CV_r_DeferredShadingDebug && bAllowDeferred)
					FX_DeferredRendering(pRenderView, true);
			}
		}
	}                                     // r_GraphicsPipeline

	if (!nRecurse)
	{
		if (CRenderer::CV_r_shownormals)
			FX_ProcessRenderList(EFSLIST_GENERAL, FX_DrawNormals, false);
		if (CRenderer::CV_r_showtangents)
			FX_ProcessRenderList(EFSLIST_GENERAL, FX_DrawTangents, false);
	}

	CFlashTextureSourceBase::RenderLights();

	FX_ApplyThreadState(m_RP.m_OldTI[nRecurse], NULL);

	m_RP.m_PS[m_RP.m_nProcessThreadID].m_fRenderTime += iTimer->GetAsyncTime().GetDifferenceInSeconds(Time);

	m_RP.m_nRendFlags          = nSaveRendFlags;
	CV_r_nodrawnear            = nSaveDrawNear;
	CV_r_watercaustics         = nSaveDrawCaustics;
	CV_r_texturesstreamingsync = nSaveStreamSync;

	////////////////////////////////////////////////
	// Lists still needed for right eye when stereo is active
	if (!GetS3DRend().RequiresSequentialSubmission() || !(nFlags & SHDF_STEREO_LEFT_EYE))
	{
		PROFILE_FRAME(RenderViewEndFrame);
		pRenderView->SwitchUsageMode(CRenderView::eUsageModeReadingDone);
		for (auto& fr : pRenderView->m_shadows.m_renderFrustums)
		{
			auto pShadowView = reinterpret_cast<CRenderView*>(fr.pShadowsView.get());
			pShadowView->SwitchUsageMode(IRenderView::eUsageModeReadingDone);
		}

		pRenderView->Clear();
		m_RP.m_pSunLight = nullptr;
	}
	m_RP.m_pCurrentRenderView = nullptr;

	if (GetS3DRend().IsStereoEnabled())
	{
		// Restore backbuffer
		assert(m_nRTStackLevel[0] == 0);
		FX_SetRenderTarget(0, m_pBackBuffer, &m_DepthBufferNative, false);
}
}

void CD3D9Renderer::RT_DrawUITextureInternal(S2DImage& img)
{
	static SVF_P3F_C4B_T2F pScreenQuad[] =
	{
		{ Vec3(0, 0, 0), {
						{ ~0 }
			    }, Vec2(0, 0) },
		{ Vec3(0, 0, 0), {
						{ ~0 }
			    }, Vec2(0, 0) },
		{ Vec3(0, 0, 0), {
						{ ~0 }
			    }, Vec2(0, 0) },
		{ Vec3(0, 0, 0), {
						{ ~0 }
			    }, Vec2(0, 0) },
	};
	pScreenQuad[0].xyz = Vec3(img.xpos, img.ypos, 0);
	pScreenQuad[1].xyz = Vec3(img.xpos, img.ypos + img.h, 0);
	pScreenQuad[2].xyz = Vec3(img.xpos + img.w, img.ypos, 0);
	pScreenQuad[3].xyz = Vec3(img.xpos + img.w, img.ypos + img.h, 0);
	pScreenQuad[0].st  = Vec2(img.s0, 1 - img.t0);
	pScreenQuad[1].st  = Vec2(img.s0, 1 - img.t1);
	pScreenQuad[2].st  = Vec2(img.s1, 1 - img.t0);
	pScreenQuad[3].st  = Vec2(img.s1, 1 - img.t1);

	CVertexBuffer strip(pScreenQuad, eVF_P3F_C4B_T2F);
	gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

void CD3D9Renderer::RT_RenderUITextures()
{
	if (m_uiImages.empty())
		return;

	SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA);
	SetProfileMarker("DRAWUIIMAGELIST", CRenderer::ESPM_PUSH);

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5] | g_HWSR_MaskBit[HWSR_REVERSE_DEPTH]);

	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
	std::map<CTexture*, bool> textureCleared;

	for (int i = 0; i < m_uiImages.size(); i++)
	{
		S2DImage& img = m_uiImages[i];

		// Clear target if used for the first time in this pass
		if (textureCleared.find(img.pTarget) == textureCleared.end())
		{
			FX_ClearTarget(img.pTarget, Clr_Transparent);
			textureCleared[img.pTarget] = true;
		}

		gcpRendD3D->FX_PushRenderTarget(0, img.pTarget, NULL);
		gcpRendD3D->FX_SetActiveRenderTargets();
		gcpRendD3D->RT_SetViewport(0, 0, img.pTarget->GetWidth(), img.pTarget->GetHeight());

		uint32 nPasses;
		CShader* pShader = CShaderMan::s_shPostEffects;
		static CCryNameTSCRC pTechTexToTexResampled("TextureToTextureOneAlpha");
		pShader->FXSetTechnique(pTechTexToTexResampled);
		pShader->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		pShader->FXBeginPass(0);

		static CCryNameR pParam0Name("g_vUITextureColor");
		Vec4 vParam0((f32)((img.col >> 16) % 256) / 256.0f, (f32)((img.col >> 8) % 256) / 256.0f,
		  (f32)(img.col % 256) / 256.0f, (f32)(img.col >> 24) / 256.0f);
		pShader->FXSetPSFloat(pParam0Name, &vParam0, 1);

		img.pTex->Apply(0, CTexture::GetTexState(STexState(FILTER_LINEAR, true)), EFTT_UNKNOWN, -1, SResourceView::DefaultView);
		RT_DrawUITextureInternal(img);
		pShader->FXEndPass();
		pShader->FXEnd();

		gcpRendD3D->FX_PopRenderTarget(0);
	}
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);
	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

	SetProfileMarker("DRAWUIIMAGELIST", CRenderer::ESPM_POP);
	m_uiImages.resize(0);
}

//======================================================================================================
// Process all render item lists (can be called recursively)
void CD3D9Renderer::SubmitRenderViewForRendering(RenderFunc pRenderFunc, int nFlags, SViewport& VP, const SRenderingPassInfo& passInfo, bool bSync3DEngineJobs)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)

	// Writing to this render view from this thread should be done.
	passInfo.GetRenderView()->SwitchUsageMode(CRenderView::eUsageModeWritingDone);

	if (passInfo.IsGeneralPass())
	{
		if ((nFlags & SHDF_ALLOWPOSTPROCESS))
		{
			// Perform pre-process operations for the current frame
			auto& postProcessRenderItems = passInfo.GetRenderView()->GetRenderItems(EFSLIST_PREPROCESS);
			if (!postProcessRenderItems.empty() && passInfo.GetRenderView()->GetBatchFlags(EFSLIST_PREPROCESS) & FSPR_MASK)
			{
				int nums = 0;
				int nume = nums + postProcessRenderItems.size();
				// Sort render items as we need them
				SRendItem::mfSortPreprocess(&postProcessRenderItems[nums], nume - nums);
				EF_Preprocess(&postProcessRenderItems[0], nums, nume, pRenderFunc, passInfo);
			}
		}
	}

	m_pRT->RC_RenderScene(passInfo.GetRenderView(), nFlags, pRenderFunc);
}

void CD3D9Renderer::EF_RenderScene(int nFlags, SViewport& VP, const SRenderingPassInfo& passInfo)
{
	int nThreadID     = passInfo.ThreadID();
	int nRecurseLevel = passInfo.GetRecursiveLevel();

	CTimeValue time0 = iTimer->GetAsyncTime();
#ifndef _RELEASE
	if (nRecurseLevel < 0) __debugbreak();
	if (CV_r_excludeshader->GetString()[0] != '0')
	{
		char nm[256];
		cry_strcpy(nm, CV_r_excludeshader->GetString());
		strlwr(nm);
		m_RP.m_sExcludeShader = nm;
	}
	else
		m_RP.m_sExcludeShader = "";
#endif

	SubmitRenderViewForRendering(FX_FlushShader_General, nFlags, VP, passInfo, true);

	m_RP.m_PS[nThreadID].m_fSceneTimeMT += iTimer->GetAsyncTime().GetDifferenceInSeconds(time0);
}

// Process all render item lists
void CD3D9Renderer::EF_EndEf3D(const int nFlags, const int nPrecacheUpdateIdSlow, const int nPrecacheUpdateIdFast, const SRenderingPassInfo& passInfo)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)
	int nThreadID = m_RP.m_nFillThreadID;

	m_beginFrameCount--;

	if (m_beginFrameCount < 0)
	{
		iLog->Log("Error: CRenderer::EF_EndEf3D without CRenderer::EF_StartEf");
		return;
	}

	m_RP.m_TI[m_RP.m_nFillThreadID].m_arrZonesRoundId[0] = max(m_RP.m_TI[m_RP.m_nFillThreadID].m_arrZonesRoundId[0], nPrecacheUpdateIdFast);
	m_RP.m_TI[m_RP.m_nFillThreadID].m_arrZonesRoundId[1] = max(m_RP.m_TI[m_RP.m_nFillThreadID].m_arrZonesRoundId[1], nPrecacheUpdateIdSlow);

	m_p3DEngineCommon.Update(nThreadID);

	if (CV_r_nodrawshaders == 1)
	{
		EF_ClearTargetsLater(FRT_CLEAR, Clr_Transparent);
		return;
	}

	int nAsyncShaders = CV_r_shadersasynccompiling;
	int nTexStr       = CV_r_texturesstreamingsync;
	if (nFlags & SHDF_NOASYNC)
		CV_r_shadersasynccompiling = 0;

	if (GetS3DRend().IsStereoEnabled())
	{
		GetS3DRend().ProcessScene(nFlags, passInfo);
	}
	else
	{
		EF_Scene3D(m_MainRTViewport, nFlags, passInfo);
	}

	CV_r_shadersasynccompiling = nAsyncShaders;
}

void CD3D9Renderer::EF_InvokeShadowMapRenderJobs(CRenderView* pRenderView, const int nFlags)
{
	if (!pRenderView->IsRecursive())
	{
		EF_PrepareShadowGenRenderList(pRenderView);
	}
}

void CD3D9Renderer::EF_Scene3D(SViewport& VP, int nFlags, const SRenderingPassInfo& passInfo)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)
	int nThreadID = m_RP.m_nFillThreadID;
	assert(nThreadID >= 0 && nThreadID < RT_COMMAND_BUF_COUNT);

	bool bFullScreen = true;
	SDynTexture* pDT = NULL;

	const bool bIsRightEye = (nFlags & (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE)) == SHDF_STEREO_RIGHT_EYE;
	if (!passInfo.IsRecursivePass() && !bIsRightEye && !CV_r_measureoverdraw && !(m_RP.m_TI[nThreadID].m_PersFlags & RBPF_MAKESPRITE))
	{
		bool bAllowDeferred = (nFlags & SHDF_ZPASS) != 0;
		if (bAllowDeferred)
		{
			gRenDev->m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);

			SShaderItem shItem(CShaderMan::s_shDeferredShading);
			CRenderObject* pObj = EF_GetObject_Temp(passInfo.ThreadID());
			if (pObj)
			{
				pObj->m_II.m_Matrix.SetIdentity();

				SRenderingPassInfo passInfoDeferredSort(passInfo);
				passInfoDeferredSort.OverrideRenderItemSorter(SRendItemSorter(SRendItemSorter::eDeferredShadingPass));
				EF_AddEf(m_RP.m_pREDeferredShading, shItem, pObj, passInfoDeferredSort, EFSLIST_DEFERRED_PREPROCESS, 0);
			}
		}

		if ((nFlags & SHDF_ALLOWHDR) && IsHDRModeEnabled())
		{
			SShaderItem shItem(CShaderMan::s_shHDRPostProcess);
			CRenderObject* pObj = EF_GetObject_Temp(passInfo.ThreadID());
			if (pObj)
			{
				pObj->m_II.m_Matrix.SetIdentity();
				EF_AddEf(m_RP.m_pREHDR, shItem, pObj, passInfo, EFSLIST_HDRPOSTPROCESS, 0);
			}
		}

		bool bAllowPostProcess = (nFlags & SHDF_ALLOWPOSTPROCESS) && (CV_r_PostProcess);
		bAllowPostProcess &= (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_MIRRORCULL) == 0;
		if (bAllowPostProcess)
		{
			SShaderItem shItem(CShaderMan::s_shPostEffects);
			CRenderObject* pObj = EF_GetObject_Temp(passInfo.ThreadID());
			if (pObj)
			{
				pObj->m_II.m_Matrix.SetIdentity();
				EF_AddEf(m_RP.m_pREPostProcess, shItem, pObj, passInfo, EFSLIST_POSTPROCESS, 0);
			}
		}
	}

	// Update per-frame params
	UpdateConstParamsPF(passInfo);

	EF_RenderScene(nFlags, VP, passInfo);

	if (!passInfo.IsRecursivePass()) gRenDev->GetIRenderAuxGeom()->Flush();
}

void CD3D9Renderer::RT_PrepareStereo(int mode, int output)
{
	m_pStereoRenderer->PrepareStereo((EStereoMode)mode, (EStereoOutput)output);
}

void CD3D9Renderer::EnablePipelineProfiler(bool bEnable)
{
#if defined(ENABLE_SIMPLE_GPU_TIMERS)
	if (m_pPipelineProfiler)
		m_pPipelineProfiler->SetEnabled(bEnable);
#endif
}

void CD3D9Renderer::LogShaderImportMiss(const CShader* pShader)
{
#if defined(SHADERS_SERIALIZING)
	stack_string requestLineStr, shaderList;

	if (!CRenderer::CV_r_shaderssubmitrequestline || !CRenderer::CV_r_shadersremotecompiler)
		return;

	gRenDev->m_cEF.CreateShaderExportRequestLine(pShader, requestLineStr);

#if CRY_PLATFORM_DURANGO
	shaderList = "ShaderList_Durango.txt";
#elif CRY_PLATFORM_ORBIS
	shaderList = "ShaderList_Orbis.txt";
#elif defined(OPENGL_ES) && DXGL_INPUT_GLSL
	shaderList = "ShaderList_GLES3.txt";
#elif defined(OPENGL) && DXGL_INPUT_GLSL
	shaderList = "ShaderList_GL4.txt";
#else
	shaderList = "ShaderList_PC.txt";
#endif

#ifdef SHADER_ASYNC_COMPILATION
	if (CRenderer::CV_r_shadersasynccompiling)
	{
		// Lazy init?
		if (!SShaderAsyncInfo::PendingList().m_Next)
		{
			SShaderAsyncInfo::PendingList().m_Next  = &SShaderAsyncInfo::PendingList();
			SShaderAsyncInfo::PendingList().m_Prev  = &SShaderAsyncInfo::PendingList();
			SShaderAsyncInfo::PendingListT().m_Next = &SShaderAsyncInfo::PendingListT();
			SShaderAsyncInfo::PendingListT().m_Prev = &SShaderAsyncInfo::PendingListT();
		}

		SShaderAsyncInfo* pAsyncRequest = new SShaderAsyncInfo;

		if (pAsyncRequest)
		{
			pAsyncRequest->m_RequestLine         = requestLineStr.c_str();
			pAsyncRequest->m_shaderList          = shaderList.c_str();
			pAsyncRequest->m_Text                = "";
			pAsyncRequest->m_bDeleteAfterRequest = true;
			CAsyncShaderTask::InsertPendingShader(pAsyncRequest);
		}
	}
	else
#endif
	{
		NRemoteCompiler::CShaderSrv::Instance().RequestLine(shaderList.c_str(), requestLineStr.c_str());
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::WaitForParticleBuffer()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);
	SRenderPipeline& rp = gRenDev->m_RP;

	rp.m_particleBuffer.WaitForFence();
}
//========================================================================================================
