// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace client
	{
		namespace internal
		{

			//===================================================================================
			//
			// CDeferredEvaluatorFactoryBase
			//
			//===================================================================================

			class CDeferredEvaluatorFactoryBase : public IDeferredEvaluatorFactory, public IParamsHolderFactory, public CFactoryBase<CDeferredEvaluatorFactoryBase>
			{
			public:
				// IDeferredEvaluatorFactory
				virtual const char*                      GetName() const override final;
				virtual const IInputParameterRegistry&   GetInputParameterRegistry() const override final;
				virtual IParamsHolderFactory&            GetParamsHolderFactory() const override final;
				// ~IDeferredEvaluatorFactory

				// IDeferredEvaluatorFactory: forward to derived class
				virtual DeferredEvaluatorUniquePtr       CreateDeferredEvaluator(const void* pParams) override = 0;
				virtual void                             DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy) override = 0;
				// ~IDeferredEvaluatorFactory

				// IParamsHolderFactory: forward to derived class
				virtual ParamsHolderUniquePtr            CreateParamsHolder() override = 0;
				virtual void                             DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) override = 0;
				// ~IParamsHolderFactory

			protected:
				explicit                                 CDeferredEvaluatorFactoryBase(const char* evaluatorName);

			protected:
				CInputParameterRegistry                  m_inputParameterRegistry;

			private:
				IParamsHolderFactory*                    m_pParamsHolderFactory;      // points to *this; it's a trick to allow GetParamsHolderFactory() return a non-const reference to *this
			};

			inline CDeferredEvaluatorFactoryBase::CDeferredEvaluatorFactoryBase(const char* evaluatorName)
				: CFactoryBase(evaluatorName)
			{
				m_pParamsHolderFactory = this;
			}

			inline const char* CDeferredEvaluatorFactoryBase::GetName() const
			{
				return CFactoryBase::GetName();
			}

			inline const IInputParameterRegistry& CDeferredEvaluatorFactoryBase::GetInputParameterRegistry() const
			{
				return m_inputParameterRegistry;
			}

			inline IParamsHolderFactory& CDeferredEvaluatorFactoryBase::GetParamsHolderFactory() const
			{
				return *m_pParamsHolderFactory;
			}

		} // namespace internal

		//===================================================================================
		//
		// CDeferredEvaluatorFactory<>
		//
		//===================================================================================

		template <class TDeferredEvaluator>
		class CDeferredEvaluatorFactory final : public internal::CDeferredEvaluatorFactoryBase
		{
		public:
			explicit                                 CDeferredEvaluatorFactory(const char* evaluatorName);

			// IDeferredEvaluatorFactory
			virtual DeferredEvaluatorUniquePtr       CreateDeferredEvaluator(const void* pParams) override;
			virtual void                             DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy) override;
			// ~IDeferredEvaluatorFactory

			// IParamsHolderFactory
			virtual ParamsHolderUniquePtr            CreateParamsHolder() override;
			virtual void                             DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) override;
			// ~IParamsHolderFactory
		};

		template <class TDeferredEvaluator>
		CDeferredEvaluatorFactory<TDeferredEvaluator>::CDeferredEvaluatorFactory(const char* evaluatorName)
			: CDeferredEvaluatorFactoryBase(evaluatorName)
		{
			typedef typename TDeferredEvaluator::SParams Params;
			Params::Expose(m_inputParameterRegistry);
		}

		template <class TDeferredEvaluator>
		DeferredEvaluatorUniquePtr CDeferredEvaluatorFactory<TDeferredEvaluator>::CreateDeferredEvaluator(const void* pParams)
		{
			const typename TDeferredEvaluator::SParams* pActualParams = static_cast<const typename TDeferredEvaluator::SParams*>(pParams);
			TDeferredEvaluator* pEvaluator = new TDeferredEvaluator(*pActualParams);
			internal::CDeferredEvaluatorDeleter deleter(*this);
			return DeferredEvaluatorUniquePtr(pEvaluator, deleter);
		}

		template <class TDeferredEvaluator>
		void CDeferredEvaluatorFactory<TDeferredEvaluator>::DestroyDeferredEvaluator(IDeferredEvaluator* pDeferredEvaluatorToDestroy)
		{
			delete pDeferredEvaluatorToDestroy;
		}

		template <class TDeferredEvaluator>
		ParamsHolderUniquePtr CDeferredEvaluatorFactory<TDeferredEvaluator>::CreateParamsHolder()
		{
			internal::CParamsHolder<typename TDeferredEvaluator::SParams>* pParamsHolder = new internal::CParamsHolder<typename TDeferredEvaluator::SParams>;
			CParamsHolderDeleter deleter(*this);
			return ParamsHolderUniquePtr(pParamsHolder, deleter);
		}

		template <class TDeferredEvaluator>
		void CDeferredEvaluatorFactory<TDeferredEvaluator>::DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy)
		{
			delete pParamsHolderToDestroy;
		}

	}
}
