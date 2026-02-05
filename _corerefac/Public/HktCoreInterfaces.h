// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HktCoreTypes.h"

//=============================================================================
// IHktStashInterface - 순수 C++ Stash 인터페이스
//=============================================================================

/**
 * IHktStashInterface - Stash 공통 인터페이스 (Pure C++)
 *
 * Entity는 두 가지 데이터를 가짐:
 * - Properties: 숫자 데이터 (위치, 체력, 공격력 등)
 * - Tags: GameplayTagContainer (Visual, Flow, Status 등)
 */
class HKTCORE_API IHktStashInterface
{
public:
    virtual ~IHktStashInterface() = default;

    // ========== Entity Management ==========
    virtual bool IsValidEntity(FHktEntityId Entity) const = 0;
    virtual FHktEntityId AllocateEntity() = 0;
    virtual void FreeEntity(FHktEntityId Entity) = 0;
    virtual int32 GetEntityCount() const = 0;

    // ========== Property API (숫자 데이터) ==========
    virtual int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const = 0;
    virtual void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value) = 0;

    // ========== Tag API (GameplayTagContainer) ==========
    virtual const FGameplayTagContainer& GetTags(FHktEntityId Entity) const = 0;
    virtual void SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) = 0;
    virtual void AddTag(FHktEntityId Entity, const FGameplayTag& Tag) = 0;
    virtual void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag) = 0;
    virtual bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const = 0;
    virtual bool HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const = 0;
    virtual bool HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const = 0;
    virtual bool HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const = 0;

    // ========== Tag Query Helpers ==========
    virtual FGameplayTag GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const = 0;
    virtual FGameplayTagContainer GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const = 0;

    // ========== Frame Management ==========
    virtual int32 GetCompletedFrameNumber() const = 0;
    virtual void MarkFrameCompleted(int32 FrameNumber) = 0;

    // ========== Iteration ==========
    virtual void ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const = 0;

    // ========== Checksum ==========
    virtual uint32 CalculateChecksum() const = 0;
};

//=============================================================================
// IHktMasterStashInterface - 서버 전용 확장 인터페이스
//=============================================================================

/**
 * IHktMasterStashInterface - MasterStash 전용 인터페이스 (서버)
 *
 * 스냅샷 생성, 위치 관리, 셀 기반 Relevancy 등 서버 전용 기능 제공
 */
class HKTCORE_API IHktMasterStashInterface : public IHktStashInterface
{
public:
    // ========== Batch Operations ==========
    struct FPendingWrite
    {
        FHktEntityId Entity;
        uint16 PropertyId;
        int32 Value;
    };
    virtual void ApplyWrites(const TArray<FPendingWrite>& Writes) = 0;

    // ========== Frame Validation ==========
    virtual bool ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const = 0;

    // ========== Snapshot & Delta ==========
    virtual FHktEntitySnapshot CreateEntitySnapshot(FHktEntityId Entity) const = 0;
    virtual TArray<FHktEntitySnapshot> CreateSnapshots(const TArray<FHktEntityId>& Entities) const = 0;
    virtual TArray<uint8> SerializeFullState() const = 0;
    virtual void DeserializeFullState(const TArray<uint8>& Data) = 0;

    // ========== Position Access ==========
    virtual bool TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const = 0;
    virtual void SetPosition(FHktEntityId Entity, const FVector& Position) = 0;

    // ========== Partial Checksum ==========
    virtual uint32 CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const = 0;

    // ========== Radius Query ==========
    virtual void ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const = 0;

    // ========== Cell-based Spatial Indexing ==========

    /** 셀 크기 설정 (cm 단위) */
    virtual void SetCellSize(float InCellSize) = 0;

    /** 현재 셀 크기 조회 */
    virtual float GetCellSize() const = 0;

    /** 엔티티의 현재 셀 조회 */
    virtual FIntPoint GetEntityCell(FHktEntityId Entity) const = 0;

    /** 특정 셀 내의 모든 엔티티 조회 */
    virtual const TSet<FHktEntityId>* GetEntitiesInCell(FIntPoint Cell) const = 0;

    /** 이번 프레임의 셀 변경 이벤트 가져오기 (호출 후 클리어됨) */
    virtual TArray<FHktCellChangeEvent> ConsumeCellChangeEvents() = 0;

    /** 여러 셀의 모든 엔티티 수집 */
    virtual void GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const = 0;
};

//=============================================================================
// IHktVisibleStashInterface - 클라이언트 전용 확장 인터페이스
//=============================================================================

/**
 * IHktVisibleStashInterface - VisibleStash 전용 인터페이스 (클라이언트)
 *
 * 스냅샷 적용, 클리어 등 클라이언트 전용 기능 제공
 */
class HKTCORE_API IHktVisibleStashInterface : public IHktStashInterface
{
public:
    // ========== Batch Operations ==========
    struct FPendingWrite
    {
        FHktEntityId Entity;
        uint16 PropertyId;
        int32 Value;
    };
    virtual void ApplyWrites(const TArray<FPendingWrite>& Writes) = 0;

    // ========== Snapshot Sync ==========
    virtual void ApplyEntitySnapshot(const FHktEntitySnapshot& Snapshot) = 0;
    virtual void ApplySnapshots(const TArray<FHktEntitySnapshot>& Snapshots) = 0;

    // ========== Clear ==========
    virtual void Clear() = 0;
};

//=============================================================================
// IHktVMProcessorInterface - VM 프로세서 인터페이스
//=============================================================================

/**
 * IHktVMProcessorInterface - VMProcessor 외부 사용 인터페이스 (Pure C++)
 *
 * 외부 모듈(HktRuntime 등)에서 VMProcessor를 사용하기 위한 인터페이스
 * 실제 구현은 HktCore의 FHktVMProcessor에서 제공
 */
class HKTCORE_API IHktVMProcessorInterface
{
public:
    virtual ~IHktVMProcessorInterface() = default;

    /** 프레임 처리 (Build → Execute → Cleanup 파이프라인) */
    virtual void Tick(int32 CurrentFrame, float DeltaSeconds) = 0;

    /** Intent 이벤트 알림 */
    virtual void NotifyIntentEvent(const FHktIntentEvent& Event) = 0;

    /** 충돌 알림 (큐에 적재, Execute에서 일괄 처리) */
    virtual void NotifyCollision(FHktEntityId WatchedEntity, FHktEntityId HitEntity) = 0;
};

//=============================================================================
// FHktWorldStateAdapter - FHktWorldState → IHktMasterStashInterface 어댑터
//=============================================================================

// Forward declarations
class FHktWorldState;
class FHktSpatialSystem;

/**
 * FHktWorldStateAdapter - 새로운 FHktWorldState를 기존 IHktMasterStashInterface로 래핑
 *
 * 외부 모듈(HktRuntime)이 아직 IHktMasterStashInterface를 사용하는 동안
 * 내부적으로 FHktWorldState + FHktSpatialSystem에 위임합니다.
 *
 * Phase 2에서 외부 모듈이 FHktWorldState를 직접 사용하도록 전환 후 제거 예정.
 */
class HKTCORE_API FHktWorldStateAdapter : public IHktMasterStashInterface
{
public:
    FHktWorldStateAdapter(FHktWorldState* InWorldState, FHktSpatialSystem* InSpatialSystem);

    // ========== IHktStashInterface ==========
    virtual bool IsValidEntity(FHktEntityId Entity) const override;
    virtual FHktEntityId AllocateEntity() override;
    virtual void FreeEntity(FHktEntityId Entity) override;
    virtual int32 GetEntityCount() const override;
    virtual int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const override;
    virtual void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value) override;
    virtual const FGameplayTagContainer& GetTags(FHktEntityId Entity) const override;
    virtual void SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) override;
    virtual void AddTag(FHktEntityId Entity, const FGameplayTag& Tag) override;
    virtual void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag) override;
    virtual bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const override;
    virtual bool HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const override;
    virtual bool HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const override;
    virtual bool HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const override;
    virtual FGameplayTag GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const override;
    virtual FGameplayTagContainer GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const override;
    virtual int32 GetCompletedFrameNumber() const override;
    virtual void MarkFrameCompleted(int32 FrameNumber) override;
    virtual void ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const override;
    virtual uint32 CalculateChecksum() const override;

    // ========== IHktMasterStashInterface ==========
    virtual void ApplyWrites(const TArray<IHktMasterStashInterface::FPendingWrite>& Writes) override;
    virtual bool ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const override;
    virtual FHktEntitySnapshot CreateEntitySnapshot(FHktEntityId Entity) const override;
    virtual TArray<FHktEntitySnapshot> CreateSnapshots(const TArray<FHktEntityId>& Entities) const override;
    virtual TArray<uint8> SerializeFullState() const override;
    virtual void DeserializeFullState(const TArray<uint8>& Data) override;
    virtual bool TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const override;
    virtual void SetPosition(FHktEntityId Entity, const FVector& Position) override;
    virtual uint32 CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const override;
    virtual void ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const override;

    // Cell API → SpatialSystem에 위임
    virtual void SetCellSize(float InCellSize) override;
    virtual float GetCellSize() const override;
    virtual FIntPoint GetEntityCell(FHktEntityId Entity) const override;
    virtual const TSet<FHktEntityId>* GetEntitiesInCell(FIntPoint Cell) const override;
    virtual TArray<FHktCellChangeEvent> ConsumeCellChangeEvents() override;
    virtual void GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const override;

private:
    FHktWorldState* WorldState;
    FHktSpatialSystem* SpatialSystem;
};

//=============================================================================
// 팩토리 함수 선언
//=============================================================================

/**
 * VMProcessor 인스턴스 생성
 *
 * 호출자가 반환된 포인터의 수명을 관리해야 함
 * TUniquePtr로 래핑하여 사용 권장
 */
HKTCORE_API TUniquePtr<IHktVMProcessorInterface> CreateVMProcessor(IHktStashInterface* InStash);

/**
 * MasterStash 인스턴스 생성 (서버 전용)
 */
HKTCORE_API TUniquePtr<IHktMasterStashInterface> CreateMasterStash();

/**
 * VisibleStash 인스턴스 생성 (클라이언트 전용)
 */
HKTCORE_API TUniquePtr<IHktVisibleStashInterface> CreateVisibleStash();
