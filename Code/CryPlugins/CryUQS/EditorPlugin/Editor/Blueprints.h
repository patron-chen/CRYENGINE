// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "DocSerializationContext.h"
#include "ItemTypeName.h"

struct SValidatorKey;

namespace uqseditor
{
//////////////////////////////////////////////////////////////////////////

class CEvaluatorFactoryHelper;
class CQueryBlueprint;

//////////////////////////////////////////////////////////////////////////

class CItemUniquePtr
{
public:
	CItemUniquePtr();
	~CItemUniquePtr();

	CItemUniquePtr(CItemUniquePtr&& other);
	CItemUniquePtr& operator=(CItemUniquePtr&& other);

	explicit CItemUniquePtr(uqs::client::IItemFactory* pItemFactory);

	bool IsExist() const { return m_pItem != nullptr; }
	bool IsSerializable() const;

	bool SetFromStringLiteral(const string& stringLiteral);
	void ConvertToStringLiteral(string& outString) const;

private:
	CItemUniquePtr(const CItemUniquePtr& other) /*= delete*/;
	CItemUniquePtr& operator=(const CItemUniquePtr& other) /*= delete*/;

private:
	friend bool Serialize(Serialization::IArchive& archive, CItemUniquePtr& value, const char* szName, const char* szLabel);

	void* m_pItem;
	uqs::client::IItemFactory* m_pItemFactory;
};

bool Serialize(Serialization::IArchive& archive, CItemUniquePtr& value, const char* szName, const char* szLabel);

//////////////////////////////////////////////////////////////////////////

class CItemLiteral
{
public:
	CItemLiteral();

	explicit CItemLiteral(const SItemTypeName& typeName, const CUqsDocSerializationContext& context);

	const SItemTypeName& GetTypeName() const { return m_typeName; }

	bool SetFromStringLiteral(const string& stringLiteral);
	void ConvertToStringLiteral(string& outString) const;
		
	bool IsExist() const { return m_item.IsExist(); }
	bool IsSerializable() const { return m_item.IsSerializable();	}
	
private:

	friend bool Serialize(Serialization::IArchive& archive, CItemLiteral& value, const char* szName, const char* szLabel);

	CItemUniquePtr m_item;
	SItemTypeName m_typeName;
};

bool Serialize(Serialization::IArchive& archive, CItemLiteral& value, const char* szName, const char* szLabel);

//////////////////////////////////////////////////////////////////////////

class CFunctionSerializationHelper
{
public:

	struct SFunction
	{
		string                                           internalName;
		string                                           prettyName;
		uqs::client::IFunctionFactory::ELeafFunctionKind leafFunctionKind;
		uqs::client::IFunctionFactory*                   pFactory;
		string                                           param;
		SItemTypeName                                    returnType;
	};

private:
	class CFunctionList
	{
	public:

		void Build(const SItemTypeName& typeName, const CUqsDocSerializationContext& context);

		int  SerializeName(
		  Serialization::IArchive& archive,
		  const char* szName,
		  const char* szLabel,
		  const CUqsDocSerializationContext& context,
		  const SValidatorKey& validatorKey,
		  const int oldFunctionIdx);

		const SFunction& GetByIdx(const int idx) const;

		int              FindByInternalNameAndParam(const string& internalName, const string& param) const;

		void             Clear();
	private:
		std::vector<SFunction>    m_functions;
		Serialization::StringList m_functionsStringList;
	};

public:

	CFunctionSerializationHelper();
	CFunctionSerializationHelper(const char* szFunctionName, const char* szParamOrReturnValue);
	CFunctionSerializationHelper(const uqs::client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context);

	void Reset(const SItemTypeName& typeName, const CUqsDocSerializationContext& context);
	void ReserializeFunctionLiteralFromParam();

	bool SerializeFunctionName(
	  Serialization::IArchive& archive,
	  const char* szName,
	  const char* szLabel,
	  const CUqsDocSerializationContext& context);

	void SerializeFunctionParam(
		Serialization::IArchive& archive,
		const CUqsDocSerializationContext& context);

	const string&                  GetFunctionInternalName() const;
	const string&                  GetFunctionParamOrReturnValue() const;
	uqs::client::IFunctionFactory* GetFunctionFactory() const;
	const SFunction*               GetSelectedFunction() const;

	const SItemTypeName&           GetExpectedType() const { return m_typeName; }

private:

	void Prepare(const CUqsDocSerializationContext& context);
	void RebuildList(const CUqsDocSerializationContext& context);
	void UpdateSelectedFunctionIndex();

private:
	string           m_funcInternalName;
	SItemTypeName    m_typeName;

	string           m_funcParam;
	CItemLiteral     m_itemLiteral;
	mutable string   m_itemCachedLiteral;

	CFunctionList    m_functionsList;
	int              m_selectedFunctionIdx;

	static const int npos = Serialization::StringList::npos;
};

//////////////////////////////////////////////////////////////////////////

class CParametersListContext
{
public:

	CParametersListContext(CQueryBlueprint* pOwner, CUqsDocSerializationContext* pSerializationContext);

	~CParametersListContext();

	void SetOwner(CQueryBlueprint* pOwner) { m_pOwner = pOwner; }

	void SetParamsChanged(bool value)      { m_bParamsChanged = value; }
	bool GetParamsChanged() const          { return m_bParamsChanged; }

	void BuildFunctionListForAvailableParameters(
	  const SItemTypeName& typeNameToFilter,
	  const CUqsDocSerializationContext& context,
	  const std::vector<CFunctionSerializationHelper::SFunction>& allGlobalParamFunctions,
	  std::vector<CFunctionSerializationHelper::SFunction>& outParamFunctions);

private:
	CParametersListContext(const CParametersListContext& other) /*= delete*/;
	CParametersListContext& operator=(const CParametersListContext& other) /*= delete*/;

private:
	CUqsDocSerializationContext* m_pSerializationContext;
	CQueryBlueprint*             m_pOwner;
	bool                         m_bParamsChanged;
};

//////////////////////////////////////////////////////////////////////////

class CSelectedGeneratorContext
{
public:
	CSelectedGeneratorContext(CQueryBlueprint* pOwner, CUqsDocSerializationContext* pSerializationContext);

	~CSelectedGeneratorContext();

	void SetGeneratorChanged(bool value)   { m_bGeneratorChanged = value; }
	bool GetGeneratorChanged() const       { return m_bGeneratorChanged; }

	void SetGeneratorProcessed(bool value) { m_bGeneratorProcessed = value; }
	bool GetGeneratorProcessed() const     { return m_bGeneratorProcessed; }

	void BuildFunctionListForAvailableGenerators(
	  const SItemTypeName& typeNameToFilter,
	  const CUqsDocSerializationContext& context,
	  const std::vector<CFunctionSerializationHelper::SFunction>& allIteraedItemFunctions,
	  std::vector<CFunctionSerializationHelper::SFunction>& outParamFunctions);

private:
	CSelectedGeneratorContext(const CSelectedGeneratorContext& other) /*= delete*/;
	CSelectedGeneratorContext& operator=(const CSelectedGeneratorContext& other) /*= delete*/;

private:
	CUqsDocSerializationContext* m_pSerializationContext;
	CQueryBlueprint*             m_pOwner;
	bool                         m_bGeneratorChanged;
	bool                         m_bGeneratorProcessed;
};

//////////////////////////////////////////////////////////////////////////

class CErrorCollector
{
public:

	CErrorCollector();
	CErrorCollector(CErrorCollector&& other);
	CErrorCollector& operator=(CErrorCollector&& other);

	void             AddErrorMessage(string&& message);
	void             Clear();

	template<typename T>
	void Serialize(Serialization::IArchive& archive, const T& element)
	{
		Serialize(archive, SValidatorKey::FromObject(element));
	}

	void Serialize(Serialization::IArchive& archive, const SValidatorKey& validatorKey);

private:

	std::vector<string>                            m_messages;
	uqs::datasource::SyntaxErrorCollectorUniquePtr m_pExternalCollector;
};

//////////////////////////////////////////////////////////////////////////

class CInputBlueprint
{
public:
	const char*            GetParamName() const;
	const char*            GetFuncName() const;
	const char*            GetFuncReturnValueLiteral() const;
	const char*            GetAddReturnValueToDebugRenderWorldUponExecution() const;
	CInputBlueprint&       AddChild(const char* szParamName, const char* szFuncName, const char* szFuncReturnValueLiteral, const char* szAddReturnValueToDebugRenderWorldUponExecution);
	size_t                 GetChildCount() const;
	const CInputBlueprint& GetChild(size_t index) const;
	const CInputBlueprint* FindChildByParamName(const char* szParamName) const;

	CInputBlueprint();
	CInputBlueprint(const char* szParamName, const char* szFuncName, const char* szFuncReturnValueLiteral, const char* szAddReturnValueToDebugRenderWorldUponExecution);

	CInputBlueprint(const char* szParamName);

	CInputBlueprint(const uqs::client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context);

	CInputBlueprint(CInputBlueprint&& other);
	CInputBlueprint&                        operator=(CInputBlueprint&& other);

	void                                    ResetChildrenFromFactory(const uqs::client::IGeneratorFactory& generatorFactory, const CUqsDocSerializationContext& context);
	void                                    ResetChildrenFromFactory(const uqs::client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context);
	void                                    ResetChildrenFromFactory(const CEvaluatorFactoryHelper& evaluatorFactory, const CUqsDocSerializationContext& context);

	void                                    Serialize(Serialization::IArchive& archive);
	void                                    SerializeRoot(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context, const SValidatorKey& validatorKey);

	const std::vector<CInputBlueprint>&     GetChildren() const { return m_children; }

	void                                    PrepareHelpers(const uqs::client::IGeneratorFactory* pGeneratorFactory, const CUqsDocSerializationContext& context);
	void                                    PrepareHelpers(const uqs::client::IFunctionFactory* pFunctionFactory, const CUqsDocSerializationContext& context);
	void                                    PrepareHelpers(const CEvaluatorFactoryHelper* pEvaluatorFactory, const CUqsDocSerializationContext& context);
	void                                    ClearErrors();

	const std::shared_ptr<CErrorCollector>& GetErrorCollectorSharedPtr() const { return m_pErrorCollector; }

private:

	template<typename TFactory>
	void SetChildrenFromFactoryInputRegistry(const TFactory& factory, const CUqsDocSerializationContext& context);

	template<typename TFactory>
	void DeriveChildrenInfoFromFactoryInputRegistry(const TFactory& factory, const CUqsDocSerializationContext& context);

	void PrepareHelpersRoot(const CUqsDocSerializationContext& context);
	void PrepareHelpersChild(const CUqsDocSerializationContext& context);

	void SetAdditionalParamInfo(const uqs::client::IInputParameterRegistry::SParameterInfo& paramInfo, const CUqsDocSerializationContext& context);

	void SerializeFunction(Serialization::IArchive& archive, const CUqsDocSerializationContext& context, const char* szParamLabel);
	void SerializeChildren(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context);

private:

	string                                   m_paramName;
	string                                   m_addReturnValueToDebugRenderWorldUponExecution;

	CFunctionSerializationHelper             m_functionHelper;

	std::vector<CInputBlueprint>             m_children;

	mutable std::shared_ptr<CErrorCollector> m_pErrorCollector;

	// TODO pavloi 2016.04.05: hack - property tree doesn't copy label strings and stores just "const char*" expecting them to be static strings.
	// That's why we build label string and try to persist it in this member variable in hope, that the object will survive long enough.
	string m_paramLabel;
};

class CConstParamBlueprint
{
public:
	void   AddParameter(const char* szName, const char* szType, CItemLiteral&& value);
	size_t GetParameterCount() const;
	void   GetParameterInfo(size_t index, const char*& szName, const char*& szType, string& szValue, std::shared_ptr<CErrorCollector>& pErrorCollector) const;

	void   Serialize(Serialization::IArchive& archive);
	void   PrepareHelpers(CUqsDocSerializationContext& context);
	void   ClearErrors();

private:

	friend class CParametersListContext;

	struct SConstParam
	{
		SConstParam();
		SConstParam(const char* szName, const char* szType, CItemLiteral&& value);
		SConstParam(SConstParam&& other);
		SConstParam& operator=(SConstParam&& other);

		void Serialize(Serialization::IArchive& archive);
		void SerializeImpl(Serialization::IArchive& archive, CUqsDocSerializationContext& context);

		void ClearErrors();

		bool IsValid() const;

		string                                   name;
		SItemTypeName                            type;
		CItemLiteral                             value;
		mutable std::shared_ptr<CErrorCollector> pErrorCollector;
	};

	std::vector<SConstParam> m_params;
};

class CRuntimeParamBlueprint
{
public:
	void   AddParameter(const char* szName, const char* szType);
	size_t GetParameterCount() const;
	void   GetParameterInfo(size_t index, const char*& szName, const char*& szType, std::shared_ptr<CErrorCollector>& pErrorCollector) const;

	void   Serialize(Serialization::IArchive& archive);
	void   PrepareHelpers(CUqsDocSerializationContext& context);
	void   ClearErrors();

private:

	friend class CParametersListContext;

	struct SRuntimeParam
	{
		SRuntimeParam();
		SRuntimeParam(const char* szName, const char* szType);
		SRuntimeParam(SRuntimeParam&& other);
		SRuntimeParam& operator=(SRuntimeParam&& other);

		void Serialize(Serialization::IArchive& archive);
		void SerializeImpl(Serialization::IArchive& archive);

		void ClearErrors();

		bool IsValid() const;

		string                                   name;
		SItemTypeName                            type;
		mutable std::shared_ptr<CErrorCollector> pErrorCollector;
	};

	std::vector<SRuntimeParam> m_params;
};

class CGeneratorBlueprint
{
public:
	CGeneratorBlueprint();
	explicit CGeneratorBlueprint(const uqs::client::IGeneratorFactory& factory, const CUqsDocSerializationContext& context);

	void                                    SetGeneratorName(const char* szGeneratorName);
	CInputBlueprint&                        GetInputRoot();
	const CInputBlueprint&                  GetInputRoot() const;
	const char*                             GetGeneratorName() const;

	bool                                    IsSet() const { return !m_name.empty(); }

	void                                    Serialize(Serialization::IArchive& archive);
	void                                    PrepareHelpers(CUqsDocSerializationContext& context);
	void                                    ClearErrors();

	const std::shared_ptr<CErrorCollector>& GetErrorCollectorSharedPtr() const { return m_pErrorCollector; }

private:
	bool SerializeName(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context);
	void SerializeInputs(Serialization::IArchive& archive, const char* szName, const char* szLabel, const bool bNameChanged, const CUqsDocSerializationContext& context);

private:
	string                                   m_name;
	CInputBlueprint                          m_inputs;
	mutable std::shared_ptr<CErrorCollector> m_pErrorCollector;
};

class CEvaluator;

struct SEvaluatorBlueprintAdapter
{
	SEvaluatorBlueprintAdapter(CEvaluator& owner)
		: m_pOwner(&owner)
	{}

	void SetOwner(CEvaluator& newOwner)
	{
		m_pOwner = &newOwner;
	}

	virtual ~SEvaluatorBlueprintAdapter() {}

	CEvaluator& Owner() const;

private:

	CEvaluator* m_pOwner;
};

class CInstantEvaluatorBlueprint
	: public SEvaluatorBlueprintAdapter
{
public:
	void                   SetEvaluatorName(const char* szEvaluatorName);
	void                   SetWeight(const char* szWeight);
	const char*            GetWeight() const;
	CInputBlueprint&       GetInputRoot();
	const CInputBlueprint& GetInputRoot() const;
	const char*            GetEvaluatorName() const;

	CInstantEvaluatorBlueprint(CEvaluator& owner)
		: SEvaluatorBlueprintAdapter(owner)
	{}
};

class CDeferredEvaluatorBlueprint
	: public SEvaluatorBlueprintAdapter
{
public:
	void                   SetEvaluatorName(const char* szEvaluatorName);
	void                   SetWeight(const char* szWeight);
	const char*            GetWeight() const;
	CInputBlueprint&       GetInputRoot();
	const CInputBlueprint& GetInputRoot() const;
	const char*            GetEvaluatorName() const;

	CDeferredEvaluatorBlueprint(CEvaluator& owner)
		: SEvaluatorBlueprintAdapter(owner)
	{}
};

enum class EEvaluatorType
{
	Undefined,
	Instant,
	Deferred
};

class CEvaluator
{
public:

	static CEvaluator            CreateInstant();
	static CEvaluator            CreateDeferred();

	void                         SetEvaluatorName(const char* szEvaluatorName);
	void                         SetWeight(const char* szWeight);
	const char*                  GetWeight() const;
	CInputBlueprint&             GetInputRoot();
	const CInputBlueprint&       GetInputRoot() const;
	const char*                  GetEvaluatorName() const;

	CInstantEvaluatorBlueprint&  AsInstant();
	CDeferredEvaluatorBlueprint& AsDeferred();

	EEvaluatorType               GetType() const;

	CEvaluator();
	CEvaluator(CEvaluator&& other);
	CEvaluator&                             operator=(CEvaluator&& other);

	void                                    Serialize(Serialization::IArchive& archive);
	void                                    PrepareHelpers(CUqsDocSerializationContext& context);
	void                                    ClearErrors();

	const std::shared_ptr<CErrorCollector>& GetErrorCollectorSharedPtr() const { return m_pErrorCollector; }

protected:

	explicit CEvaluator(const EEvaluatorType type);

private:

	CEvaluator(const CEvaluator&);
	CEvaluator& operator=(const CEvaluator&);

	bool        SerializeName(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context);

	void        SetType(EEvaluatorType type);

private:

	string                                      m_name;
	float                                       m_weight;
	mutable string                              m_weightString;
	CInputBlueprint                             m_inputs;
	mutable std::shared_ptr<CErrorCollector>    m_pErrorCollector;

	EEvaluatorType                              m_evaluatorType;
	std::unique_ptr<SEvaluatorBlueprintAdapter> m_interfaceAdapter;
};

class CEvaluators
{
public:
	// Implementation of interface from ITextualQueryBlueprint
	CInstantEvaluatorBlueprint&        AddInstantEvaluator();
	size_t                             GetInstantEvaluatorCount() const;
	CInstantEvaluatorBlueprint&        GetInstantEvaluator(size_t index);
	const CInstantEvaluatorBlueprint&  GetInstantEvaluator(size_t index) const;
	void                               RemoveInstantEvaluator(size_t index);
	CDeferredEvaluatorBlueprint&       AddDeferredEvaluator();
	size_t                             GetDeferredEvaluatorCount() const;
	CDeferredEvaluatorBlueprint&       GetDeferredEvaluator(size_t index);
	const CDeferredEvaluatorBlueprint& GetDeferredEvaluator(size_t index) const;
	void                               RemoveDeferredEvaluator(size_t index);
	// ~ITextualQueryBlueprint

	void Serialize(Serialization::IArchive& archive);
	void PrepareHelpers(CUqsDocSerializationContext& context);
	void ClearErrors();
private:
	typedef std::vector<CEvaluator> Evaluators;

	Evaluators::iterator FindByIndexOfType(const EEvaluatorType type, const size_t index);
	size_t               CalcCountOfType(const EEvaluatorType type) const;

	Evaluators m_evaluators;
};

struct SQueryFactoryType
{
	// TODO pavloi 2016.06.23: I'm not totally sure, that this enum will actually be needed.
	// I suspect it may be to properly implement support for composite queries, but if not - it better be removed.
	enum EType : uint32
	{
		Regular,
		Chained,
		Fallbacks,
		Unknown,
		Count
	};

	class CTraits
	{
	public:
		CTraits();
		CTraits(const uqs::core::IQueryFactory& queryFactory);

		bool   operator==(const CTraits& other) const;
		bool   operator!=(const CTraits& other) const { return !(*this == other); }

		bool   IsUndefined() const;
		bool   RequiresGenerator() const;
		bool   SupportsEvaluators() const;
		bool   SupportsParameters() const;
		size_t GetMinRequiredChildren() const;
		size_t GetMaxAllowedChildren() const;

		bool   IsUnlimitedChildren() const;
		bool   CanHaveChildren() const;

	private:
		const uqs::core::IQueryFactory* m_pQueryFactory;
	};

	static const char* GetQueryFactoryNameByType(const EType queryFactoryType);
	static EType       GetQueryFactoryTypeByName(const string& queryFactoryType);

	SQueryFactoryType(const EType queryFactoryType);
	SQueryFactoryType(const char* szQueryFactoryName);

	bool                            Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext* pContext);

	const uqs::core::IQueryFactory* GetFactory(const CUqsDocSerializationContext* pContext) const;
	const CTraits&                  GetTraits() const { return queryTraits; }

	void                            UpdateTraits(const CUqsDocSerializationContext* pContext);

	string                          queryFactoryName;
	EType                           queryFactoryType;
private:
	CTraits                         queryTraits;
};

class CQueryBlueprint
{
private:
	typedef std::vector<CQueryBlueprint> QueryBlueprintChildren;

public:
	const CGeneratorBlueprint& GetGenerator() const;

	CQueryBlueprint();

	const string& GetName() const { return m_name; }

	void          BuildSelfFromITextualQueryBlueprint(const uqs::core::ITextualQueryBlueprint& source, CUqsDocSerializationContext& context);
	void          BuildITextualQueryBlueprintFromSelf(uqs::core::ITextualQueryBlueprint& target) const;

	void          Serialize(Serialization::IArchive& archive);

	void          PrepareHelpers(CUqsDocSerializationContext& context);
	void          ClearErrors();

private:

	void        CheckQueryTraitsChange(const SQueryFactoryType::CTraits& queryTraits, const SQueryFactoryType::CTraits& oldTraits, CParametersListContext& paramListContext);

	static void HelpBuildCInputBlueprintHierarchyFromITextualInputBlueprint(CInputBlueprint& targetRoot, const uqs::core::ITextualInputBlueprint& sourceRoot);
	static void HelpBuildITextualInputBlueprintHierarchyFromCInputBlueprint(uqs::core::ITextualInputBlueprint& targetRoot, const CInputBlueprint& sourceRoot);

private:

	friend class CParametersListContext;

	string                                   m_name;
	SQueryFactoryType                        m_queryFactory;
	size_t                                   m_maxItemsToKeepInResultSet;
	CConstParamBlueprint                     m_constParams;
	CRuntimeParamBlueprint                   m_runtimeParams;
	CGeneratorBlueprint                      m_generator;
	CEvaluators                              m_evaluators;
	QueryBlueprintChildren                   m_queryChildren;
	SItemTypeName                            m_expectedShuttledType;
	mutable std::shared_ptr<CErrorCollector> m_pErrorCollector;
};
} // uqseditor
