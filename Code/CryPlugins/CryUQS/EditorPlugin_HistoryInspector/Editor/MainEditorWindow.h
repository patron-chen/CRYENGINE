// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QtViewPane.h>
#include <QLabel>
#include <QAbstractItemModel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>

#include <IEditor.h>
#include <DockedWidget.h>
#include <Controls/EditorDialog.h>

#include <CryUQS/Interfaces/InterfacesIncludes.h>
#include "QPropertyTree/QPropertyTree.h"

struct SQuery;
class CHistoricQueryTreeModel;
class CHistoricQueryTreeView;

class CMainEditorWindow : public CDockableWindow, public IEditorNotifyListener, public uqs::core::IQueryHistoryListener, public uqs::core::IQueryHistoryConsumer
{
	Q_OBJECT

public:

	CMainEditorWindow();
	~CMainEditorWindow();

	// CDockableWindow
	virtual const char*                       GetPaneTitle() const override;
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	// ~CDockableWindow

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent ev) override;
	// ~IEditorNotifyListener

	// ~IQueryHistoryListener
	virtual void OnQueryHistoryEvent(EEvent ev) override;
	// ~IQueryHistoryListener

	// IQueryHistoryConsumer
	virtual void AddHistoricQuery(const SHistoricQueryOverview& overview) override;
	virtual void AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* fmt, ...) override;
	virtual void AddTextLineToFocusedItem(const ColorF& color, const char* fmt, ...) override;
	virtual void AddInstantEvaluatorName(const char* szInstantEvaluatorName) override;
	virtual void AddDeferredEvaluatorName(const char* szDeferredEvaluatorName) override;
	// ~IQueryHistoryConsumer

private:
	void OnHistoryOriginComboBoxSelectionChanged(int index);
	void OnClearHistoryButtonClicked(bool checked);
	void OnSaveLiveHistoryToFile();
	void OnLoadHistoryFromFile();
	void OnTreeViewCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
	uqs::core::IQueryHistoryManager* m_pQueryHistoryManager;
	SQuery*                          m_pFreshlyAddedQuery;
	CHistoricQueryTreeView*          m_pTreeView;
	CHistoricQueryTreeModel*         m_pTreeModel;
	QTextEdit*                       m_pTextQueryDetails;
	QTextEdit*                       m_pTextItemDetails;
	QComboBox*                       m_pComboBoxHistoryOrigin;
	QPushButton*                     m_pButtonClearCurrentHistory;
	QPropertyTree*                   m_pPropertyTree;
};
