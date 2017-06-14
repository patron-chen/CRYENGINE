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
		// CQuery_Regular::SPhaseUpdateContext
		//
		//===================================================================================

		CQuery_Regular::SPhaseUpdateContext::SPhaseUpdateContext(const CTimeValue& _timeLimit, shared::CUqsString& _error)
			: timeLimit(_timeLimit)
			, error(_error)
		{}

		//===================================================================================
		//
		// CQuery_Regular::SItemWorkingData
		//
		//===================================================================================

		CQuery_Regular::SItemWorkingData::SItemWorkingData()
			: indexInGeneratedItems(0)
			, accumulatedAndWeightedScoreSoFar(0.0f)
			, bitsFinishedInstantEvaluators(0)
			, bitsDiscardedByInstantEvaluators(0)
			, bitsExceptionByInstantEvaluatorsThemselves(0)
			, bitsExceptionByInstantEvaluatorFunctionCalls(0)
			, bitsWorkingDeferredEvaluators(0)
			, bitsFinishedDeferredEvaluators(0)
			, bitsAbortedDeferredEvaluators(0)
			, bitsDiscardedByDeferredEvaluators(0)
			, bitsExceptionByDeferredEvaluatorsThemselves(0)
			, bitsExceptionByDeferredEvaluatorFunctionCalls(0)
			, bDisqualifiedDueToBadScore(false)
		{}

		//===================================================================================
		//
		// CQuery_Regular::SInstantEvaluatorWithIndex
		//
		//===================================================================================

		CQuery_Regular::SInstantEvaluatorWithIndex::SInstantEvaluatorWithIndex(client::InstantEvaluatorUniquePtr _pInstantEvaluator, client::ParamsHolderUniquePtr _pParamsHolder, const client::IInputParameterRegistry* _pInputParameterRegistry, size_t _originalIndexInQueryBlueprint)
			: pInstantEvaluator(std::move(_pInstantEvaluator))
			, pParamsHolder(std::move(_pParamsHolder))
			, pInputParameterRegistry(_pInputParameterRegistry)
			, originalIndexInQueryBlueprint(_originalIndexInQueryBlueprint)
		{}

		CQuery_Regular::SInstantEvaluatorWithIndex::SInstantEvaluatorWithIndex(SInstantEvaluatorWithIndex&& other)
			: pInstantEvaluator(std::move(other.pInstantEvaluator))
			, pParamsHolder(std::move(other.pParamsHolder))
			, pInputParameterRegistry(other.pInputParameterRegistry)
			, originalIndexInQueryBlueprint(other.originalIndexInQueryBlueprint)
		{}

		CQuery_Regular::SInstantEvaluatorWithIndex& CQuery_Regular::SInstantEvaluatorWithIndex::operator=(SInstantEvaluatorWithIndex&& other)
		{
			if (this != &other)
			{
				pInstantEvaluator = std::move(other.pInstantEvaluator);
				pParamsHolder = std::move(other.pParamsHolder);
				pInputParameterRegistry = other.pInputParameterRegistry;
				originalIndexInQueryBlueprint = other.originalIndexInQueryBlueprint;
			}
			return *this;
		}

		//===================================================================================
		//
		// CQuery_Regular::SDeferredEvaluatorWithIndex
		//
		//===================================================================================

		CQuery_Regular::SDeferredEvaluatorWithIndex::SDeferredEvaluatorWithIndex(client::DeferredEvaluatorUniquePtr _pDeferredEvaluator, size_t _originalIndexInQueryBlueprint)
			: pDeferredEvaluator(std::move(_pDeferredEvaluator))
			, originalIndexInQueryBlueprint(_originalIndexInQueryBlueprint)
		{}

		CQuery_Regular::SDeferredEvaluatorWithIndex::SDeferredEvaluatorWithIndex(SDeferredEvaluatorWithIndex&& other)
			: pDeferredEvaluator(std::move(other.pDeferredEvaluator))
			, originalIndexInQueryBlueprint(other.originalIndexInQueryBlueprint)
		{}

		CQuery_Regular::SDeferredEvaluatorWithIndex& CQuery_Regular::SDeferredEvaluatorWithIndex::operator=(SDeferredEvaluatorWithIndex&& other)
		{
			if (this != &other)
			{
				pDeferredEvaluator = std::move(other.pDeferredEvaluator);
				originalIndexInQueryBlueprint = other.originalIndexInQueryBlueprint;
			}
			return *this;
		}

		//===================================================================================
		//
		// CQuery_Regular::SDeferredTask
		//
		//===================================================================================

		CQuery_Regular::SDeferredTask::SDeferredTask(SItemWorkingData* _pWorkingData)
			: pWorkingData(_pWorkingData)
			, deferredEvaluators()
			, status(0)
		{}

		//===================================================================================
		//
		// CQuery_Regular
		//
		//===================================================================================

		CQuery_Regular::CQuery_Regular(const SCtorContext& ctorContext)
			: CQueryBase(ctorContext, true)  // true = yes, we need some time budget from CQueryManager for some potentially complex computations
			, m_currentPhaseFn(&CQuery_Regular::Phase1_PrepareGenerationPhase)
			, m_maxCandidates(0)
			, m_remainingItemWorkingDatasIndexForCheapInstantEvaluators(0)
		{
			m_elapsedTimePerPhase.resize(1);              // 1st phase is already active, so keep track of the elapsed time of that from the beginning
			m_elapsedFramesPerPhase.resize(1);            // ditto for the elapsed frames
			m_peakElapsedTimePerPhaseUpdate.resize(1);    // ditto for the peak call duration
		}

		bool CQuery_Regular::OnInstantiateFromQueryBlueprint(const shared::IVariantDict& runtimeParams, shared::CUqsString& error)
		{
			// ensure that a generator exists in the query-blueprint (CQuery_Regular::Phase1_PrepareGenerationPhase() uses it)
			if (!m_queryBlueprint->GetGeneratorBlueprint())
			{
				error.Format("CQuery_Regular::OnInstantiateFromQueryBlueprint: the query-blueprint '%s' has no generator specified", m_queryBlueprint->GetName());
				return false;
			}

			return true;
		}

		CQuery_Regular::EUpdateState CQuery_Regular::OnUpdate(const CTimeValue& timeBudget, shared::CUqsString& error)
		{
			assert(m_currentPhaseFn);	// query has already finished before; cannot recycle a query

			++m_elapsedFramesPerPhase.back();

			const CTimeValue queryStartTime = gEnv->pTimer->GetAsyncTime();
			const CTimeValue queryTimeLimit = queryStartTime + timeBudget;
			const SPhaseUpdateContext phaseUpdateContext(queryTimeLimit, error);

			CTimeValue endTime;
			EUpdateState status = EUpdateState::StillRunning;

			do
			{
				const CTimeValue phaseStartTime = gEnv->pTimer->GetAsyncTime();

				// update the current phase (this might switch to the next phase)
				EPhaseStatus (CQuery_Regular::*oldPhaseFn)(const SPhaseUpdateContext&) = m_currentPhaseFn;
				const EPhaseStatus phaseStatus = (this->*m_currentPhaseFn)(phaseUpdateContext);

				// keep track of the elapsed time in the current phase
				endTime = gEnv->pTimer->GetAsyncTime();
				const CTimeValue elapsedTimeInPhaseUpdate = (endTime - phaseStartTime);
				m_elapsedTimePerPhase.back() += elapsedTimeInPhaseUpdate;
				m_peakElapsedTimePerPhaseUpdate.back() = std::max(m_peakElapsedTimePerPhaseUpdate.back(), elapsedTimeInPhaseUpdate);

				if (phaseStatus != EPhaseStatus::Ok)
				{
					// error occurred

					m_currentPhaseFn = nullptr;

					status = EUpdateState::ExceptionOccurred;
					break;
				}

				if (!m_currentPhaseFn)
				{
					// all phases have finished without an error

					status = EUpdateState::Finished;
					break;
				}

				// if we're still in the same phase, it means that the phase figured that it either ran out of time or that it just couldn't do any more work in the current frame
				// -> we prematurely interrupt the running query and continue from here on the next frame
				if (oldPhaseFn == m_currentPhaseFn)
				{
					break;
				}
				else
				{
					// a phase transition occurred -> keep track of the elapsed frames and time in the next phase from now on
					m_elapsedFramesPerPhase.resize(m_elapsedFramesPerPhase.size() + 1);
					m_elapsedTimePerPhase.resize(m_elapsedTimePerPhase.size() + 1);
					m_peakElapsedTimePerPhaseUpdate.resize(m_peakElapsedTimePerPhaseUpdate.size() + 1);
				}

			} while(endTime < queryTimeLimit);

			return status;
		}

		void CQuery_Regular::OnCancel()
		{
			// nothing special to do
		}

		void CQuery_Regular::OnGetStatistics(SStatistics& out) const
		{
			out.elapsedFramesPerPhase = m_elapsedFramesPerPhase;
			out.elapsedTimePerPhase = m_elapsedTimePerPhase;
			out.peakElapsedTimePerPhaseUpdate = m_peakElapsedTimePerPhaseUpdate;

			out.numGeneratedItems = m_generatedItems.GetItemCount();
			out.numRemainingItemsToInspect = m_remainingItemWorkingDatasToInspect.size();
			out.numItemsInFinalResultSet = m_candidates.size();

			// Instant-Evaluator runs
			{
				const size_t numInstantEvaluatorBPs = m_queryBlueprint->GetInstantEvaluatorBlueprints().size();

				out.instantEvaluatorsRuns.resize(numInstantEvaluatorBPs);

				// count how often each instant-evaluator has been run
				for (size_t evaluatorIndex = 0; evaluatorIndex < numInstantEvaluatorBPs; ++evaluatorIndex)
				{
					const evaluatorsBitfield_t bit = (evaluatorsBitfield_t)1 << evaluatorIndex;

					for (const SItemWorkingData& wd : m_itemWorkingDatas)
					{
						if (wd.bitsFinishedInstantEvaluators & bit)
						{
							++out.instantEvaluatorsRuns[evaluatorIndex];
						}
					}
				}
			}

			// Deferred-Evaluator runs
			{
				const size_t numDeferredEvaluatorBPs = m_queryBlueprint->GetDeferredEvaluatorBlueprints().size();

				out.deferredEvaluatorsFullRuns.resize(numDeferredEvaluatorBPs);
				out.deferredEvaluatorsAbortedRuns.resize(numDeferredEvaluatorBPs);

				for (size_t evaluatorIndex = 0; evaluatorIndex < numDeferredEvaluatorBPs; ++evaluatorIndex)
				{
					const evaluatorsBitfield_t bit = (evaluatorsBitfield_t)1 << evaluatorIndex;

					for (const SItemWorkingData& wd : m_itemWorkingDatas)
					{
						if (wd.bitsFinishedDeferredEvaluators & bit)
						{
							++out.deferredEvaluatorsFullRuns[evaluatorIndex];
						}
						else if (wd.bitsAbortedDeferredEvaluators & bit)
						{
							++out.deferredEvaluatorsAbortedRuns[evaluatorIndex];
						}
					}
				}
			}

			// memory usage
			out.memoryUsedByGeneratedItems = m_generatedItems.GetMemoryUsedSafe();
			out.memoryUsedByItemsWorkingData = m_itemWorkingDatas.size() * sizeof(SItemWorkingData);
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase1_PrepareGenerationPhase(const SPhaseUpdateContext& phaseUpdateContext)
		{
			//
			// instantiate the generator
			//

			const CGeneratorBlueprint* pGenBP = m_queryBlueprint->GetGeneratorBlueprint();
			assert(pGenBP);   // should have been detected by OnInstantiateFromQueryBlueprint() already
			m_pGenerator = pGenBP->InstantiateGenerator(m_blackboard, phaseUpdateContext.error);
			if (!m_pGenerator)
			{
				return EPhaseStatus::ExceptionOccurred;
			}

			//
			// provide the data type for all the items that will get generated in the next phase
			//

			m_generatedItems.SetItemFactory(m_pGenerator->GetItemFactory());

			// TODO: secondary generators

			m_currentPhaseFn = &CQuery_Regular::Phase2_GenerateItems;
			return EPhaseStatus::Ok;
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase2_GenerateItems(const SPhaseUpdateContext& phaseUpdateContext)
		{
			const client::IGenerator::SUpdateContext updateContext(m_queryID, m_blackboard, phaseUpdateContext.error);
			const client::IGenerator::EUpdateStatus generatorStatus = m_pGenerator->Update(updateContext, m_generatedItems);

			switch (generatorStatus)
			{
			case client::IGenerator::EUpdateStatus::StillGeneratingItems:
				// keep generating
				return EPhaseStatus::Ok;

			case client::IGenerator::EUpdateStatus::FinishedGeneratingItems:
				if (m_pHistory)
				{
					m_pHistory->OnGenerationPhaseFinished(m_generatedItems.GetItemCount(), *m_queryBlueprint);
				}
				m_currentPhaseFn = &CQuery_Regular::Phase3_CreateDebugRepresentationsOfGeneratedItemsIfHistoryLoggingIsDesired;
				return EPhaseStatus::Ok;

			case client::IGenerator::EUpdateStatus::ExceptionOccurred:
				return EPhaseStatus::ExceptionOccurred;

			default:
				assert(0);
				return EPhaseStatus::ExceptionOccurred;
			}
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase3_CreateDebugRepresentationsOfGeneratedItemsIfHistoryLoggingIsDesired(const SPhaseUpdateContext& phaseUpdateContext)
		{
			if (m_pHistory)
			{
				const client::IItemFactory& itemFactory = m_generatedItems.GetItemFactory();

				for (size_t i = 0, n = m_generatedItems.GetItemCount(); i < n; ++i)
				{
					const void* pItem = m_generatedItems.GetItemAtIndex(i);
					m_pHistory->CreateItemDebugProxyViaItemFactoryForItem(itemFactory, pItem, i);
				}
			}
			m_currentPhaseFn = &CQuery_Regular::Phase4_PrepareEvaluationPhase;
			return EPhaseStatus::Ok;
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase4_PrepareEvaluationPhase(const SPhaseUpdateContext& phaseUpdateContext)
		{
			//
			// make the generated items accessible to all functions in all evaluators from now on
			//

			m_pItemIterationContext.reset(new SItemIterationContext(m_generatedItems));
			m_blackboard.pItemIterationContext = m_pItemIterationContext.get();

			//
			// - instantiate the function-call-hierarchies of instant-evaluators and deferred-evaluators
			// - these hierarchies exist exactly once and are re-used for all items when evaluating them
			// - this works fine as functions are stateless, and can therefore be called as often as desired
			//

			// instant-evaluator blueprints
			{
				const std::vector<CInstantEvaluatorBlueprint*>& instantEvaluatorBlueprints = m_queryBlueprint->GetInstantEvaluatorBlueprints();
				const size_t numInstantEvaluators = instantEvaluatorBlueprints.size();
				m_functionCallHierarchyPerInstantEvalBP.reserve(numInstantEvaluators);

				for (size_t i = 0; i < numInstantEvaluators; ++i)
				{
					std::unique_ptr<CFunctionCallHierarchy> pFunctionCallHierarchy(new CFunctionCallHierarchy);
					if (!instantEvaluatorBlueprints[i]->InstantiateFunctionCallHierarchy(*pFunctionCallHierarchy, m_blackboard, phaseUpdateContext.error))
					{
						return EPhaseStatus::ExceptionOccurred;
					}
					m_functionCallHierarchyPerInstantEvalBP.push_back(std::move(pFunctionCallHierarchy));
				}
			}

			// deferred-evaluator blueprints
			{
				const std::vector<CDeferredEvaluatorBlueprint*>& deferredEvaluatorBlueprints = m_queryBlueprint->GetDeferredEvaluatorBlueprints();
				const size_t numDeferredEvaluators = deferredEvaluatorBlueprints.size();
				m_functionCallHierarchyPerDeferredEvalBP.reserve(numDeferredEvaluators);

				for (size_t i = 0; i < numDeferredEvaluators; ++i)
				{
					std::unique_ptr<CFunctionCallHierarchy> pFunctionCallHierarchy(new CFunctionCallHierarchy);
					if (!deferredEvaluatorBlueprints[i]->InstantiateFunctionCallHierarchy(*pFunctionCallHierarchy, m_blackboard, phaseUpdateContext.error))
					{
						return EPhaseStatus::ExceptionOccurred;
					}
					m_functionCallHierarchyPerDeferredEvalBP.push_back(std::move(pFunctionCallHierarchy));
				}
			}

			//
			// - instantiate all kinds of instant-evaluators (they will be re-used for all items as they're stateless)
			// - notice: this is *not* the case for deferred-evaluators, so we don't instantiate these here
			//

			{
				const std::vector<CInstantEvaluatorBlueprint*>& instantEvaluatorBlueprints = m_queryBlueprint->GetInstantEvaluatorBlueprints();
				const size_t numInstantEvaluators = instantEvaluatorBlueprints.size();

				//
				// - instantiate all instant-evaluators into temporary containers
				// - afterwards, we'll sort and keep them around in the following order:
				//    m_cheapInstantEvaluators:     [tester1, tester2, tester3, ..., scorer1, scorer2, scorer3, ...]
				//    m_expensiveInstantEvaluators: [tester1, tester2, tester3, ..., scorer1, scorer2, scorer3, ...]
				//
				// FIXME: the sorting could be done at the blueprint level already

				std::vector<SInstantEvaluatorWithIndex> cheapTesters;
				std::vector<SInstantEvaluatorWithIndex> cheapScorers;
				std::vector<SInstantEvaluatorWithIndex> expensiveTesters;
				std::vector<SInstantEvaluatorWithIndex> expensiveScorers;

				for (size_t i = 0; i < numInstantEvaluators; ++i)
				{
					client::IInstantEvaluatorFactory& instantEvaluatorFactory = instantEvaluatorBlueprints[i]->GetFactory();
					client::InstantEvaluatorUniquePtr pEval = instantEvaluatorFactory.CreateInstantEvaluator();
					client::ParamsHolderUniquePtr pParamsHolder = instantEvaluatorFactory.GetParamsHolderFactory().CreateParamsHolder();
					const client::IInputParameterRegistry* pInputParameterRegistry = &instantEvaluatorFactory.GetInputParameterRegistry();

					const client::IInstantEvaluatorFactory::ECostCategory costCategory = instantEvaluatorFactory.GetCostCategory();
					const client::IInstantEvaluatorFactory::EEvaluationModality evaluationModality = instantEvaluatorFactory.GetEvaluationModality();

					switch (costCategory)
					{
					case client::IInstantEvaluatorFactory::ECostCategory::Cheap:
						switch (evaluationModality)
						{
						case client::IInstantEvaluatorFactory::EEvaluationModality::Testing:
							cheapTesters.emplace_back(std::move(pEval), std::move(pParamsHolder), pInputParameterRegistry, i);
							break;

						case client::IInstantEvaluatorFactory::EEvaluationModality::Scoring:
							cheapScorers.emplace_back(std::move(pEval), std::move(pParamsHolder), pInputParameterRegistry, i);
							break;

						default:
							assert(0);
						}
						break;

					case client::IInstantEvaluatorFactory::ECostCategory::Expensive:
						switch (evaluationModality)
						{
						case client::IInstantEvaluatorFactory::EEvaluationModality::Testing:
							expensiveTesters.emplace_back(std::move(pEval), std::move(pParamsHolder), pInputParameterRegistry, i);
							break;

						case client::IInstantEvaluatorFactory::EEvaluationModality::Scoring:
							expensiveScorers.emplace_back(std::move(pEval), std::move(pParamsHolder), pInputParameterRegistry, i);
							break;

						default:
							assert(0);
						}
						break;

					default:
						assert(0);
					}
				}

				m_cheapInstantEvaluators.reserve(cheapTesters.size() + cheapScorers.size());
				m_expensiveInstantEvaluators.reserve(expensiveTesters.size() + expensiveScorers.size());

				// cheap testers
				for (SInstantEvaluatorWithIndex& eval : cheapTesters)
				{
					m_cheapInstantEvaluators.push_back(std::move(eval));
				}

				// cheap scorers
				for (SInstantEvaluatorWithIndex& eval : cheapScorers)
				{
					m_cheapInstantEvaluators.push_back(std::move(eval));
				}

				// expensive testers
				for (SInstantEvaluatorWithIndex& eval : expensiveTesters)
				{
					m_expensiveInstantEvaluators.push_back(std::move(eval));
				}

				// expensive scorers
				for (SInstantEvaluatorWithIndex& eval : expensiveScorers)
				{
					m_expensiveInstantEvaluators.push_back(std::move(eval));
				}
			}

			//
			// prepare the working-data for all items
			//

			const size_t numGeneratedItems = m_generatedItems.GetItemCount();

			m_itemWorkingDatas.resize(numGeneratedItems);
			m_remainingItemWorkingDatasToInspect.resize(numGeneratedItems);

			for (size_t i = 0; i < numGeneratedItems; ++i)
			{
				m_itemWorkingDatas[i].indexInGeneratedItems = i;
				m_remainingItemWorkingDatasToInspect[i] = &m_itemWorkingDatas[i];
			}

			//
			// - candidates for the final result set (only reserve memory here)
			// - notice: we might end up with less candidates than the desired limit after the query finishes
			//

			const int maxItemsToKeepInResultSet = m_queryBlueprint->GetMaxItemsToKeepInResultSet();
			m_maxCandidates = (maxItemsToKeepInResultSet < 1) ? numGeneratedItems : std::min((size_t)maxItemsToKeepInResultSet, numGeneratedItems);
			m_candidates.reserve(m_maxCandidates);

			m_currentPhaseFn = &CQuery_Regular::Phase5_RunCheapEvaluators;
			return EPhaseStatus::Ok;
		}

		void CQuery_Regular::RunInstantEvaluator(const SInstantEvaluatorWithIndex& instantEvaluatorToRun, SItemWorkingData& workingDataToWriteResultTo)
		{
			assert(workingDataToWriteResultTo.bitsDiscardedByInstantEvaluators == 0);
			assert(workingDataToWriteResultTo.bitsWorkingDeferredEvaluators == 0);
			assert(workingDataToWriteResultTo.bitsDiscardedByDeferredEvaluators == 0);
			assert(workingDataToWriteResultTo.bitsFinishedDeferredEvaluators == 0);
			assert(workingDataToWriteResultTo.bitsAbortedDeferredEvaluators == 0);
			assert(!workingDataToWriteResultTo.bDisqualifiedDueToBadScore);
			assert(workingDataToWriteResultTo.bitsExceptionByInstantEvaluatorFunctionCalls == 0);
			assert(workingDataToWriteResultTo.bitsExceptionByInstantEvaluatorsThemselves == 0);
			assert(workingDataToWriteResultTo.bitsExceptionByDeferredEvaluatorFunctionCalls == 0);
			assert(workingDataToWriteResultTo.bitsExceptionByDeferredEvaluatorsThemselves == 0);

			// associate given item with all primitives that the upcoming function and evaluators draw
			if (m_pHistory)
			{
				m_pHistory->GetDebugRenderWorld().AssociateAllUpcomingAddedPrimitivesWithItem(workingDataToWriteResultTo.indexInGeneratedItems);
			}

			// original index of the instant-evaluator as it appears in the blueprint
			const size_t instantEvaluatorIndex = instantEvaluatorToRun.originalIndexInQueryBlueprint;

			// re-use the parameters from possible previous calls (they'll get overwritten by the function calls below)
			void* pParams = instantEvaluatorToRun.pParamsHolder->GetParams();

			// fill the parameters for this instant-evaluator by making function calls
			{
				shared::CUqsString exceptionMessageFromFunctionCalls;
				bool bExceptionOccurredInFunctionCalls = false;
				const client::IFunction::SExecuteContext executeContext(workingDataToWriteResultTo.indexInGeneratedItems, m_blackboard, exceptionMessageFromFunctionCalls, bExceptionOccurredInFunctionCalls);
				const CFunctionCallHierarchy* pFunctionCalls = m_functionCallHierarchyPerInstantEvalBP[instantEvaluatorIndex].get();
				pFunctionCalls->ExecuteAll(executeContext, pParams, *instantEvaluatorToRun.pInputParameterRegistry);

				// bail out if an exception occurred during the function calls
				if (bExceptionOccurredInFunctionCalls)
				{
					if (m_pHistory)
					{
						m_pHistory->OnFunctionCallExceptionOccurredInInstantEvaluator(instantEvaluatorIndex, workingDataToWriteResultTo.indexInGeneratedItems, exceptionMessageFromFunctionCalls.c_str());
					}
					workingDataToWriteResultTo.bitsExceptionByInstantEvaluatorFunctionCalls |= (evaluatorsBitfield_t)1 << instantEvaluatorIndex;
					return;
				}
			}

			// run this instant-evaluator
			SItemEvaluationResult evaluationResult;
			shared::CUqsString exceptionMessageFromInstantEvaluatorHimself;
			const client::IInstantEvaluator* pInstantEvaluator = instantEvaluatorToRun.pInstantEvaluator.get();
			const client::IInstantEvaluator::SRunContext runContext(evaluationResult, m_blackboard, exceptionMessageFromInstantEvaluatorHimself);
			const client::IInstantEvaluator::ERunStatus status = pInstantEvaluator->Run(runContext, pParams);

			switch(status)
			{
			case client::IInstantEvaluator::ERunStatus::Finished:
				{
					// mark the evaluator as finished (this may only be done if the evaluator finished with whatever he was supposed to do without causing an exception)
					workingDataToWriteResultTo.bitsFinishedInstantEvaluators |= (evaluatorsBitfield_t)1 << instantEvaluatorIndex;

					if (evaluationResult.bDiscardItem)
					{
						// discard the item completely
						workingDataToWriteResultTo.bitsDiscardedByInstantEvaluators |= (evaluatorsBitfield_t)1 << instantEvaluatorIndex;

						if (m_pHistory)
						{
							m_pHistory->OnInstantEvaluatorDiscardedItem(instantEvaluatorIndex, workingDataToWriteResultTo.indexInGeneratedItems);
						}
					}
					else
					{
						// update the item's score so far
						const float weight = m_queryBlueprint->GetInstantEvaluatorBlueprints()[instantEvaluatorIndex]->GetWeight();	// FIXME: could cache the weights of all evaluators and spare the pointer access
						const float weightedScore = evaluationResult.score * weight;
						workingDataToWriteResultTo.accumulatedAndWeightedScoreSoFar += weightedScore;

						if (m_pHistory)
						{
							m_pHistory->OnInstantEvaluatorScoredItem(instantEvaluatorIndex, workingDataToWriteResultTo.indexInGeneratedItems, evaluationResult.score, weightedScore, workingDataToWriteResultTo.accumulatedAndWeightedScoreSoFar);
						}
					}
				}
				break;

			case client::IInstantEvaluator::ERunStatus::ExceptionOccurred:
				{
					// the instant-evaluator himself caused an exception (meaning that he did *not* fully run to its end, so doesn't count as "finished")
					workingDataToWriteResultTo.bitsExceptionByInstantEvaluatorsThemselves |= (evaluatorsBitfield_t)1 << instantEvaluatorIndex;

					if (m_pHistory)
					{
						m_pHistory->OnExceptionOccurredInInstantEvaluator(instantEvaluatorIndex, workingDataToWriteResultTo.indexInGeneratedItems, exceptionMessageFromInstantEvaluatorHimself.c_str());
					}
				}
				break;

			default:
				assert(0);
			}
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase5_RunCheapEvaluators(const SPhaseUpdateContext& phaseUpdateContext)
		{
			//
			// basically, the algorithm goes like this:
			//
			//     for each item:
			//     {
			//         for each cheap instant-evaluator:
			//         {
			//            run the evaluator on the item, discard the item if necessary, otherwise add to its score so far
			//         }
			//         check for time-budget
			//     }
			//

			while (m_remainingItemWorkingDatasIndexForCheapInstantEvaluators < m_remainingItemWorkingDatasToInspect.size())
			{
				SItemWorkingData* pWorkingData = m_remainingItemWorkingDatasToInspect[m_remainingItemWorkingDatasIndexForCheapInstantEvaluators];

				assert(pWorkingData->bitsDiscardedByInstantEvaluators == 0);
				assert(pWorkingData->bitsDiscardedByDeferredEvaluators == 0);
				assert(pWorkingData->bitsFinishedDeferredEvaluators == 0);
				assert(pWorkingData->bitsAbortedDeferredEvaluators == 0);
				assert(!pWorkingData->bDisqualifiedDueToBadScore);

				bool bDiscardedItem = false;

				for (auto it = m_cheapInstantEvaluators.cbegin(); it != m_cheapInstantEvaluators.cend() && !bDiscardedItem; ++it)
				{
					const SInstantEvaluatorWithIndex& ie = *it;

					RunInstantEvaluator(ie, *pWorkingData);

					if (pWorkingData->bitsDiscardedByInstantEvaluators != 0 || pWorkingData->bitsExceptionByInstantEvaluatorFunctionCalls != 0 || pWorkingData->bitsExceptionByInstantEvaluatorsThemselves != 0)
					{
						bDiscardedItem = true;
					}
				}

				if (bDiscardedItem)
				{
					m_remainingItemWorkingDatasToInspect.erase(m_remainingItemWorkingDatasToInspect.begin() + m_remainingItemWorkingDatasIndexForCheapInstantEvaluators);
				}
				else
				{
					++m_remainingItemWorkingDatasIndexForCheapInstantEvaluators;
				}

				if (phaseUpdateContext.timeLimit <= gEnv->pTimer->GetAsyncTime())
					break;
			}

			assert(m_remainingItemWorkingDatasIndexForCheapInstantEvaluators <= m_remainingItemWorkingDatasToInspect.size());

			// examined all items? -> proceed to next phase
			if (m_remainingItemWorkingDatasIndexForCheapInstantEvaluators == m_remainingItemWorkingDatasToInspect.size())
			{
				m_currentPhaseFn = &CQuery_Regular::Phase6_SortByScoreSoFar;
			}

			return EPhaseStatus::Ok;
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase6_SortByScoreSoFar(const SPhaseUpdateContext& phaseUpdateContext)
		{
			// sort the remaining items such that the ones with higher scores come first
			auto sorter = [](const SItemWorkingData* lhs, const SItemWorkingData* rhs)
			{
				return lhs->accumulatedAndWeightedScoreSoFar > rhs->accumulatedAndWeightedScoreSoFar;
			};
			std::sort(m_remainingItemWorkingDatasToInspect.begin(), m_remainingItemWorkingDatasToInspect.end(), sorter);

			m_currentPhaseFn = &CQuery_Regular::Phase7_RunExpensiveEvaluators;
			return EPhaseStatus::Ok;
		}

		void CQuery_Regular::UpdateDeferredTasks()
		{
			for (auto it = m_deferredTasks.begin(); it != m_deferredTasks.end(); )
			{
				SDeferredTask& taskToUpdate = *it;

				UpdateDeferredTask(taskToUpdate);

				// finished with all its deferred-evaluators? -> finalize the item and remove the task
				if (taskToUpdate.deferredEvaluators.empty())
				{
					FinalizeItemAfterDeferredEvaluation(*taskToUpdate.pWorkingData);
					it = m_deferredTasks.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		void CQuery_Regular::UpdateDeferredTask(SDeferredTask& taskToUpdate)
		{
			assert(taskToUpdate.pWorkingData->bitsDiscardedByInstantEvaluators == 0);
			assert(taskToUpdate.pWorkingData->bitsDiscardedByDeferredEvaluators == 0);
			assert(taskToUpdate.pWorkingData->bitsWorkingDeferredEvaluators != 0);
			assert(taskToUpdate.pWorkingData->bitsFinishedDeferredEvaluators == 0);
			assert(!taskToUpdate.pWorkingData->bDisqualifiedDueToBadScore);
			assert(taskToUpdate.pWorkingData->bitsExceptionByInstantEvaluatorFunctionCalls == 0);
			assert(taskToUpdate.pWorkingData->bitsExceptionByInstantEvaluatorsThemselves == 0);
			assert(taskToUpdate.pWorkingData->bitsExceptionByDeferredEvaluatorFunctionCalls == 0);
			assert(taskToUpdate.pWorkingData->bitsExceptionByDeferredEvaluatorsThemselves == 0);

			taskToUpdate.status = 0;

			// associate given item with all primitives that the upcoming functions and evaluators draw
			if (m_pHistory)
			{
				m_pHistory->GetDebugRenderWorld().AssociateAllUpcomingAddedPrimitivesWithItem(taskToUpdate.pWorkingData->indexInGeneratedItems);
			}

			bool bDiscardedItem = false;
			stack_string reasonForAbortingTheRemainingDeferredEvaluators;

			for (auto it = taskToUpdate.deferredEvaluators.begin(); it != taskToUpdate.deferredEvaluators.end() && !bDiscardedItem; )
			{
				//
				// update this deferred-evaluator
				//

				SDeferredEvaluatorWithIndex& de = *it;
				SItemEvaluationResult evaluationResult;
				shared::CUqsString exceptionMessageFromDeferredEvaluatorHimself;
				const client::IDeferredEvaluator::SUpdateContext evaluatorUpdateContext(evaluationResult, m_blackboard, exceptionMessageFromDeferredEvaluatorHimself);
				const client::IDeferredEvaluator::EUpdateStatus evaluatorStatus = de.pDeferredEvaluator->Update(evaluatorUpdateContext);

				switch (evaluatorStatus)
				{
				case client::IDeferredEvaluator::EUpdateStatus::BusyWaitingForExternalSchedulerFeedback:
					taskToUpdate.status |= SDeferredTask::statusFlag_atLeastOneEvaluatorIsWaitingForExternalResourceInCurrentFrame;
					++it;
					break;

				case client::IDeferredEvaluator::EUpdateStatus::BusyButBlockedDueToResourceShortage:
					taskToUpdate.status |= SDeferredTask::statusFlag_atLeastOneEvaluatorIsShortOnResources;
					++it;
					break;

				case client::IDeferredEvaluator::EUpdateStatus::BusyDoingTimeSlicedWork:
					taskToUpdate.status |= SDeferredTask::statusFlag_atLeastOneEvaluatorIsDoingTimeSlicedWork;
					++it;
					break;

				case client::IDeferredEvaluator::EUpdateStatus::Finished:
					{
						const evaluatorsBitfield_t myOwnBit = (evaluatorsBitfield_t)1 << de.originalIndexInQueryBlueprint;

						// take off our working mark
						taskToUpdate.pWorkingData->bitsWorkingDeferredEvaluators &= ~myOwnBit;

						// leave our finished mark
						taskToUpdate.pWorkingData->bitsFinishedDeferredEvaluators |= myOwnBit;

						if (evaluationResult.bDiscardItem)
						{
							taskToUpdate.pWorkingData->bitsDiscardedByDeferredEvaluators |= myOwnBit;
							bDiscardedItem = true;
							reasonForAbortingTheRemainingDeferredEvaluators = "item got discarded by another deferred-evaluator in the meantime";

							if (m_pHistory)
							{
								m_pHistory->OnDeferredEvaluatorDiscardedItem(de.originalIndexInQueryBlueprint, taskToUpdate.pWorkingData->indexInGeneratedItems);
							}
						}
						else
						{
							const float weight = m_queryBlueprint->GetDeferredEvaluatorBlueprints()[de.originalIndexInQueryBlueprint]->GetWeight();	// FIXME: could cache the weights of all evaluators and spare the pointer access
							const float weightedScore = evaluationResult.score * weight;
							taskToUpdate.pWorkingData->accumulatedAndWeightedScoreSoFar += weightedScore;

							if (m_pHistory)
							{
								m_pHistory->OnDeferredEvaluatorScoredItem(de.originalIndexInQueryBlueprint, taskToUpdate.pWorkingData->indexInGeneratedItems, evaluationResult.score, weightedScore, taskToUpdate.pWorkingData->accumulatedAndWeightedScoreSoFar);
							}
						}

						it = taskToUpdate.deferredEvaluators.erase(it);
					}
					break;

				case client::IDeferredEvaluator::EUpdateStatus::ExceptionOccurred:
					{
						const evaluatorsBitfield_t myOwnBit = (evaluatorsBitfield_t)1 << de.originalIndexInQueryBlueprint;

						// take off our working mark
						taskToUpdate.pWorkingData->bitsWorkingDeferredEvaluators &= ~myOwnBit;

						// mark the item as having encountered an exception by this deferred-evaluator
						taskToUpdate.pWorkingData->bitsExceptionByDeferredEvaluatorsThemselves |= myOwnBit;

						bDiscardedItem = true;
						reasonForAbortingTheRemainingDeferredEvaluators = exceptionMessageFromDeferredEvaluatorHimself.c_str();

						if (m_pHistory)
						{
							m_pHistory->OnExceptionOccurredInDeferredEvaluator(de.originalIndexInQueryBlueprint, taskToUpdate.pWorkingData->indexInGeneratedItems, exceptionMessageFromDeferredEvaluatorHimself.c_str());
						}

						it = taskToUpdate.deferredEvaluators.erase(it);
					}
					break;

				default:
					assert(0);
				}
			}

			//
			// if the item got discarded, then abort all remaining deferred-evaluators to save processing power
			//

			if (bDiscardedItem)
			{
				AbortDeferredTask(taskToUpdate, reasonForAbortingTheRemainingDeferredEvaluators.c_str());
			}
		}

		void CQuery_Regular::AbortDeferredTask(SDeferredTask& taskToAbort, const char* reasonForAbort)
		{
			// abort all remaining deferred-evaluators and mark them as "aborted"
			while (!taskToAbort.deferredEvaluators.empty())
			{
				const size_t originalIndexInQueryBlueprint = taskToAbort.deferredEvaluators.front().originalIndexInQueryBlueprint;
				const evaluatorsBitfield_t myOwnBit = (evaluatorsBitfield_t)1 << originalIndexInQueryBlueprint;

				// the deferred-evaluator must have been working already before (to be consistent)
				assert(taskToAbort.pWorkingData->bitsWorkingDeferredEvaluators & myOwnBit);

				// prematurely destroy it (this can save processing time on those doing time-sliced work)
				taskToAbort.deferredEvaluators.pop_front();

				// mark as "aborted" (this information is only used for debugging purpose to ask questions like "which evaluator was started, but prematurely aborted before it finished")
				taskToAbort.pWorkingData->bitsAbortedDeferredEvaluators |= myOwnBit;

				// take off its working mark
				taskToAbort.pWorkingData->bitsWorkingDeferredEvaluators &= ~myOwnBit;

				if (m_pHistory)
				{
					m_pHistory->OnDeferredEvaluatorGotAborted(originalIndexInQueryBlueprint, taskToAbort.pWorkingData->indexInGeneratedItems, reasonForAbort);
				}
			}
		}

		bool CQuery_Regular::CanItemStillBeatTheWorstCandidate(const SItemWorkingData& itemToCheck) const
		{
			// the list of final candidates must be already full
			assert(m_candidates.size() == m_maxCandidates);

			// given item must not have encountered any kind of exception
			assert(itemToCheck.bitsExceptionByInstantEvaluatorFunctionCalls == 0);
			assert(itemToCheck.bitsExceptionByInstantEvaluatorsThemselves == 0);
			assert(itemToCheck.bitsExceptionByDeferredEvaluatorFunctionCalls == 0);
			assert(itemToCheck.bitsExceptionByDeferredEvaluatorsThemselves == 0);

			// - assume that all cheap instant-evaluators have already run and didn't discard the item, as we don't check for them here
			// - in other words: we may only get called from the expensive evaluators phase

			float bestPossibleScore = itemToCheck.accumulatedAndWeightedScoreSoFar;

			// accumulate best possible score from expensive instant-evaluators that haven't run yet
			{
				const std::vector<CInstantEvaluatorBlueprint*>& instantEvaluatorBlueprints = m_queryBlueprint->GetInstantEvaluatorBlueprints();

				for(const SInstantEvaluatorWithIndex& ie : m_expensiveInstantEvaluators)
				{
					// skip this instant-evaluator if it has already run (its score is already tracked)
					if (itemToCheck.bitsFinishedInstantEvaluators & ((evaluatorsBitfield_t)1 << ie.originalIndexInQueryBlueprint))
						continue;

					// TODO: cache the weights upfront
					bestPossibleScore += instantEvaluatorBlueprints[ie.originalIndexInQueryBlueprint]->GetWeight();
				}
			}

			// accumulate best possible score from deferred-evaluators
			{
				const std::vector<CDeferredEvaluatorBlueprint*>& deferredEvaluatorBlueprints = m_queryBlueprint->GetDeferredEvaluatorBlueprints();
				const size_t numDeferredEvaluatorBlueprints = deferredEvaluatorBlueprints.size();

				// figure out the best possible score that given item can still achieve
				for (size_t blueprintIndex = 0; blueprintIndex < numDeferredEvaluatorBlueprints; ++blueprintIndex)
				{
					// assume that no deferred-evaluator has run or attempted to run yet (check for exception has been done above already)
					assert((itemToCheck.bitsFinishedDeferredEvaluators & ((evaluatorsBitfield_t)1 << blueprintIndex)) == 0);
					assert((itemToCheck.bitsWorkingDeferredEvaluators & ((evaluatorsBitfield_t)1 << blueprintIndex)) == 0);
					assert((itemToCheck.bitsAbortedDeferredEvaluators & ((evaluatorsBitfield_t)1 << blueprintIndex)) == 0);

					// TODO: cache the weights upfront
					bestPossibleScore += deferredEvaluatorBlueprints[blueprintIndex]->GetWeight();
				}
			}

			const float worstScoreAmongCandidates = m_candidates.back()->accumulatedAndWeightedScoreSoFar;
			return (bestPossibleScore > worstScoreAmongCandidates);
		}

		void CQuery_Regular::AddItemToResultSetOrDisqualifyIt(SItemWorkingData& itemThatJustFinishedAndSurvivedAllEvaluators)
		{
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsDiscardedByInstantEvaluators == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsDiscardedByDeferredEvaluators == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsWorkingDeferredEvaluators == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsAbortedDeferredEvaluators == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsExceptionByInstantEvaluatorFunctionCalls == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsExceptionByInstantEvaluatorsThemselves == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsExceptionByDeferredEvaluatorFunctionCalls == 0);
			assert(itemThatJustFinishedAndSurvivedAllEvaluators.bitsExceptionByDeferredEvaluatorsThemselves == 0);

			bool bKeepItemAsCandidate = true;

			// if the candidate list is already full, see if the item is better than the worst candidate
			if (m_candidates.size() == m_maxCandidates)
			{
				SItemWorkingData* pWorstCandidate = m_candidates.back();

				if (itemThatJustFinishedAndSurvivedAllEvaluators.accumulatedAndWeightedScoreSoFar > pWorstCandidate->accumulatedAndWeightedScoreSoFar)
				{
					// make room for our item (discard the worst one, but before we do so, mark that item as having been disqualified due to a bad score)
					pWorstCandidate->bDisqualifiedDueToBadScore = true;
					m_candidates.pop_back();

					if (m_pHistory)
					{
						m_pHistory->OnItemGotDisqualifiedDueToBadScore(pWorstCandidate->indexInGeneratedItems);
					}
				}
				else
				{
					// item is not good enough
					bKeepItemAsCandidate = false;
				}
			}

			if (bKeepItemAsCandidate)
			{
				// insert the item into its correct position among the candidates (high scores appear at the beginning, low scores at the end)
				std::vector<SItemWorkingData*>::iterator insertPos;
				for (insertPos = m_candidates.begin(); insertPos != m_candidates.end(); ++insertPos)
				{
					if (itemThatJustFinishedAndSurvivedAllEvaluators.accumulatedAndWeightedScoreSoFar > (*insertPos)->accumulatedAndWeightedScoreSoFar)
						break;
				}
				m_candidates.insert(insertPos, &itemThatJustFinishedAndSurvivedAllEvaluators);
			}
			else
			{
				itemThatJustFinishedAndSurvivedAllEvaluators.bDisqualifiedDueToBadScore = true;

				if (m_pHistory)
				{
					m_pHistory->OnItemGotDisqualifiedDueToBadScore(itemThatJustFinishedAndSurvivedAllEvaluators.indexInGeneratedItems);
				}
			}
		}

		void CQuery_Regular::FinalizeItemAfterDeferredEvaluation(SItemWorkingData& itemToFinalize)
		{
			//
			// check the status of all deferred-evaluators on given item and eventually add that item to the final result set
			//

			// none of the deferred-evaluators shall be working on the item anymore
			assert(itemToFinalize.bitsWorkingDeferredEvaluators == 0);

			// items that already got discarded by an instant-evaluator shouldn't even make it until here
			assert(itemToFinalize.bitsDiscardedByInstantEvaluators == 0);

			// only we (this method) shall figure out if an item can no longer make it into the final result set
			assert(!itemToFinalize.bDisqualifiedDueToBadScore);

			// items that encountered any kind of exception in the context of past instant-evaluators shouldn't have made it until here
			assert(itemToFinalize.bitsExceptionByInstantEvaluatorFunctionCalls == 0);
			assert(itemToFinalize.bitsExceptionByInstantEvaluatorsThemselves == 0);

			// did this item encounter an exception in the context of deferred-evaluators?
			if (itemToFinalize.bitsExceptionByDeferredEvaluatorFunctionCalls != 0 || itemToFinalize.bitsExceptionByDeferredEvaluatorsThemselves != 0)
			{
				return;
			}

			// did this item get discarded by a deferred-evaluator?
			if (itemToFinalize.bitsDiscardedByDeferredEvaluators != 0)
			{
				return;
			}

			// (notice: no need to also check for itemToFinalize.bitsAbortedDeferredEvaluators, since some deferred-evaluator would have already marked the item as discarded or as exception)

			//
			// if we're here, then the item survived all evaluators and has a final score now
			//

			// all deferred-evaluators must have had their say on the item
			assert(itemToFinalize.bitsFinishedDeferredEvaluators == ((evaluatorsBitfield_t)1 << m_queryBlueprint->GetDeferredEvaluatorBlueprints().size()) - 1);

			AddItemToResultSetOrDisqualifyIt(itemToFinalize);
		}

		void CQuery_Regular::StartMoreEvaluatorsOnRemainingItems(const SPhaseUpdateContext& phaseUpdateContext)
		{
			//
			// still more items to inspect?
			//

			while (!m_remainingItemWorkingDatasToInspect.empty())
			{
				assert(m_candidates.size() <= m_maxCandidates);

				//
				// - check for whether there's still room in the potential result set
				// - notice that we allow one more item to get evaluated even if the capacity has already been exhausted
				//   => it's this particular item that will tell us whether any of the remaining items are still promising (or whether we can cut them all off)
				//

				const size_t remainingCapacityInResultSet = m_maxCandidates - m_candidates.size();
				const bool bRemainingCapacityAllowsToStartMoreEvaluators = (m_deferredTasks.size() < remainingCapacityInResultSet + 1);

				if (!bRemainingCapacityAllowsToStartMoreEvaluators)
					break;

				auto itWorkingDataToInspectNext = m_remainingItemWorkingDatasToInspect.begin();
				SItemWorkingData* pWorkingDataToInspectNext = *itWorkingDataToInspectNext;

				assert(pWorkingDataToInspectNext->bitsDiscardedByInstantEvaluators == 0);
				assert(pWorkingDataToInspectNext->bitsDiscardedByDeferredEvaluators == 0);
				assert(pWorkingDataToInspectNext->bitsWorkingDeferredEvaluators == 0);
				assert(pWorkingDataToInspectNext->bitsFinishedDeferredEvaluators == 0);
				assert(pWorkingDataToInspectNext->bitsAbortedDeferredEvaluators == 0);
				assert(!pWorkingDataToInspectNext->bDisqualifiedDueToBadScore);
				assert(pWorkingDataToInspectNext->bitsExceptionByInstantEvaluatorFunctionCalls == 0);
				assert(pWorkingDataToInspectNext->bitsExceptionByInstantEvaluatorsThemselves == 0);
				assert(pWorkingDataToInspectNext->bitsExceptionByDeferredEvaluatorFunctionCalls == 0);
				assert(pWorkingDataToInspectNext->bitsExceptionByDeferredEvaluatorsThemselves == 0);

				//
				// if the potential result set is already full, check for whether the upcoming item (and all remaining ones) can still make it into the result set
				//

				if (remainingCapacityInResultSet == 0)
				{
					if (!CanItemStillBeatTheWorstCandidate(*pWorkingDataToInspectNext))
					{
						//
						// none of the remaining items will ever make it into the result set anymore, so disqualify all of them
						//

						for (SItemWorkingData* pWD : m_remainingItemWorkingDatasToInspect)
						{
							pWD->bDisqualifiedDueToBadScore = true;

							if (m_pHistory)
							{
								m_pHistory->OnItemGotDisqualifiedDueToBadScore(pWD->indexInGeneratedItems);
							}
						}

						m_remainingItemWorkingDatasToInspect.clear();
						return;
					}
				}

				//
				// run all expensive instant-evaluators on this item
				//

				bool bDiscardedItem = false;

				for (auto it = m_expensiveInstantEvaluators.cbegin(); it != m_expensiveInstantEvaluators.cend() && !bDiscardedItem; ++it)
				{
					const SInstantEvaluatorWithIndex& ie = *it;

					RunInstantEvaluator(ie, *pWorkingDataToInspectNext);

					if (pWorkingDataToInspectNext->bitsDiscardedByInstantEvaluators != 0 ||
						pWorkingDataToInspectNext->bitsExceptionByInstantEvaluatorFunctionCalls != 0 ||
						pWorkingDataToInspectNext->bitsExceptionByInstantEvaluatorsThemselves != 0)
					{
						bDiscardedItem = true;
					}
				}

				//
				// if the item survived the expensive instant-evaluators, then schedule all deferred-evaluators as a single deferred task on it
				//

				if (!bDiscardedItem)
				{
					//
					// - extra check to see if the query-blueprint actually comes with some deferred-evaluators
					// - if it doesn't then there's no need to set up a costly deferred task that will ultimately be a NOP
					//

					const size_t numDeferredEvaluatorBlueprints = m_queryBlueprint->GetDeferredEvaluatorBlueprints().size();

					if (numDeferredEvaluatorBlueprints > 0)
					{
						// can the item still make it into the final result set?
						if (remainingCapacityInResultSet > 0 || CanItemStillBeatTheWorstCandidate(*pWorkingDataToInspectNext))
						{
							// yep => schedule a deferred task on it
							if (SDeferredTask* pNewTask = StartDeferredTask(pWorkingDataToInspectNext))
							{
								assert(&m_deferredTasks.back() == pNewTask);

								UpdateDeferredTask(*pNewTask);

								//
								// finished with all its deferred-evaluators already? -> finalize the item and remove the task
								//
								// notice: it may seem strange that deferred-evaluators might finish within the same frame that they've been started in, but it's perfectly legal; 
								// e. g. given a list of positions and trying to find the position with the shortest path distance to it, we can easily reject positions with an 
								// euclidean distance longer than the shortest path so far.
								//

								if (pNewTask->deferredEvaluators.empty())
								{
									FinalizeItemAfterDeferredEvaluation(*pNewTask->pWorkingData);
									m_deferredTasks.pop_back();
								}
							}
						}
						else
						{
							// the potential result set is already full and the item can never ever beat the worst candidate anymore
							// => disqualify the item due to a bad score
							pWorkingDataToInspectNext->bDisqualifiedDueToBadScore = true;
							if (m_pHistory)
							{
								m_pHistory->OnItemGotDisqualifiedDueToBadScore(pWorkingDataToInspectNext->indexInGeneratedItems);
							}
						}
					}
					else
					{
						// the query-blueprint has no deferred-evaluators in it, so finalize the item here already
						AddItemToResultSetOrDisqualifyIt(*pWorkingDataToInspectNext);
					}
				}

				m_remainingItemWorkingDatasToInspect.erase(itWorkingDataToInspectNext);

				if (phaseUpdateContext.timeLimit <= gEnv->pTimer->GetAsyncTime())
					break;
			}
		}

		CQuery_Regular::SDeferredTask* CQuery_Regular::StartDeferredTask(SItemWorkingData* pWorkingDataToInspectNext)
		{
			const std::vector<CDeferredEvaluatorBlueprint*>& deferredEvaluatorBlueprints = m_queryBlueprint->GetDeferredEvaluatorBlueprints();
			const size_t numDeferredEvaluatorBlueprints = deferredEvaluatorBlueprints.size();

			// create a new task (we'll fill it with deferred-evaluators below)
			m_deferredTasks.emplace_back(pWorkingDataToInspectNext);
			SDeferredTask& freshlyCreatedTask = m_deferredTasks.back();

			// function execution context (used for all function calls)
			shared::CUqsString exceptionMessageFromFunctionCalls;
			bool bExceptionOccurredInFunctionCalls = false;
			const client::IFunction::SExecuteContext executeContext(pWorkingDataToInspectNext->indexInGeneratedItems, m_blackboard, exceptionMessageFromFunctionCalls, bExceptionOccurredInFunctionCalls);

			// fill the new task with all deferred-evaluators
			for (size_t deferredEvaluatorBlueprintIndex = 0; deferredEvaluatorBlueprintIndex < numDeferredEvaluatorBlueprints; ++deferredEvaluatorBlueprintIndex)
			{
				client::IDeferredEvaluatorFactory& deferredEvaluatorFactory = deferredEvaluatorBlueprints[deferredEvaluatorBlueprintIndex]->GetFactory();

				// create input parameters (they will get filled by the function calls below)
				client::ParamsHolderUniquePtr pParamsHolder = deferredEvaluatorFactory.GetParamsHolderFactory().CreateParamsHolder();
				void* pParams = pParamsHolder->GetParams();

				// fill the parameters for this deferred-evaluator by making function calls
				const CFunctionCallHierarchy* pFunctionCalls = m_functionCallHierarchyPerDeferredEvalBP[deferredEvaluatorBlueprintIndex].get();
				pFunctionCalls->ExecuteAll(executeContext, pParams, deferredEvaluatorFactory.GetInputParameterRegistry());

				// prematurely abort and delete the task if any of its functions caused an exception
				if (bExceptionOccurredInFunctionCalls)
				{
					AbortDeferredTask(freshlyCreatedTask, exceptionMessageFromFunctionCalls.c_str());
					pWorkingDataToInspectNext->bitsExceptionByDeferredEvaluatorFunctionCalls |= (evaluatorsBitfield_t)1 << deferredEvaluatorBlueprintIndex;
					m_deferredTasks.pop_back();
					return nullptr;
				}

				// instantiate a new DE and add it to the task
				client::DeferredEvaluatorUniquePtr pDeferredEvaluator = deferredEvaluatorFactory.CreateDeferredEvaluator(pParams);
				freshlyCreatedTask.deferredEvaluators.emplace_back(std::move(pDeferredEvaluator), deferredEvaluatorBlueprintIndex);

				// mark the item as being worked on by this DE
				pWorkingDataToInspectNext->bitsWorkingDeferredEvaluators |= (evaluatorsBitfield_t)1 << deferredEvaluatorBlueprintIndex;

				if (m_pHistory)
				{
					m_pHistory->OnDeferredEvaluatorStartedRunningOnItem(deferredEvaluatorBlueprintIndex, pWorkingDataToInspectNext->indexInGeneratedItems);
				}
			}

			return &freshlyCreatedTask;
		}

		CQuery_Regular::EPhaseStatus CQuery_Regular::Phase7_RunExpensiveEvaluators(const SPhaseUpdateContext& phaseUpdateContext)
		{
			//
			// update the deferred tasks (each task is working on one item)
			//
			// TODO: if one of the deferred-tasks contains a deferred-evaluator that reports doing time-sliced-work, then update that task all over again until exceeding the time-budget (not just once per frame) so that we get an optimal use of the granted time

			UpdateDeferredTasks();

			//
			// start more evaluators
			//

			StartMoreEvaluatorsOnRemainingItems(phaseUpdateContext);

			//
			// no more items to inspect? => we have the final result set
			//

			if (m_remainingItemWorkingDatasToInspect.empty() && m_deferredTasks.empty())
			{
				m_currentPhaseFn = nullptr;

				// prepare the result set for getting inspected by the caller
				CQueryResultSet* pResultSet = new CQueryResultSet;
				pResultSet->SetItemFactoryAndCreateItems(m_generatedItems.GetItemFactory(), m_candidates.size());
				for (size_t i = 0, n = m_candidates.size(); i < n; ++i)
				{
					const SItemWorkingData* pWD = m_candidates[i];
					const float score = pWD->accumulatedAndWeightedScoreSoFar;
					const void* pItem = m_generatedItems.GetItemAtIndex(pWD->indexInGeneratedItems);
					pResultSet->SetItemAndScore(i, pItem, score);
				}
				m_pResultSet.reset(pResultSet);

#ifdef UQS_CHECK_PROPER_CLEANUP_ONCE_ALL_ITEMS_ARE_INSPECTED
				assert(m_deferredTasks.empty());

				// check the working data of all items for consistency
				for (const SItemWorkingData& wd : m_itemWorkingDatas)
				{
					// none of the deferred-evaluators must be working anymore
					assert(wd.bitsWorkingDeferredEvaluators == 0);

					// if instant-evaluators discarded the item, then exactly those instant-evaluators must have fully finished their work
					assert(wd.bitsDiscardedByInstantEvaluators == 0 || (wd.bitsFinishedInstantEvaluators & wd.bitsDiscardedByInstantEvaluators) != 0);

					// instant-evaluators whose function calls caused an exception cannot have done any work (read: cannot have finished)
					assert((wd.bitsExceptionByInstantEvaluatorFunctionCalls & wd.bitsFinishedInstantEvaluators) == 0);

					// instant-evaluators that caused an exception themselves cannot have finished their work
					assert((wd.bitsExceptionByInstantEvaluatorsThemselves & wd.bitsFinishedInstantEvaluators) == 0);

					// if deferred-evaluators discarded the item, then exactly those deferred-evaluators must have fully finished their work
					assert(wd.bitsDiscardedByDeferredEvaluators == 0 || (wd.bitsFinishedDeferredEvaluators & wd.bitsDiscardedByDeferredEvaluators) != 0);

					// if instant-evaluator discarded the item, then none of the deferred-evaluators must have been started on that item
					// (instant-evaluators [no matter if cheap or expensive] always run before deferred-evaluators)
					assert((wd.bitsDiscardedByInstantEvaluators & (wd.bitsFinishedDeferredEvaluators | wd.bitsAbortedDeferredEvaluators)) == 0);

					// those deferred-evaluators that discarded the item cannot have been aborted
					// (the deferred-evaluator who discards an item aborts all other deferred-evaluators, but he never aborts himself)
					assert((wd.bitsDiscardedByDeferredEvaluators & wd.bitsAbortedDeferredEvaluators) == 0);

					// deferred-evaluators whose function calls caused an exception cannot have done any work (read: cannot have finished)
					assert((wd.bitsExceptionByDeferredEvaluatorFunctionCalls & (wd.bitsAbortedDeferredEvaluators | wd.bitsFinishedDeferredEvaluators)) == 0);

					// deferred-evaluators that caused an exception themselves cannot have finished their work
					assert((wd.bitsExceptionByDeferredEvaluatorsThemselves & (wd.bitsAbortedDeferredEvaluators | wd.bitsFinishedDeferredEvaluators)) == 0);
				}
#endif
			}

			return EPhaseStatus::Ok;
		}

	}
}
