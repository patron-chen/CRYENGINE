// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

// make the global Serialize() functions available for use in yasli serialization
using uqs::core::Serialize;

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// CQueryBase::SCtorContext
		//
		//===================================================================================

		CQueryBase::SCtorContext::SCtorContext(const CQueryID& _queryID, const char* _querierName, const HistoricQuerySharedPtr& _pOptionalHistoryToWriteTo, std::unique_ptr<CItemList>& _optionalResultingItemsFromPreviousChainedQuery)
			: queryID(_queryID)
			, querierName(_querierName)
			, pOptionalHistoryToWriteTo(_pOptionalHistoryToWriteTo)
			, optionalResultingItemsFromPreviousChainedQuery(_optionalResultingItemsFromPreviousChainedQuery)
		{}

		//===================================================================================
		//
		// CQueryBase::SGrantedAndUsedTime
		//
		//===================================================================================

		CQueryBase::SGrantedAndUsedTime::SGrantedAndUsedTime()
		{}

		CQueryBase::SGrantedAndUsedTime::SGrantedAndUsedTime(const CTimeValue& _granted, const CTimeValue& _used)
			: granted(_granted)
			, used(_used)
		{}

		void CQueryBase::SGrantedAndUsedTime::Serialize(Serialization::IArchive& ar)
		{
			ar(granted, "granted");
			ar(used, "used");
		}

		//===================================================================================
		//
		// CQueryBase::SStatistics
		//
		//===================================================================================

		CQueryBase::SStatistics::SStatistics()
			: querierName()
			, queryBlueprintName()
			, totalElapsedFrames(0)
			, totalConsumedTime()
			, grantedAndUsedTimePerFrame()

			, numGeneratedItems(0)
			, numRemainingItemsToInspect(0)
			, numItemsInFinalResultSet(0)
			, memoryUsedByGeneratedItems(0)
			, memoryUsedByItemsWorkingData(0)

			, elapsedFramesPerPhase()
			, elapsedTimePerPhase()
			, peakElapsedTimePerPhaseUpdate()
			, instantEvaluatorsRuns()
			, deferredEvaluatorsFullRuns()
			, deferredEvaluatorsAbortedRuns()
		{}

		void CQueryBase::SStatistics::Serialize(Serialization::IArchive& ar)
		{
			ar(querierName, "querierName");
			ar(queryBlueprintName, "queryBlueprintName");
			ar(totalElapsedFrames, "totalElapsedFrames");
			ar(totalConsumedTime, "totalConsumedTime");
			ar(grantedAndUsedTimePerFrame, "grantedAndUsedTimePerFrame");

			ar(numGeneratedItems, "numGeneratedItems");
			ar(numRemainingItemsToInspect, "numRemainingItemsToInspect");
			ar(numItemsInFinalResultSet, "numItemsInFinalResultSet");
			ar(memoryUsedByGeneratedItems, "memoryUsedByGeneratedItems");
			ar(memoryUsedByItemsWorkingData, "memoryUsedByItemsWorkingData");

			ar(elapsedFramesPerPhase, "elapsedFramesPerPhase");
			ar(elapsedTimePerPhase, "elapsedTimePerPhase");
			ar(peakElapsedTimePerPhaseUpdate, "peakElapsedTimePerPhaseUpdate");
			ar(instantEvaluatorsRuns, "instantEvaluatorsRuns");
			ar(deferredEvaluatorsFullRuns, "deferredEvaluatorsFullRuns");
			ar(deferredEvaluatorsAbortedRuns, "deferredEvaluatorsAbortedRuns");
		}

		//===================================================================================
		//
		// CQueryBase
		//
		//===================================================================================

		CQueryBase::CQueryBase(const SCtorContext& ctorContext, bool bRequiresSomeTimeBudgetForExecution)
			: m_querierName(ctorContext.querierName)
			, m_pHistory(ctorContext.pOptionalHistoryToWriteTo)
			, m_queryID(ctorContext.queryID)
			, m_totalElapsedFrames(0)
			, m_bRequiresSomeTimeBudgetForExecution(bRequiresSomeTimeBudgetForExecution)
			, m_pOptionalShuttledItems(std::move(ctorContext.optionalResultingItemsFromPreviousChainedQuery))
			, m_blackboard(m_globalParams, m_pOptionalShuttledItems.get(), ctorContext.pOptionalHistoryToWriteTo ? &ctorContext.pOptionalHistoryToWriteTo->GetDebugRenderWorld() : nullptr)
		{
			if (m_pHistory)
			{
				m_pHistory->OnQueryCreated();
			}
		}

		CQueryBase::~CQueryBase()
		{
			if (m_pHistory)
			{
				m_pHistory->OnQueryDestroyed();
			}
		}

		bool CQueryBase::RequiresSomeTimeBudgetForExecution() const
		{
			return m_bRequiresSomeTimeBudgetForExecution;
		}

		bool CQueryBase::InstantiateFromQueryBlueprint(const std::shared_ptr<const CQueryBlueprint>& queryBlueprint, const shared::IVariantDict& runtimeParams, shared::CUqsString& error)
		{
			assert(!m_queryBlueprint);	// we don't support recycling the query

			m_queryBlueprint = queryBlueprint;

			if (m_pHistory)
			{
				m_pHistory->OnQueryBlueprintInstantiationStarted(queryBlueprint->GetName());
			}

			//
			// ensure that the max. number of instant- and deferred-evaluators is not exceeded
			//

			{
				const size_t numInstantEvaluators = m_queryBlueprint->GetInstantEvaluatorBlueprints().size();
				if (numInstantEvaluators > UQS_MAX_EVALUATORS)
				{
					error.Format("Exceeded the maximum number of instant-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numInstantEvaluators);
					return false;
				}
			}

			{
				const size_t numDeferredEvaluators = m_queryBlueprint->GetDeferredEvaluatorBlueprints().size();
				if (numDeferredEvaluators > UQS_MAX_EVALUATORS)
				{
					error.Format("Exceeded the maximum number of deferred-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numDeferredEvaluators);
					return false;
				}
			}

			//
			// - ensure that all required runtime-params have been passed in and that their data types are correct
			// - note: we need to do this only for top-level queries (child queries will get recursively checked when their parent is about to start)
			//

			if (!m_queryBlueprint->GetParent())
			{
				if (!m_queryBlueprint->CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(runtimeParams, error))
				{
					return false;
				}
			}

			//
			// merge constant-params and runtime-params into global params
			//

			const shared::CVariantDict& constantParams = m_queryBlueprint->GetGlobalConstantParamsBlueprint().GetParams();
			constantParams.AddSelfToOtherAndReplace(m_globalParams);
			runtimeParams.AddSelfToOtherAndReplace(m_globalParams);

			//
			// allow the derived class to do further custom instantiation
			//

			return OnInstantiateFromQueryBlueprint(runtimeParams, error);
		}

		void CQueryBase::AddItemMonitor(client::ItemMonitorUniquePtr&& pItemMonitor)
		{
			assert(pItemMonitor);
			m_itemMonitors.push_back(std::move(pItemMonitor));
		}

		void CQueryBase::TransferAllItemMonitorsToOtherQuery(CQueryBase& receiver)
		{
			if (!m_itemMonitors.empty())
			{
				for (client::ItemMonitorUniquePtr& pItemMonitor : m_itemMonitors)
				{
					receiver.m_itemMonitors.push_back(std::move(pItemMonitor));
				}
				m_itemMonitors.clear();
			}
		}

		CQueryBase::EUpdateState CQueryBase::Update(const CTimeValue& timeBudget, shared::CUqsString& error)
		{
			++m_totalElapsedFrames;
			const CTimeValue startTime = gEnv->pTimer->GetAsyncTime();

			bool bCorruptionOccurred = false;

			//
			// check the item-monitors for having encountered a corruption *before* updating the query
			//

			if (!m_itemMonitors.empty())
			{
				for (const client::ItemMonitorUniquePtr& pItemMonitor : m_itemMonitors)
				{
					assert(pItemMonitor);

					const client::IItemMonitor::EHealthState healthState = pItemMonitor->UpdateAndCheckForCorruption(error);

					if (healthState == client::IItemMonitor::EHealthState::CorruptionOccurred)
					{
						bCorruptionOccurred = true;
						break;
					}
				}
			}

			//
			// allow the derived class to update itself if no item corruption has occurred yet
			//

			const EUpdateState state = bCorruptionOccurred ? EUpdateState::ExceptionOccurred : OnUpdate(timeBudget, error);

			//
			// finish timings
			//

			const CTimeValue timeSpent = gEnv->pTimer->GetAsyncTime() - startTime;
			m_totalConsumedTime += timeSpent;
			m_grantedAndUsedTimePerFrame.emplace_back(timeBudget, timeSpent);

			//
			// track a possible exception
			//

			if (state == EUpdateState::ExceptionOccurred && m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnExceptionOccurred(error.c_str(), stats);
			}

			//
			// track when the query finishes
			//

			if (state == EUpdateState::Finished && m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnQueryFinished(stats);
			}

			return state;
		}

		void CQueryBase::Cancel()
		{
			if (m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnQueryCanceled(stats);
			}

			//
			// allow the derived class to do some custom cancellation
			//

			OnCancel();
		}

		void CQueryBase::GetStatistics(SStatistics& out) const
		{
			out.querierName = m_querierName;

			if (m_queryBlueprint)
				out.queryBlueprintName = m_queryBlueprint->GetName();

			out.totalElapsedFrames = m_totalElapsedFrames;
			out.totalConsumedTime = m_totalConsumedTime;
			out.grantedAndUsedTimePerFrame = m_grantedAndUsedTimePerFrame;

			//
			// allow the derived class to add some more statistics
			//

			OnGetStatistics(out);
		}

		QueryResultSetUniquePtr CQueryBase::ClaimResultSet()
		{
			return std::move(m_pResultSet);
		}

	}
}
