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
			// CFunctionFactoryBase
			//
			//===================================================================================

			class CFunctionFactoryBase : public IFunctionFactory, public CFactoryBase<CFunctionFactoryBase>
			{
			public:
				// IFunctionFactory
				virtual const char*                       GetName() const override final;
				virtual const IInputParameterRegistry&    GetInputParameterRegistry() const override final;
				// ~IFunctionFactory

				// IFunctionFactory: forward to derived class
				virtual const shared::CTypeInfo&          GetReturnType() const override = 0;
				virtual const shared::CTypeInfo*          GetContainedType() const override = 0;
				virtual ELeafFunctionKind                 GetLeafFunctionKind() const override = 0;
				virtual FunctionUniquePtr                 CreateFunction(const IFunction::SCtorContext& ctorContext) override = 0;
				virtual void                              DestroyFunction(IFunction* pFunctionToDestroy) override = 0;
				// ~IFunctionFactory

			protected:
				explicit                                  CFunctionFactoryBase(const char* functionName);

			protected:
				CInputParameterRegistry                   m_inputParameterRegistry;
			};

			inline CFunctionFactoryBase::CFunctionFactoryBase(const char* functionName)
				: CFactoryBase(functionName)
			{}

			inline const char* CFunctionFactoryBase::GetName() const
			{
				return CFactoryBase::GetName();
			}

			inline const IInputParameterRegistry& CFunctionFactoryBase::GetInputParameterRegistry() const
			{
				return m_inputParameterRegistry;
			}

			//===================================================================================
			//
			// SFunctionParamsExpositionHelper<>
			//
			// - helper struct that is used by CFunctionFactory<>'s ctor to register the parameters of the function that factory creates
			// - depending on whether it's a leaf-function or non-leaf function, parameters may or may not get registered (leaf-function don't have input parameters)
			// - this goes hand in hand with how CFunctionBase<>::Execute() is implemented: leaf-functions never get passed in any parameters when being called, while non-leaf functions do
			// - actually, this template should better reside in the private section of CFunctionFactory<>, but template specializations are not allowed inside a class
			//
			//===================================================================================

			template <class TFunction, bool isLeafFunction>
			struct SFunctionParamsExpositionHelper
			{
			};

			template <class TFunction>
			struct SFunctionParamsExpositionHelper<TFunction, true>
			{
				static void Expose(CInputParameterRegistry& registry)
				{
					// nothing (leaf-function have no parameters to expose)
				}
			};

			template <class TFunction>
			struct SFunctionParamsExpositionHelper<TFunction, false>
			{
				static void Expose(CInputParameterRegistry& registry)
				{
					typedef typename TFunction::SParams Params;
					Params::Expose(registry);
				}
			};

		} // namespace internal

		//===================================================================================
		//
		// CFunctionFactory<>
		//
		//===================================================================================

		template <class TFunction>
		class CFunctionFactory : public internal::CFunctionFactoryBase
		{
		public:
			explicit                            CFunctionFactory(const char* functionName);

			// IFunctionFactory
			virtual const shared::CTypeInfo&    GetReturnType() const override final;
			virtual const shared::CTypeInfo*    GetContainedType() const override final;
			virtual ELeafFunctionKind           GetLeafFunctionKind() const override final;
			virtual FunctionUniquePtr           CreateFunction(const IFunction::SCtorContext& ctorContext) override final;
			virtual void                        DestroyFunction(IFunction* pFunctionToDestroy) override final;
			// ~IFunctionFactory
		};

		template <class TFunction>
		inline CFunctionFactory<TFunction>::CFunctionFactory(const char* functionName)
			: CFunctionFactoryBase(functionName)
		{
			const bool bIsLeafFunction = TFunction::kLeafFunctionKind != ELeafFunctionKind::None;
			internal::SFunctionParamsExpositionHelper<TFunction, bIsLeafFunction>::Expose(m_inputParameterRegistry);
		}

		template <class TFunction>
		const shared::CTypeInfo& CFunctionFactory<TFunction>::GetReturnType() const
		{
			return shared::SDataTypeHelper<typename TFunction::ReturnType>::GetTypeInfo();
		}

		template <class TFunction>
		const shared::CTypeInfo* CFunctionFactory<TFunction>::GetContainedType() const
		{
			return internal::SContainedTypeRetriever<typename TFunction::ReturnType>::GetTypeInfo();
		}

		template <class TFunction>
		IFunctionFactory::ELeafFunctionKind CFunctionFactory<TFunction>::GetLeafFunctionKind() const
		{
			return TFunction::kLeafFunctionKind;
		}

		template <class TFunction>
		FunctionUniquePtr CFunctionFactory<TFunction>::CreateFunction(const IFunction::SCtorContext& ctorContext)
		{
#if 0
			TFunction* pFunction = new TFunction(ctorContext);
#else
			// notice: we assign the instantiated function to its base class pointer to ensure that the function type itself (and not accidentally another function type) was injected at its class definition
			CFunctionBase<TFunction, typename TFunction::ReturnType, TFunction::kLeafFunctionKind>* pFunction = new TFunction(ctorContext);
#endif
			internal::CFunctionDeleter deleter(*this);
			return FunctionUniquePtr(pFunction, deleter);
		}

		template <class TFunction>
		void CFunctionFactory<TFunction>::DestroyFunction(IFunction* pFunctionToDestroy)
		{
			delete pFunctionToDestroy;
		}

	}
}
