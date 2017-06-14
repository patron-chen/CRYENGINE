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
			// CInstantEvaluatorFactoryBase
			//
			//===================================================================================

			class CInstantEvaluatorFactoryBase : public IInstantEvaluatorFactory, public IParamsHolderFactory, public CFactoryBase<CInstantEvaluatorFactoryBase>
			{
			public:
				// IInstantEvaluatorFactory
				virtual const char*                      GetName() const override final;
				virtual const IInputParameterRegistry&   GetInputParameterRegistry() const override final;
				virtual IParamsHolderFactory&            GetParamsHolderFactory() const override final;
				// ~IInstantEvaluatorFactory

				// IInstantEvaluatorFactory: forward to derived class
				virtual ECostCategory                    GetCostCategory() const override = 0;
				virtual EEvaluationModality              GetEvaluationModality() const override = 0;
				virtual InstantEvaluatorUniquePtr        CreateInstantEvaluator() override = 0;
				virtual void                             DestroyInstantEvaluator(IInstantEvaluator* pInstantEvaluatorToDestroy) override = 0;
				// ~IInstantEvaluatorFactory

				// IParamsHolderFactory: forward to derived class
				virtual ParamsHolderUniquePtr            CreateParamsHolder() override = 0;
				virtual void                             DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) override = 0;
				// ~IParamsHolderFactory

			protected:
				explicit                                 CInstantEvaluatorFactoryBase(const char* evaluatorName);

			protected:
				CInputParameterRegistry                  m_inputParameterRegistry;

			private:
				IParamsHolderFactory*                    m_pParamsHolderFactory;      // points to *this; it's a trick to allow GetParamsHolderFactory() return a non-const reference to *this
			};

			inline CInstantEvaluatorFactoryBase::CInstantEvaluatorFactoryBase(const char* evaluatorName)
				: CFactoryBase(evaluatorName)
			{
				m_pParamsHolderFactory = this;
			}

			inline const char* CInstantEvaluatorFactoryBase::GetName() const
			{
				return CFactoryBase::GetName();
			}

			inline const IInputParameterRegistry& CInstantEvaluatorFactoryBase::GetInputParameterRegistry() const
			{
				return m_inputParameterRegistry;
			}

			inline IParamsHolderFactory& CInstantEvaluatorFactoryBase::GetParamsHolderFactory() const
			{
				return *m_pParamsHolderFactory;
			}

		} // namespace internal

		//===================================================================================
		//
		// CInstantEvaluatorFactory<>
		//
		//===================================================================================

		template <class TInstantEvaluator>
		class CInstantEvaluatorFactory final : public internal::CInstantEvaluatorFactoryBase
		{
		public:
			explicit                             CInstantEvaluatorFactory(const char* evaluatorName);

			// IInstantEvaluatorFactory
			virtual ECostCategory                GetCostCategory() const override;
			virtual EEvaluationModality          GetEvaluationModality() const override;
			virtual InstantEvaluatorUniquePtr    CreateInstantEvaluator() override;
			virtual void                         DestroyInstantEvaluator(IInstantEvaluator* pInstantEvaluatorToDestroy) override;
			// ~IInstantEvaluatorFactory

			// IParamsHolderFactory
			virtual ParamsHolderUniquePtr        CreateParamsHolder() override;
			virtual void                         DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy) override;
			// ~IParamsHolderFactory
		};

		template <class TInstantEvaluator>
		CInstantEvaluatorFactory<TInstantEvaluator>::CInstantEvaluatorFactory(const char* evaluatorName)
			: CInstantEvaluatorFactoryBase(evaluatorName)
		{
			typedef typename TInstantEvaluator::SParams Params;
			Params::Expose(m_inputParameterRegistry);
		}

		template <class TInstantEvaluator>
		IInstantEvaluatorFactory::ECostCategory CInstantEvaluatorFactory<TInstantEvaluator>::GetCostCategory() const
		{
			return TInstantEvaluator::kCostCategory;
		}

		template <class TInstantEvaluator>
		IInstantEvaluatorFactory::EEvaluationModality CInstantEvaluatorFactory<TInstantEvaluator>::GetEvaluationModality() const
		{
			return TInstantEvaluator::kEvaluationModality;
		}

		template <class TInstantEvaluator>
		InstantEvaluatorUniquePtr CInstantEvaluatorFactory<TInstantEvaluator>::CreateInstantEvaluator()
		{
#if 0
			TInstantEvaluator* pEvaluator = new TInstantEvaluator;
#else
			// notice: we assign the instantiated evaluator to its base class pointer to ensure that the evaluator type itself (and not accidentally another evaluator type) was injected at its class definition
			CInstantEvaluatorBase<TInstantEvaluator, TInstantEvaluator::kCostCategory, TInstantEvaluator::kEvaluationModality>* pEvaluator = new TInstantEvaluator;
#endif
			internal::CInstantEvaluatorDeleter deleter(*this);
			return InstantEvaluatorUniquePtr(pEvaluator, deleter);
		}

		template <class TInstantEvaluator>
		void CInstantEvaluatorFactory<TInstantEvaluator>::DestroyInstantEvaluator(IInstantEvaluator* pInstantEvaluatorToDestroy)
		{
			delete pInstantEvaluatorToDestroy;
		}

		template <class TInstantEvaluator>
		ParamsHolderUniquePtr CInstantEvaluatorFactory<TInstantEvaluator>::CreateParamsHolder()
		{
			internal::CParamsHolder<typename TInstantEvaluator::SParams>* pParamsHolder = new internal::CParamsHolder<typename TInstantEvaluator::SParams>;
			CParamsHolderDeleter deleter(*this);
			return ParamsHolderUniquePtr(pParamsHolder, deleter);
		}

		template <class TInstantEvaluator>
		void CInstantEvaluatorFactory<TInstantEvaluator>::DestroyParamsHolder(IParamsHolder* pParamsHolderToDestroy)
		{
			delete pParamsHolderToDestroy;
		}

	}
}
