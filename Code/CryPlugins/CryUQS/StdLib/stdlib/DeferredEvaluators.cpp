// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/IRenderer.h>
#include <CryPhysics/physinterface.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace stdlib
	{

		void CStdLibRegistration::InstantiateDeferredEvaluatorFactoriesForRegistration()
		{
			static const client::CDeferredEvaluatorFactory<CDeferredEvaluator_TestRaycast> deferredEvaluatorFactory_TestRaycast("std::TestRaycast");
		}

		//===================================================================================
		//
		// CDeferredEvaluator_TestRaycast::CRaycastRegulator
		//
		//===================================================================================

		CDeferredEvaluator_TestRaycast::CRaycastRegulator::CRaycastRegulator(int maxRequestsPerFrame)
			: m_maxRequestsPerFrame(maxRequestsPerFrame)
			, m_currentFrame(0)
			, m_numRequestsInCurrentFrame(0)
		{}

		bool CDeferredEvaluator_TestRaycast::CRaycastRegulator::RequestRaycast()
		{
			const int currentFrame = gEnv->pRenderer->GetFrameID();

			if (currentFrame != m_currentFrame)
			{
				m_currentFrame = currentFrame;
				m_numRequestsInCurrentFrame = 0;
			}

			return (++m_numRequestsInCurrentFrame <= m_maxRequestsPerFrame);
		}

		//===================================================================================
		//
		// CDeferredEvaluator_TestRaycast
		//
		//===================================================================================

		CDeferredEvaluator_TestRaycast::CRaycastRegulator CDeferredEvaluator_TestRaycast::s_regulator(12);  // allow up to 12 raycasts per frame

		CDeferredEvaluator_TestRaycast::CDeferredEvaluator_TestRaycast(const SParams& params)
			: m_params(params)
		{
			// nothing
		}

		client::IDeferredEvaluator::EUpdateStatus CDeferredEvaluator_TestRaycast::Update(const SUpdateContext& updateContext)
		{
			if (s_regulator.RequestRaycast())
			{
				//
				// perform the raycast
				//

				// these constants are exactly the same as the ones in <IVisionMap.h> [2016-08-16]
				const int raycastPierceability = 10;
				const int objectTypesToTestAgainst = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid;
				const int blockingHardColliders = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (raycastPierceability & rwi_pierceability_mask);
				const int blockingSoftColliders = (geom_colltype14 << rwi_colltype_bit);

				ray_hit singleHit;

				IPhysicalWorld::SRWIParams params;
				params.org = m_params.from;
				params.dir = m_params.to - m_params.from;
				params.objtypes = objectTypesToTestAgainst;
				params.flags = blockingHardColliders | blockingSoftColliders;
				params.hits = &singleHit;
				params.nMaxHits = 1;
				params.pSkipEnts = nullptr;
				params.nSkipEnts = 0;

				const bool bHitSomething = (gEnv->pPhysicalWorld->RayWorldIntersection(params) > 0);

				updateContext.evaluationResult.bDiscardItem = (bHitSomething == m_params.raycastShallSucceed);

				// persistent debug drawing of the ray
				IF_UNLIKELY(updateContext.blackboard.pDebugRenderWorld)
				{
					if (bHitSomething)
					{
						// yellow line: start pos -> impact
						// red line:    impact -> end pos
						updateContext.blackboard.pDebugRenderWorld->AddLine(m_params.from, singleHit.pt, Col_Yellow);
						updateContext.blackboard.pDebugRenderWorld->AddLine(singleHit.pt, m_params.to, Col_Red);
					}
					else
					{
						// green line: start pos -> end pos
						updateContext.blackboard.pDebugRenderWorld->AddLine(m_params.from, m_params.to, Col_Green);
					}
				}

				return EUpdateStatus::Finished;
			}
			else
			{
				//
				// we're still short on resources
				//

				// draw a white line to indicate that we're still waiting to start the raycast
				IF_UNLIKELY(updateContext.blackboard.pDebugRenderWorld)
				{
					updateContext.blackboard.pDebugRenderWorld->DrawLine(m_params.from, m_params.to, Col_White);
				}

				return EUpdateStatus::BusyButBlockedDueToResourceShortage;
			}
		}

	}
}
