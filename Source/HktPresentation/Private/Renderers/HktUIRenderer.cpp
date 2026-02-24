// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUIRenderer.h"

FHktUIRenderer::FHktUIRenderer(ULocalPlayer* InLP)
	: LocalPlayer(InLP)
{
}

void FHktUIRenderer::Sync(const FHktPresentationState& State)
{
	// TODO: IngameHUD 등에 OnPresentationEntitySpawned/Removed 브로드캐스트 또는 State 직접 참조
	(void)State;
}

void FHktUIRenderer::Teardown()
{
}
