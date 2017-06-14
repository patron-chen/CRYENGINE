// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct VrProjectionInfo;

class CSceneRenderPass
{
public:
	enum EPassFlags
	{
		ePassFlags_None                         = 0,
		ePassFlags_RenderNearest                = BIT(0),
		ePassFlags_ReverseDepth                 = BIT(1),
		ePassFlags_UseVrProjectionState         = BIT(2),
		ePassFlags_RequireVrProjectionConstants = BIT(3),
		ePassFlags_VrProjectionPass             = ePassFlags_UseVrProjectionState | ePassFlags_RequireVrProjectionConstants // for convenience
	};

	CSceneRenderPass();

	void SetupPassContext(uint32 stageID, uint32 stagePassID, EShaderTechniqueID technique, uint32 filter, ERenderListID renderList = EFSLIST_GENERAL, uint32 excludeFilter = 0, bool drawCompiledRenderObject = true);
	void SetPassResources(CDeviceResourceLayoutPtr pResourceLayout, CDeviceResourceSetPtr pPerPassResources);
	void SetRenderTargets(SDepthTexture* pDepthTarget, CTexture* pColorTarget0, CTexture* pColorTarget1 = NULL, CTexture* pColorTarget2 = NULL, CTexture* pColorTarget3 = NULL);
	void ExchangeRenderTarget(uint32 slot, CTexture* pNewColorTarget);
	void SetLabel(const char* label) { m_szLabel = label; }
	void SetFlags(EPassFlags flags)  { m_passFlags = flags; }
	void SetViewport(const D3DViewPort& viewport);
	void SetDepthBias(float constBias, float slopeBias, float biasClamp);

	void BeginExecution();
	void ExtractRenderTargetFormats(CDeviceGraphicsPSODesc& psoDesc);
	void EndExecution();

	void DrawRenderItems(CRenderView* pRenderView, ERenderListID list, int listStart = -1, int listEnd = -1, int profilingListID = -1);

	// Called from rendering backend (has to be threadsafe)
	void                PrepareRenderPassForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	void                BeginRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest, uint32 profilerSectionIndex, bool bIssueGPUTimestamp) const;
	void                EndRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest, uint32 profilerSectionIndex, bool bIssueGPUTimestamp) const;

	uint32              GetStageID()             const { return m_stageID; }
	uint32              GetPassID()              const { return m_passID; }
	uint32              GetNumRenderItemGroups() const { return m_numRenderItemGroups; }
	EPassFlags          GetFlags()               const { return m_passFlags; }
	const D3DViewPort&  GetViewport(bool n)      const { return m_viewPort[n]; }
	const D3DRectangle& GetScissorRect()         const { return m_scissorRect; }

protected:
	void DrawRenderItems_GP2(SGraphicsPipelinePassContext& passContext);

protected:
	SDepthTexture*           m_pDepthTarget;
	CTexture*                m_pColorTargets[4];
	D3DViewPort              m_viewPort[2];
	D3DRectangle             m_scissorRect;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CDeviceResourceSetPtr    m_pPerPassResources;
	const char*              m_szLabel;

	EShaderTechniqueID       m_technique;
	uint32                   m_stageID : 16;
	uint32                   m_passID  : 16;
	uint32                   m_batchFilter;
	uint32                   m_excludeFilter;
	EPassFlags               m_passFlags;
	ERenderListID            m_renderList;

	uint32                   m_numRenderItemGroups;
	uint32                   m_profilerSectionIndex;

	float                    m_depthConstBias;
	float                    m_depthSlopeBias;
	float                    m_depthBiasClamp;

protected:
	static int               s_recursionCounter;  // For asserting Begin/EndExecution get called on pass
};

DEFINE_ENUM_FLAG_OPERATORS(CSceneRenderPass::EPassFlags)
