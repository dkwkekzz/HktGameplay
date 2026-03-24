// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUIRenderer.h"

FHktUIRenderer::FHktUIRenderer(ULocalPlayer* InLP)
	: LocalPlayer(InLP)
{
}

void FHktUIRenderer::Sync(const FHktPresentationState& State)
{
	if (RegisteredRenderer)
	{
		RegisteredRenderer->Sync(State);
	}
}

void FHktUIRenderer::Teardown()
{
	RegisteredRenderer = nullptr;
}

bool FHktUIRenderer::NeedsTick() const
{
	return RegisteredRenderer && RegisteredRenderer->NeedsTick();
}

void FHktUIRenderer::RegisterRenderer(IHktPresentationRenderer* InRenderer)
{
	RegisteredRenderer = InRenderer;
}

void FHktUIRenderer::UnregisterRenderer(IHktPresentationRenderer* InRenderer)
{
	if (RegisteredRenderer == InRenderer)
	{
		RegisteredRenderer = nullptr;
	}
}
