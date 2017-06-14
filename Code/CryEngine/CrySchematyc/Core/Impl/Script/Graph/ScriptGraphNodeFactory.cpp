// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptGraphNodeFactory.h"

namespace Schematyc
{
	bool CScriptGraphNodeFactory::RegisterCreator(const IScriptGraphNodeCreatorPtr& pCreator)
	{
		if(!pCreator)
		{
			return false;
		}
		const SGUID& typeGUID = pCreator->GetTypeGUID();
		if(GetCreator(typeGUID))
		{
			return false;
		}
		m_creators.insert(Creators::value_type(typeGUID, pCreator));
		return true;
	}

	IScriptGraphNodeCreator* CScriptGraphNodeFactory::GetCreator(const SGUID& typeGUID)
	{
		Creators::iterator itCreator = m_creators.find(typeGUID);
		return itCreator != m_creators.end() ? itCreator->second.get() : nullptr;
	}

	IScriptGraphNodePtr CScriptGraphNodeFactory::CreateNode(const SGUID& typeGUID, const SGUID& guid)
	{
		IScriptGraphNodeCreator* pCreator = GetCreator(typeGUID);
		return pCreator ? pCreator->CreateNode(guid) : IScriptGraphNodePtr();
	}

	void CScriptGraphNodeFactory::PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph)
	{
		for(Creators::value_type& creator : m_creators)
		{
			creator.second->PopulateNodeCreationMenu(nodeCreationMenu, scriptView, graph);
		}
	}
}