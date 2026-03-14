#pragma once

#include "CoreMinimal.h"
#include "HktClientRuleInterfaces.h"

class HKTRULE_API FHktDefaultClientRule : public IHktClientRule
{
public:
	FHktDefaultClientRule();
	virtual ~FHktDefaultClientRule();

	// 컨텍스트 바인딩
	virtual void BindContext(
		IHktProxySimulator*      InSimulator,
		IHktIntentBuilder*       InBuilder,
		IHktUnitSelectionPolicy* InPolicy,
		IHktCommandContainer*    InContainer) override;

	virtual void OnUserEvent_LoginButtonClick() override;
	virtual void OnUserEvent_SubjectInputAction() override;
	virtual void OnUserEvent_TargetInputAction() override;
	virtual void OnUserEvent_CommandInputAction(int32 InSlotIndex) override;
	virtual void OnUserEvent_ZoomInputAction(float InDelta) override;
	virtual void OnReceived_InitialState(const FHktWorldState& InState, int32 InGroupIndex) override;
	virtual void OnReceived_FrameBatch(const FHktSimulationEvent& InBatch) override;

private:
	// 바인딩된 컨텍스트
	IHktProxySimulator*      CachedSimulator  = nullptr;
	IHktIntentBuilder*       CachedBuilder    = nullptr;
	IHktUnitSelectionPolicy* CachedPolicy     = nullptr;
	IHktCommandContainer*    CachedContainer  = nullptr;

	TArray<FHktSimulationEvent> PendingBatches;
};
