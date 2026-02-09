// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HktRuntimeTypes.h"
#include "HktCoreInterfaces.h"
#include "HktRuntimeDelegates.h"
#include "HktModelProvider.generated.h"

// ============================================================================
// IHktModelProvider 인터페이스
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktModelProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * IHktModelProvider
 *
 */
class HKTRUNTIME_API IHktModelProvider
{
	GENERATED_BODY()

public:
	// ========== Stash 접근 ==========

	/** 엔티티 데이터 읽기용 Stash 인터페이스 */
	virtual IHktStashInterface* GetStashInterface() const = 0;

	// ========== Intent Builder 상태 ==========

	/** 현재 선택된 Subject EntityId */
	virtual FHktEntityId GetSelectedSubject() const = 0;

	/** 현재 선택된 Target EntityId */
	virtual FHktEntityId GetSelectedTarget() const = 0;

	/** 현재 타겟 위치 (Ground 클릭 등) */
	virtual FVector GetTargetLocation() const = 0;

	/** 현재 선택된 커맨드 태그 */
	virtual FGameplayTag GetSelectedCommand() const = 0;

	/** Intent 빌더가 유효한 상태인지 */
	virtual bool IsIntentValid() const = 0;

	// ========== 델리게이트 ==========

	/** Subject 선택 변경 시 */
	virtual FOnHktSubjectChanged& OnSubjectChanged() = 0;

	/** Target 선택 변경 시 */
	virtual FOnHktTargetChanged& OnTargetChanged() = 0;

	/** Command 선택 변경 시 */
	virtual FOnHktCommandChanged& OnCommandChanged() = 0;

	/** Intent 제출 시 */
	virtual FOnHktIntentSubmitted& OnIntentSubmitted() = 0;

	/** 휠 입력 시 (줌용) */
	virtual FOnHktWheelInput& OnWheelInput() = 0;

	/** 엔티티 생성 시 (Presentation 연결용) */
	virtual FOnHktEntityCreated& OnEntityCreated() = 0;

	/** 엔티티 제거 시 (Presentation 연결용) */
	virtual FOnHktEntityDestroyed& OnEntityDestroyed() = 0;
};
