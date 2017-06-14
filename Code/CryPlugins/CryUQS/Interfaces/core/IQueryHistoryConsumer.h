// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// IQueryHistoryConsumer
		//
		// - reads back data from an IQueryHistoryManager
		// - basically, it's the IQueryHistoryManager that calls back into this class to provide it with the requested data
		// - usually, reading data happens in an event-driven way by getting notified through an IQueryHistoryListener
		//
		//===================================================================================

		struct IQueryHistoryConsumer
		{
			// passed in to AddHistoricQuery()
			struct SHistoricQueryOverview
			{
				explicit                  SHistoricQueryOverview(const ColorF& _color, const char *_querierName, const CQueryID& _queryID, const CQueryID& _parentQueryID, const char* _queryBlueprintName, size_t _numGeneratedItems, size_t _numResultingItems, CTimeValue _timeElapsedUntilResult);

				// TODO: itemType of the generated items

				ColorF                    color;
				const char *              querierName;
				const CQueryID&           queryID;
				const CQueryID&           parentQueryID;
				const char*               queryBlueprintName;
				size_t                    numGeneratedItems;
				size_t                    numResultingItems;
				CTimeValue                timeElapsedUntilResult;
			};

			virtual                       ~IQueryHistoryConsumer() {}

			// - called when requesting to enumerate all historic queries via IQueryHistoryManager::EnumerateHistoricQueries()
			// - the passed in format string will contain some short info about the historic query
			virtual void                  AddHistoricQuery(const SHistoricQueryOverview& overview) = 0;

			// - called when requesting details about a specific historic query via IQueryHistoryManager::GetDetailsOfHistoricQuery()
			// - details about the historic query may be comprised of multiple text lines, hence this method may get called multiple times in a row
			virtual void                  AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* fmt, ...) = 0;

			// - called when requesting details about the potentially focused item in the 3D world
			// - this function may or may not get called multiple times in a row, depending on the amount of details and whether an item is focused at all
			virtual void                  AddTextLineToFocusedItem(const ColorF& color, const char* fmt, ...) = 0;

			// - called when requesting the names of all evaluators involved in a specific historic query
			// - see IQueryHistoryManager::EnumerateInstantEvaluatorNames() and EnumerateDeferredEvaluatorNames()
			virtual void                  AddInstantEvaluatorName(const char* szInstantEvaluatorName) = 0;
			virtual void                  AddDeferredEvaluatorName(const char* szDeferredEvaluatorName) = 0;
		};

		inline IQueryHistoryConsumer::SHistoricQueryOverview::SHistoricQueryOverview(const ColorF& _color, const char *_querierName, const CQueryID& _queryID, const CQueryID& _parentQueryID, const char* _queryBlueprintName, size_t _numGeneratedItems, size_t _numResultingItems, CTimeValue _timeElapsedUntilResult)
			: color(_color)
			, querierName(_querierName)
			, queryID(_queryID)
			, parentQueryID(_parentQueryID)
			, queryBlueprintName(_queryBlueprintName)
			, numGeneratedItems(_numGeneratedItems)
			, numResultingItems(_numResultingItems)
			, timeElapsedUntilResult(_timeElapsedUntilResult)
		{
			// nothing
		}

	}
}
