// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryEntitySystem/IEntitySystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace stdlib
	{

		void EntityId_CreateItemDebugProxyForItem(const EntityIdWrapper& item, core::IItemDebugProxyFactory& itemDebugProxyFactory)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(item.value))
			{
				const Vec3 worldPos = pEntity->GetWorldPos();
				AABB entityBounds(AABB::RESET);
				pEntity->GetLocalBounds(entityBounds);

				// ensure a halfway proper AABB
				if (entityBounds.IsReset() || entityBounds.IsEmpty())
				{
					static const Vec3 defaultSize(0.2f, 0.2f, 0.2f);
					entityBounds.min = -defaultSize;
					entityBounds.max = defaultSize;
				}

				entityBounds.Move(worldPos);

				uqs::core::IItemDebugProxy_AABB& aabb = itemDebugProxyFactory.CreateAABB();
				aabb.SetAABB(entityBounds);
			}
		}

		void Vec3_CreateItemDebugProxyForItem(const Vec3& item, core::IItemDebugProxyFactory& itemDebugProxyFactory)
		{
			core::IItemDebugProxy_Sphere& sphere = itemDebugProxyFactory.CreateSphere();
			sphere.SetPosAndRadius(item, 0.2f);  // let's assume that a radius of 20cm is "just fine for all purposes"
		}

	}
}
