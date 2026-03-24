// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPresentationRenderer.h"
#include "HktPresentationState.h"

class ULocalPlayer;

/** 외부 IHktPresentationRenderer(예: AHktIngameHUD)에 Sync를 위임하는 프록시 렌더러. */
class FHktUIRenderer : public IHktPresentationRenderer
{
public:
	explicit FHktUIRenderer(ULocalPlayer* InLP);
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;
	virtual bool NeedsTick() const override;

	/** 외부 IHktPresentationRenderer 등록/해제 */
	void RegisterRenderer(IHktPresentationRenderer* InRenderer);
	void UnregisterRenderer(IHktPresentationRenderer* InRenderer);

private:
	ULocalPlayer* LocalPlayer = nullptr;
	IHktPresentationRenderer* RegisteredRenderer = nullptr;
};
