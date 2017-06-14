// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySerialization/Forward.h>
#include <CryMath/Cry_Camera.h>

#include <Schematyc/Entity/EntityClasses.h>
#include <Schematyc/Types/BasicTypes.h>
#include <Schematyc/Types/MathTypes.h>

#include "AutoRegister.h"
#include "STDEnv.h"
#include "STDModules.h"
#include "Entity/EntityObjectMap.h"

namespace Schematyc
{
namespace Entity
{
CSharedString GetEntityName(ExplicitEntityId entityId)
{
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityId));
	return pEntity ? CSharedString(pEntity->GetName()) : CSharedString();
}

ObjectId GetEntityObjectId(ExplicitEntityId entityId)
{
	return CSTDEnv::GetInstance().GetEntityObjectMap().GetEntityObjectId(static_cast<EntityId>(entityId));
}

CRotation GetViewRotation(bool bIgnorePitchAndRoll)
{
	Ang3 yawPitchRoll = gEnv->pSystem->GetViewCamera().GetAngles();
	if (bIgnorePitchAndRoll)
	{
		yawPitchRoll.y = 0.0f;
		yawPitchRoll.z = 0.0f;
	}
	return CRotation(Ang3(yawPitchRoll.y, yawPitchRoll.z, yawPitchRoll.x));
}

static void RegisterUtilFunctions(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(g_entityClassGUID);
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityName, "955ca6c4-5b0a-4150-aaba-79e2939e85f7"_schematyc_guid, "GetEntityName");
		pFunction->SetAuthor(g_szCrytek);
		pFunction->SetDescription("Get name of entity");
		pFunction->BindInput(1, 'ent', "EntityId");
		pFunction->BindOutput(0, 'name', "Name");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetEntityObjectId, "cdd011ac-14a1-403b-af21-28d9e77e562d"_schematyc_guid, "GetEntityObjectId");
		pFunction->SetAuthor(g_szCrytek);
		pFunction->SetDescription("Get object id from entity");
		pFunction->BindInput(1, 'ent', "EntityId");
		pFunction->BindOutput(0, 'obj', "ObjectId");
		scope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&GetViewRotation, "766aeb56-dba1-40ae-8094-028422563dca"_schematyc_guid, "GetViewRotation");
		pFunction->SetAuthor(g_szCrytek);
		pFunction->SetDescription("Get view rotation");
		pFunction->BindInput(1, 'ipr', "IgnorePitchAndRoll", nullptr, true);
		pFunction->BindOutput(0, 'rot', "Rotation");
		scope.Register(pFunction);
	}
}
} // Entity
} // Schematyc

SCHEMATYC_AUTO_REGISTER(&Schematyc::Entity::RegisterUtilFunctions)
