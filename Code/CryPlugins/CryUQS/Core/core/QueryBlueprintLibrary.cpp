// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprintLibrary.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		CQueryBlueprintLibrary::CQueryBlueprintLibrary()
		{
			// nothing
		}

		CQueryBlueprintLibrary::ELoadAndStoreResult CQueryBlueprintLibrary::LoadAndStoreQueryBlueprint(ELoadAndStoreOverwriteBehavior overwriteBehavior, datasource::IQueryBlueprintLoader& loader, shared::IUqsString& error)
		{
			CTextualQueryBlueprint textualQueryBP;

			if (!loader.LoadTextualQueryBlueprint(textualQueryBP, error))
			{
				return ELoadAndStoreResult::ExceptionOccurred;
			}

			const char* queryBPName = textualQueryBP.GetName();
			const CQueryBlueprintID blueprintID = FindQueryBlueprintIDByName(queryBPName);
			const bool bQueryBPExistsAlready = blueprintID.IsOrHasBeenValid() && (m_queryBlueprintsVector[blueprintID.m_index] != nullptr);

			if (bQueryBPExistsAlready && overwriteBehavior == ELoadAndStoreOverwriteBehavior::KeepExisting)
			{
				return ELoadAndStoreResult::PreservedExistingOne;
			}

			std::shared_ptr<CQueryBlueprint> pNewBP(new CQueryBlueprint);
			if (!pNewBP->Resolve(textualQueryBP))
			{
				error.Format("Could not resolve the textual query-blueprint '%s'", queryBPName);
				return ELoadAndStoreResult::ExceptionOccurred;
			}

			assert(strcmp(queryBPName, pNewBP->GetName()) == 0);

			if (blueprintID.IsOrHasBeenValid())
			{
				// replace the existing blueprint (or re-fill its empty slot)
				m_queryBlueprintsVector[blueprintID.m_index] = pNewBP;
			}
			else
			{
				// this is a completely new blueprint that has never been in the library so far
				const CQueryBlueprintID newBlueprintID((int32)m_queryBlueprintsVector.size(), queryBPName);
				m_queryBlueprintsVector.push_back(pNewBP);
				m_queryBlueprintsMap[queryBPName] = newBlueprintID;
			}

			return bQueryBPExistsAlready ? ELoadAndStoreResult::OverwrittenExistingOne : ELoadAndStoreResult::StoredFromScratch;
		}

		bool CQueryBlueprintLibrary::RemoveStoredQueryBlueprint(const char* szQueryBlueprintName, shared::IUqsString& error)
		{
			auto it = m_queryBlueprintsMap.find(szQueryBlueprintName);
			if (it == m_queryBlueprintsMap.cend())
			{
				error.Format("Could not remove query-blueprint '%s': not found", szQueryBlueprintName);
				return false;
			}

			const CQueryBlueprintID& blueprintID = it->second;
			m_queryBlueprintsVector[blueprintID.m_index].reset();   // clear the slot (but keep it around so that any CQueryBlueprintID referencing it will effectively become a "weak pointer", which is what we intend)
			return true;
		}

		CQueryBlueprintID CQueryBlueprintLibrary::FindQueryBlueprintIDByName(const char* szQueryBlueprintName) const
		{
			auto it = m_queryBlueprintsMap.find(szQueryBlueprintName);
			return (it == m_queryBlueprintsMap.cend()) ? CQueryBlueprintID(CQueryBlueprintID::s_invalidIndex, szQueryBlueprintName) : it->second;
		}

		const IQueryBlueprint* CQueryBlueprintLibrary::GetQueryBlueprintByID(const CQueryBlueprintID& blueprintID) const
		{
			return GetQueryBlueprintByIDInternal(blueprintID).get();
		}

		std::shared_ptr<const CQueryBlueprint> CQueryBlueprintLibrary::GetQueryBlueprintByIDInternal(const CQueryBlueprintID& blueprintID) const
		{
			if (blueprintID.IsOrHasBeenValid())
			{
				return m_queryBlueprintsVector[blueprintID.m_index];  // might still be a nullptr (when the blueprint got removed from the library and hasn't been re-loaded again)
			}
			else
			{
				return nullptr;
			}
		}

		void CQueryBlueprintLibrary::PrintToConsole(CLogger& logger) const
		{
			logger.Printf("");
			logger.Printf("=== %i UQS query blueprints in the library ===", (int)m_queryBlueprintsMap.size());
			logger.Printf("");
			CLoggerIndentation _indent;

			for (const auto& entry : m_queryBlueprintsMap)
			{
				const CQueryBlueprintID& blueprintID = entry.second;
				const CQueryBlueprint* pBlueprint = m_queryBlueprintsVector[blueprintID.m_index].get();

				if (pBlueprint)
				{
					pBlueprint->PrintToConsole(logger);
				}
				else
				{
					logger.Printf("\"%s\" (was removed and hasn't been (successfully) loaded again)", entry.first.c_str());
				}

				logger.Printf("");
			}
		}

	}
}
