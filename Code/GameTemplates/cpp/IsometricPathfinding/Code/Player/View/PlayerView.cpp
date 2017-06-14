#include "StdAfx.h"
#include "PlayerView.h"

#include "Player/Player.h"
#include "Player/Input/PlayerInput.h"
#include "Player/Movement/PlayerMovement.h"

#include <IViewSystem.h>
#include <CryAnimation/ICryAnimation.h>

CPlayerView::CPlayerView()
{
}

CPlayerView::~CPlayerView()
{
	GetGameObject()->ReleaseView(this);
}

void CPlayerView::PostInit(IGameObject *pGameObject)
{
	m_pPlayer = static_cast<CPlayer *>(pGameObject->QueryExtension("Player"));

	// Register for UpdateView callbacks
	GetGameObject()->CaptureView(this);
}

void CPlayerView::ProcessEvent(SEntityEvent &event)
{
	if (event.event == ENTITY_EVENT_DONE)
	{
		GetGameObject()->ReleaseView(this);
	}
}

void CPlayerView::UpdateView(SViewParams &viewParams)
{
	IEntity &entity = *GetEntity();

	// Create rotation, facing the player
	viewParams.rotation = Quat::CreateRotationXYZ(Ang3(DEG2RAD(-45), 0, DEG2RAD(-45)));

	viewParams.position = entity.GetWorldPos() - viewParams.rotation.GetColumn1() * m_pPlayer->GetCVars().m_viewDistanceFromPlayer;
}