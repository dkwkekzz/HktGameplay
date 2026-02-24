// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPresentationRenderer.h"
#include "HktPresentationState.h"

class ULocalPlayer;

/** HktUI 연동 — Spawned/Removed 시 UI 요소 생성/제거. HealthRatio 등은 UI가 State 직접 참조. */
class FHktUIRenderer : public IHktPresentationRenderer
{
public:
	explicit FHktUIRenderer(ULocalPlayer* InLP);
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;

private:
	ULocalPlayer* LocalPlayer = nullptr;
};
