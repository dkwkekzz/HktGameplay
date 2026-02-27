// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "HktPresentationViewModels.h"
#include "HktWorldState.h"

/** 엔터티의 렌더 카테고리 (어떤 렌더러가 담당할지 결정) */
enum class EHktRenderCategory : uint8
{
	None = 0,
	Actor,
	MassEntity,
	FX,
};

/** 단일 엔터티의 렌더 ViewModel (Generation Counter 기반) */
struct FHktEntityPresentation
{
	FHktEntityId EntityId = InvalidEntityId;
	FHktTypeId TypeId = HktType::None;
	EHktRenderCategory RenderCategory = EHktRenderCategory::None;
	int64 SpawnedFrame = 0;
	int64 RemovedFrame = 0;

	FHktVM_Transform Transform;
	FHktVM_Movement Movement;
	FHktVM_Vitals Vitals;
	FHktVM_Combat Combat;
	FHktVM_Ownership Ownership;
	FHktVM_Animation Animation;
	FHktVM_Visualization Visualization;

	void InitFromWorldState(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	void ApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
	void ApplyOwnerDelta(int64 NewOwnerUid, int64 Frame);

	bool IsAlive() const;
	bool IsSpawnedAt(int64 Frame) const;
	bool IsRemovedAt(int64 Frame) const;

	static EHktRenderCategory DetermineRenderCategory(FHktTypeId Type);
};

/** 전체 Presentation 상태 (렌더러가 그대로 읽어서 그리는 ViewModel) */
struct FHktPresentationState
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

private:
	bool IsEntityDirtyThisFrame(const FHktEntityPresentation& E) const;
};
