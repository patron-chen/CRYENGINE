// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// QueryFactoryDatabase
		//
		//===================================================================================

		typedef CFactoryDatabase<IQueryFactory> QueryFactoryDatabase;

		//===================================================================================
		//
		// QueryBaseUniquePtr
		//
		//===================================================================================

		typedef std::unique_ptr<CQueryBase> QueryBaseUniquePtr;

		//===================================================================================
		//
		// CQueryFactoryBase
		//
		//===================================================================================

		class CQueryBlueprint;

		class CQueryFactoryBase : public IQueryFactory
		{
		public:
			// IQueryFactory
			virtual const char*                 GetName() const override;
			virtual bool                        SupportsParameters() const override;
			virtual bool                        RequiresGenerator() const override;
			virtual bool                        SupportsEvaluators() const override;
			virtual size_t                      GetMinRequiredChildren() const override;
			virtual size_t                      GetMaxAllowedChildren() const override;
			// ~IQueryFactory

			virtual QueryBaseUniquePtr          CreateQuery(const CQueryBase::SCtorContext& ctorContext) = 0;
			virtual const shared::CTypeInfo&    GetQueryBlueprintType(const CQueryBlueprint& queryBlueprint) const = 0;
			virtual bool                        CheckOutputTypeCompatibilityAmongChildQueryBlueprints(const CQueryBlueprint& parentQueryBlueprint, string& error, size_t& childIndexCausingTheError) const = 0;

			static void                         RegisterAllInstancesInDatabase(QueryFactoryDatabase& databaseToRegisterIn);

		protected:
			explicit                            CQueryFactoryBase(const char* name, bool bSupportsParameters, bool bRequiresGenerator, bool bSupportsEvaluators, size_t minRequiredChildren, size_t maxAllowedChildren);

		private:
			string                              m_name;
			bool                                m_bSupportsParameters;
			bool                                m_bRequiresGenerator;
			bool                                m_bSupportsEvaluators;
			size_t                              m_minRequiredChildren;
			size_t                              m_maxAllowedChildren;
			CQueryFactoryBase*                  m_pNext;
			static CQueryFactoryBase*           s_pList;
		};

		//===================================================================================
		//
		// CQueryFactory<>
		//
		//===================================================================================

		template <class TQuery>
		class CQueryFactory : public CQueryFactoryBase
		{
		public:
			explicit                            CQueryFactory(const char* name, bool bSupportsParameters, bool bRequiresGenerator, bool bSupportsEvaluators, size_t minRequiredChildren, size_t maxAllowedChildren);

			// CQueryFactoryBase
			virtual QueryBaseUniquePtr          CreateQuery(const CQueryBase::SCtorContext& ctorContext) override;
			virtual const shared::CTypeInfo&    GetQueryBlueprintType(const CQueryBlueprint& queryBlueprint) const override;
			virtual bool                        CheckOutputTypeCompatibilityAmongChildQueryBlueprints(const CQueryBlueprint& parentQueryBlueprint, string& error, size_t& childIndexCausingTheError) const override;
			// ~CQueryFactoryBase
		};

		template <class TQuery>
		CQueryFactory<TQuery>::CQueryFactory(const char* name, bool bSupportsParameters, bool bRequiresGenerator, bool bSupportsEvaluators, size_t minRequiredChildren, size_t maxAllowedChildren)
			: CQueryFactoryBase(name, bSupportsParameters, bRequiresGenerator, bSupportsEvaluators, minRequiredChildren, maxAllowedChildren)
		{
			assert(minRequiredChildren <= maxAllowedChildren);
		}

		template <class TQuery>
		QueryBaseUniquePtr CQueryFactory<TQuery>::CreateQuery(const CQueryBase::SCtorContext& ctorContext)
		{
			return QueryBaseUniquePtr(new TQuery(ctorContext));
		}

	}
}
