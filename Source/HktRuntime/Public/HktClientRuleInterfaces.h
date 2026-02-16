// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktRuntimeTypes.h"
#include "HktClientRuleInterfaces.generated.h"

// ============================================================================
// IHktClientSimulator - 클라이언트 시뮬레이터 인터페이스
//
// 용도: PlayerController에 붙는 컴포넌트 (ClientSimulatorComponent)
//
// 주요 메서드:
//   Execute(Batch)    : FrameBatch(입력)를 받아 시뮬레이션 실행
//   RestoreState(State): 초기 상태를 받아 시뮬레이션 복원 (신규 접속)
//   GetSimulationState(): 현재 시뮬레이션 상태 조회 (Newbie 접속, 중간 합류)
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktClientSimulator : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktClientSimulator
{
	GENERATED_BODY()

public:
	/** FrameBatch(입력)를 받아 시뮬레이션 실행 */
	virtual void Execute(const FHktSimulationEvent& InBatch) = 0;

	/** 초기 상태를 받아 시뮬레이션 복원 + 대기 중인 배치 처리 */
	virtual void RestoreState(const FHktWorldState& InState, TArray<FHktSimulationEvent>&& InPendingBatches) = 0;

	/** 현재 시뮬레이션 상태 조회 (Newbie 접속, 중간 합류 시 사용) */
	virtual const FHktWorldState& GetSimulationState() const = 0;

	/** 초기화 여부 확인 (RestoreState 호출 후 Execute 가능하면 true) */
	virtual bool IsInitialized() const = 0;
};

// ============================================================================
// IHktIntentBuilder - Intent 빌더 인터페이스
//
// 용도: PlayerController에 붙는 컴포넌트 (IntentBuilderComponent)
//
// 역할:
//   - Subject/Command/Target 설정
//   - Intent 생성 및 Submit
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktIntentBuilder : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktIntentBuilder
{
	GENERATED_BODY()

public:
	/** Subject 엔티티 설정 */
	virtual void SetSubject(FHktEntityId InSubject) = 0;

	/** Command 설정 (EventTag 및 Target 필요 여부) */
	virtual void SetCommand(FGameplayTag InEventTag, bool bInTargetRequired) = 0;

	/** Target 엔티티 및 위치 설정 */
	virtual void SetTarget(FHktEntityId InTarget, FVector InLocation) = 0;

	/** Command 초기화 */
	virtual void ResetCommand() = 0;

	/** Submit 준비 여부 확인 */
	virtual bool IsReadyToSubmit() const = 0;

	/** Intent Submit (서버로 전송) */
	virtual bool Submit() = 0;

	/** 현재 Subject 엔티티 ID 조회 */
	virtual FHktEntityId GetSubjectEntityId() const = 0;

	/** 현재 Target 엔티티 ID 조회 */
	virtual FHktEntityId GetTargetEntityId() const = 0;

	/** 현재 EventTag 조회 */
	virtual FGameplayTag GetEventTag() const = 0;

	/** Submit이 호출되어 대기 중인 Intent가 있는지 확인 */
	virtual bool HasPendingSubmit() const = 0;

	/** 대기 중인 Intent를 소비 (Actor가 RPC로 전송) */
	virtual FHktRuntimeEvent ConsumePendingSubmit() = 0;
};

// ============================================================================
// IHktUnitSelectionPolicy - 유닛 선택 정책 인터페이스
//
// 용도: PlayerController에 붙는 컴포넌트 (SelectionPolicyComponent)
//
// 역할:
//   - Subject 엔티티 해석 (마우스 커서 아래 선택 가능한 엔티티)
//   - Target 엔티티 및 위치 해석 (마우스 커서 아래 타겟)
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktUnitSelectionPolicy : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktUnitSelectionPolicy
{
	GENERATED_BODY()

public:
	/** Subject 엔티티 해석 */
	virtual FHktEntityId ResolveSubject() const = 0;

	/** Target 엔티티 및 위치 해석 */
	virtual void ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const = 0;
};

// ============================================================================
// IHktCommandContainer - Command 컨테이너 인터페이스
//
// 용도: PlayerController에 붙는 컴포넌트 (CommandContainerComponent)
//
// 역할:
//   - 슬롯별 Command 정보 제공 (EventTag, TargetRequired 여부)
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktCommandContainer : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktCommandContainer
{
	GENERATED_BODY()

public:
	/** 슬롯의 EventTag 조회 */
	virtual FGameplayTag GetEventTagAtSlot(int32 SlotIndex) const = 0;

	/** 슬롯의 Target 필요 여부 조회 */
	virtual bool IsTargetRequiredAtSlot(int32 SlotIndex) const = 0;

	/** 슬롯 개수 조회 */
	virtual int32 GetNumSlots() const = 0;

	/** SlotActions 배열 설정 */
	virtual void SetSlotActions(const TArray<TObjectPtr<UHktInputAction>>& InSlotActions) = 0;
};

//=============================================================================
// IHktClientRule
//=============================================================================

class HKTRUNTIME_API IHktClientRule
{
public:
    virtual ~IHktClientRule() = default;

    // === 유저 입력 ===
    virtual void OnUserEvent_LoginButtonClick() = 0;
    virtual void OnUserEvent_SubjectInputAction(const IHktUnitSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_TargetInputAction(const IHktUnitSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) = 0;

    // === 서버 수신 ===

    /** 신규 유저: 그룹 시뮬레이션 결과를 받아 즉시 동기화 (접속 직후 1회) */
    virtual void OnReceived_InitialSimulationState(const FHktWorldState& InState, IHktClientSimulator& InSimulator) = 0;

    /** 기존 유저: FrameBatch(입력)를 받아 로컬 시뮬레이션 수행 (매 프레임) */
    virtual void OnReceived_FrameBatch(const FHktSimulationEvent& InBatch, IHktClientSimulator& InSimulator) = 0;
};
