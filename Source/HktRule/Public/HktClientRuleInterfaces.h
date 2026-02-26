// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"
#include "HktCoreEvents.h"
#include "HktWorldState.h"
#include "HktClientRuleInterfaces.generated.h"

// ============================================================================
// IHktProxySimulator
// ============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktProxySimulator : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktProxySimulator
{
	GENERATED_BODY()
public:
	virtual void RestoreState(const FHktWorldState& InState) = 0;
	virtual const FHktWorldState& GetWorldState() const = 0;
	virtual bool IsInitialized() const = 0;
	virtual void AdvanceLocalFrame(float DeltaSeconds) = 0;

	/** 서버 Batch를 큐에 적재 — 다음 틱에서 롤백/빨리감기 처리 */
	virtual void EnqueueServerBatch(const FHktSimulationEvent& InBatch) = 0;

	/** 마지막 조정으로 생성된 Diff를 소비 (없으면 false) */
	virtual bool ConsumePendingDiff(FHktSimulationDiff& OutDiff) = 0;
};

// ============================================================================
// IHktIntentBuilder
// ============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktIntentBuilder : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktIntentBuilder
{
	GENERATED_BODY()
public:
	virtual void SetSubject(FHktEntityId InSubject) = 0;
	virtual void SetCommand(FGameplayTag InEventTag, bool bInTargetRequired) = 0;
	virtual void SetTarget(FHktEntityId InTarget, FVector InLocation) = 0;
	virtual void ResetCommand() = 0;
	virtual bool IsReadyToSubmit() const = 0;
	virtual bool Submit() = 0;
	virtual FHktEntityId GetSubjectEntityId() const = 0;
	virtual FHktEntityId GetTargetEntityId() const = 0;
	virtual FGameplayTag GetEventTag() const = 0;
	virtual bool HasPendingSubmit() const = 0;
	virtual FHktEvent ConsumePendingSubmit() = 0;
};

// ============================================================================
// IHktUnitSelectionPolicy
// ============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktUnitSelectionPolicy : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktUnitSelectionPolicy
{
	GENERATED_BODY()
public:
	virtual FHktEntityId ResolveSubject() const = 0;
	virtual void ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const = 0;
};

// ============================================================================
// IHktCommandContainer
// ============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktCommandContainer : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktCommandContainer
{
	GENERATED_BODY()
public:
	virtual FGameplayTag GetEventTagAtSlot(int32 SlotIndex) const = 0;
	virtual bool IsTargetRequiredAtSlot(int32 SlotIndex) const = 0;
	virtual int32 GetNumSlots() const = 0;
	virtual void SetSlotActions(const TArray<TObjectPtr<UObject>>& InSlotActions) = 0;
};

// ============================================================================
// IHktClientRule
// ============================================================================
class HKTRULE_API IHktClientRule
{
public:
	virtual ~IHktClientRule() = default;

	/** 컨텍스트 바인딩 — 서버 룰과 동일한 패턴 (ServerRule::BindContext와 일관성 유지) */
	virtual void BindContext(
		IHktProxySimulator*       InSimulator,
		IHktIntentBuilder*        InBuilder,
		IHktUnitSelectionPolicy*  InPolicy,
		IHktCommandContainer*     InContainer) {}

	virtual void OnUserEvent_LoginButtonClick() {}

	/** 내부 캐싱된 Policy/Builder 사용 */
	virtual void OnUserEvent_SubjectInputAction() {}
	virtual void OnUserEvent_TargetInputAction() {}
	virtual void OnUserEvent_CommandInputAction(int32 InSlotIndex) {}
	virtual void OnUserEvent_ZoomInputAction(float InDelta) {}

	/** 내부 캐싱된 Simulator 사용 */
	virtual void OnReceived_InitialState(const FHktWorldState& InState) {}
	virtual FHktSimulationDiff OnReceived_FrameBatch(const FHktSimulationEvent& InBatch) { return {}; }
};

namespace HktRule
{
	HKTRULE_API IHktClientRule* GetClientRule(UWorld* World);
}
