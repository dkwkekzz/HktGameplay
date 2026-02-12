// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "IHktUserEventDispatcher.generated.h"

class UHktEventParam;
class IHktStashInterface;

// ============================================================================
// 델리게이트
// ============================================================================

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHktEntityEvent, FHktEntityId, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHktEntityCreated, FHktEntityId);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHktEntityDestroyed, FHktEntityId);

// ============================================================================
// 엔티티 위치 정보 (위젯 부착용)
// ============================================================================

/**
 * 엔티티의 월드 위치 정보.
 * Actor든 MassEntity든 상관없이 위치만 제공.
 */
USTRUCT(BlueprintType)
struct FHktEntityLocationInfo
{
	GENERATED_BODY()

	/** 엔티티가 유효한지 */
	UPROPERTY(BlueprintReadOnly)
	bool bIsValid = false;

	/** 월드 위치 */
	UPROPERTY(BlueprintReadOnly)
	FVector WorldLocation = FVector::ZeroVector;

	/** 위젯 부착 오프셋 (머리 위 등) */
	UPROPERTY(BlueprintReadOnly)
	FVector AttachOffset = FVector(0.0f, 0.0f, 120.0f);
};

// ============================================================================
// IHktUserEventDispatcher
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktUserEventDispatcher : public UInterface
{
	GENERATED_BODY()
};

/**
 * IHktUserEventDispatcher
 *
 * PlayerController가 구현하는 이벤트 디스패처 인터페이스.
 *
 * 역할:
 * 1. UI → 서버 이벤트 전달 (DispatchUserEvent)
 * 2. 엔티티 생성/파괴/상태 이벤트 제공
 * 3. 엔티티 위치 정보 제공 (Actor/MassEntity 무관)
 * 4. Stash 인터페이스 제공 (HUD 데이터 읽기)
 */
class HKTRUNTIME_API IHktUserEventDispatcher
{
	GENERATED_BODY()

public:
	// ========== 유저 이벤트 디스패치 ==========

	/**
	 * UI에서 발생한 이벤트를 PlayerController에 전달
	 * @param EventTag 이벤트 종류
	 * @param Param 파라미터 + 콜백 UObject (nullable)
	 */
	virtual void DispatchUserEvent(const FGameplayTag& EventTag, UHktEventParam* Param) = 0;

	// ========== 엔티티 이벤트 구독 ==========

	virtual FOnHktEntityCreated& OnEntityCreated() = 0;
	virtual FOnHktEntityDestroyed& OnEntityDestroyed() = 0;
	virtual FOnHktEntityEvent& OnEntityEvent() = 0;

	// ========== 엔티티 정보 조회 ==========

	/**
	 * 엔티티의 월드 위치 정보를 반환.
	 * Actor, MassEntity, Stash 기반 등 구현에 무관하게 위치를 제공.
	 */
	virtual FHktEntityLocationInfo GetEntityLocationInfo(FHktEntityId EntityId) const = 0;

	/**
	 * 엔티티 데이터 읽기용 Stash 인터페이스.
	 * HUD 위젯이 체력/마나 등을 읽을 때 사용.
	 */
	virtual IHktStashInterface* GetStashInterface() const = 0;
};
