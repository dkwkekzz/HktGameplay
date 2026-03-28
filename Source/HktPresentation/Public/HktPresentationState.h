// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "HktVisualField.h"
#include "HktWorldState.h"
#include "HktCoreProperties.h"
#include "Math/Color.h"
#include "UObject/SoftObjectPath.h"

/** 엔터티의 렌더 카테고리 (어떤 렌더러가 담당할지 결정) */
enum class EHktRenderCategory : uint8
{
	None = 0,
	Actor,
	MassEntity,
	FX,
};

/** 단일 엔터티의 렌더 ViewModel — 모든 프레젠테이션 필드를 플랫하게 보유 */
struct FHktEntityPresentation
{
	FHktEntityId EntityId = InvalidEntityId;
	EHktRenderCategory RenderCategory = EHktRenderCategory::None;
	int64 SpawnedFrame = 0;
	int64 RemovedFrame = 0;
	int64 LastDirtyFrame = -1;

	// --- Transform ---
	THktVisualField<FVector> Location;
	THktVisualField<FRotator> Rotation;
	THktVisualField<FVector> RenderLocation;       // Location + 지면 트레이스 + 캡슐 오프셋 적용된 최종 렌더 위치
	float CapsuleHalfHeight = 0.f;                 // DataAsset CDO에서 해결. 렌더 위치 계산에 사용

	// --- Physics (Debug) ---
	THktVisualField<float> CollisionRadius;
	THktVisualField<int32> CollisionLayer;

	// --- Movement ---
	THktVisualField<FVector> MoveTarget;
	THktVisualField<float> MoveForce;
	THktVisualField<bool> bIsMoving;
	THktVisualField<FVector> Velocity;

	// --- Vitals ---
	THktVisualField<float> Health;
	THktVisualField<float> MaxHealth;
	THktVisualField<float> HealthRatio;
	THktVisualField<float> Mana;
	THktVisualField<float> MaxMana;
	THktVisualField<float> ManaRatio;

	// --- Combat ---
	THktVisualField<int32> AttackPower;
	THktVisualField<int32> Defense;
	THktVisualField<int32> CP;
	THktVisualField<int32> MaxCP;
	THktVisualField<float> CPRatio;
	THktVisualField<int32> AttackSpeed;

	// --- Ownership ---
	THktVisualField<int32> Team;
	THktVisualField<int64> OwnedPlayerUid;

	// --- Animation ---
	THktVisualField<FGameplayTag> AnimState;
	THktVisualField<FGameplayTag> MontageState;
	THktVisualField<FGameplayTag> AnimStateUpper;
	THktVisualField<FGameplayTag> Stance;

	// --- Visualization ---
	THktVisualField<FGameplayTag> VisualElement;
	THktVisualField<FSoftObjectPath> ResolvedAssetPath;   // VisualElement에서 비동기 해결된 에셋 경로

	// --- Pre-computed Display ---
	THktVisualField<FString> OwnerLabel;                   // "P:12345" 또는 "-"
	THktVisualField<FLinearColor> TeamColor;                // Team 인덱스에서 계산된 색상

	// --- Item ---
	THktVisualField<int32> OwnerEntity;   // 소유 캐릭터 EntityId (0 = 없음)
	THktVisualField<int32> EquipIndex;    // -1 = 미등록, 0+ = 장착 슬롯
	THktVisualField<int32> ItemState;

	/** Entity의 GameplayTag 컨테이너 (AnimInstance 태그 동기화용) */
	FGameplayTagContainer Tags;
	int64 TagsDirtyFrame = -1;

	/** 이번 프레임에 수신된 일회성 애니메이션 이벤트 (소비 후 Clear) */
	TArray<FGameplayTag> PendingAnimTriggers;

	void InitFromWorldState(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	void ApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
	void ApplyOwnerDelta(int64 NewOwnerUid, int64 Frame);

	HKTPRESENTATION_API bool IsAlive() const;
	HKTPRESENTATION_API bool IsSpawnedAt(int64 Frame) const;
	HKTPRESENTATION_API bool IsRemovedAt(int64 Frame) const;
	HKTPRESENTATION_API bool IsItemAttached() const { return OwnerEntity.Get() != InvalidEntityId && ItemState.Get() == 2; }

	HKTPRESENTATION_API static EHktRenderCategory DetermineRenderCategory(const FGameplayTagContainer& Tags);

	/** Team 인덱스에서 팀 색상 반환 */
	HKTPRESENTATION_API static FLinearColor GetTeamColor(int32 TeamIndex);

private:
	void ComputeOwnerLabel(int64 Uid, int64 Frame);
	void ComputeTeamColor(int32 TeamIndex, int64 Frame);
};

/** 전체 Presentation 상태 (렌더러가 그대로 읽어서 그리는 ViewModel) */
struct HKTPRESENTATION_API FHktPresentationState
{
	TArray<FHktEntityPresentation> Entities;
	TBitArray<> ValidMask;
	int64 CurrentFrame = 0;

	TArray<FHktEntityId> SpawnedThisFrame;
	TArray<FHktEntityId> RemovedThisFrame;
	TArray<FHktEntityId> DirtyThisFrame;

	void BeginFrame(int64 Frame);
	void EnsureCapacity(FHktEntityId MaxId);
	void AddEntity(const FHktWorldState& WS, FHktEntityId Id);
	void RemoveEntity(FHktEntityId Id);
	void ApplyDelta(FHktEntityId Id, uint16 PropId, int32 NewValue);
	void ApplyOwnerDelta(FHktEntityId Id, int64 NewOwnerUid);
	void ApplyTagDelta(FHktEntityId Id, const FGameplayTagContainer& NewTags);

	bool IsValid(FHktEntityId Id) const;
	const FHktEntityPresentation* Get(FHktEntityId Id) const;
	FHktEntityPresentation* GetMutable(FHktEntityId Id);
	int64 GetCurrentFrame() const;

	template<typename F>
	void ForEachEntity(F&& Cb) const
	{
		for (int32 i = 0; i < Entities.Num(); ++i)
			if (ValidMask[i]) Cb(Entities[i]);
	}

	void Clear();
};
