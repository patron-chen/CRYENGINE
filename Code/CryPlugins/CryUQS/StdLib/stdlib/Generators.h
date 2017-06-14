// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#include <CryAISystem/INavigationSystem.h>

namespace uqs
{
	namespace stdlib
	{

		//===================================================================================
		//
		// CGenerator_PointsOnPureGrid
		//
		// - generates points on a grid
		// - the grid is specified by a center, size (of one edge) and a spacing between the points
		// - notice: this generator is very limited and doesn't work well if you intend to use it for things like uneven terrain
		//           -> rather use CGenerator_PointsOnNavMesh for that
		//
		//===================================================================================

		class CGenerator_PointsOnPureGrid : public client::CGeneratorBase<CGenerator_PointsOnPureGrid, Vec3>
		{
		public:
			struct SParams
			{
				Vec3                    center;                  // center of the grid
				float                   size;                    // length of one edge of the grid
				float                   spacing;                 // space between individual points on both, the x- and y-axis

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("center", center);
					UQS_EXPOSE_PARAM("size", size);
					UQS_EXPOSE_PARAM("spacing", spacing);
				UQS_EXPOSE_PARAMS_END
			};

		public:
			explicit                    CGenerator_PointsOnPureGrid(const SParams& params);
			EUpdateStatus               DoUpdate(const SUpdateContext& updateContext, client::CItemListProxy_Writable<Vec3>& itemListToPopulate);

		private:
			const SParams               m_params;
		};

		//===================================================================================
		//
		// CGenerator_PointsOnNavMesh
		//
		// - generates Vec3s on the navigation mesh limited by an AABB
		// - the AABB is defined by a pivot and local mins and maxs
		// - the pivot has to reside within the enclosing volume of a NavMesh or else no points will be generated
		// - each of the resulting points will reside on the center of the corresponding NavMesh triangle
		//
		//===================================================================================

		class CGenerator_PointsOnNavMesh : public client::CGeneratorBase<CGenerator_PointsOnNavMesh, Vec3>
		{
		public:
			struct SParams
			{
				Vec3                    pivot;                   // center of the AABB
				Vec3                    localAABBMins;           // local min extents of the AABB in which the points will be generated
				Vec3                    localAABBMaxs;           // local max extents of the AABB in which the points will be generated
				NavigationAgentTypeID   navigationAgentTypeID;   // the points will be generated in this layer of the NavMesh

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("pivot", pivot);
					UQS_EXPOSE_PARAM("localAABBMins", localAABBMins);
					UQS_EXPOSE_PARAM("localAABBMaxs", localAABBMaxs);
					UQS_EXPOSE_PARAM("navigationAgentTypeID", navigationAgentTypeID);
				UQS_EXPOSE_PARAMS_END
			};

		public:
			explicit                    CGenerator_PointsOnNavMesh(const SParams& params);
			EUpdateStatus               DoUpdate(const SUpdateContext& updateContext, client::CItemListProxy_Writable<Vec3>& itemListToPopulate);

		private:
			const SParams               m_params;
		};

	}
}
