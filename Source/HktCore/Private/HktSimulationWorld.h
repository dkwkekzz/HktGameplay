// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktEventTypes.h"

//class FHktSimulationWorld
//    {
//    public:
//        FHktSimulationWorld() 
//            : VMProcessor(&WorldState, &SpatialSystem)
//            , PublishedSnapshotPtr(nullptr)
//        {}
//
//        ~FHktSimulationWorld()
//        {
//            if (PublishedSnapshotPtr) delete (FHktFrameSnapshot*)PublishedSnapshotPtr;
//            for (auto* Ptr : GarbageSnapshotBin) delete Ptr;
//        }
//
//        void ProcessBatch(const FHktFrameBatch& Batch)
//        {
//            // 1. [Pre-Tick] 엔티티 삭제
//            for (int64 RemovedID : Batch.RemovedOwnerIds)
//            {
//                FEntityID ID = (FEntityID)RemovedID;
//                if (const FComponentData* Data = WorldState.GetComponent(ID))
//                {
//                    SpatialSystem.RemoveEntity(ID, Data->Position);
//                    WorldState.RemoveEntity(ID);
//                }
//            }
//
//            // 2. [Logic] 외부 입력 배치 즉시 실행 (Zero-Copy Execution)
//            // 큐에 넣는 과정 없이 들어온 배열을 그대로 순회합니다.
//            VMProcessor.ExecuteIntentBatch(Batch.Events, Batch.DeltaSeconds, Batch.RandomSeed);
//
//            // 3. [Collision] 충돌 감지 및 위치 보정
//            TArray<FHktPhysicsEvent> NewPhysicsEvents;
//            SpatialSystem.ResolveCollisionsAndGenEvents(WorldState, NewPhysicsEvents);
//
//            // 4. [Reaction] 물리 반응 즉시 실행
//            // 발생한 충돌 리스트를 그대로 VM에 넘겨 처리합니다.
//            VMProcessor.ExecutePhysicsBatch(NewPhysicsEvents);
//
//            // 5. [Publish] 스냅샷 발행
//            PublishSnapshotAtomic(Batch.FrameNumber, NewPhysicsEvents);
//            ProcessGarbageBin(Batch.FrameNumber);
//        }
//
//        void RestoreState(const FHktGroupSimulationState& InState)
//        {
//            WorldState.LoadFromGroupState(InState);
//            SpatialSystem.Rebuild(WorldState);
//            // VMProcessor는 이제 Stateless하므로 ResetQueue 호출 불필요
//            PublishSnapshotAtomic(InState.FrameNumber, {});
//        }
//
//        void SaveState(FHktGroupSimulationState& OutState) const
//        {
//            WorldState.SaveToGroupState(OutState);
//        }
//
//        const FHktFrameSnapshot* GetLastSnapshot() const
//        {
//            return (const FHktFrameSnapshot*)FPlatformAtomics::AtomicReadPtr((void* volatile*)&PublishedSnapshotPtr);
//        }
//
//    private:
//        void PublishSnapshotAtomic(int64 FrameNumber, const TArray<FHktPhysicsEvent>& PhysicsEvents)
//        {
//            FHktFrameSnapshot* NewSnapshot = new FHktFrameSnapshot();
//            NewSnapshot->FrameNumber = FrameNumber;
//            NewSnapshot->PhysicsEvents = PhysicsEvents; 
//            WorldState.ExtractRenderData(NewSnapshot->Entities);
//
//            void* OldSnapshot = FPlatformAtomics::InterlockedExchangePtr((void* volatile*)&PublishedSnapshotPtr, NewSnapshot);
//
//            if (OldSnapshot)
//                GarbageSnapshotBin.Enqueue({ (FHktFrameSnapshot*)OldSnapshot, FrameNumber });
//        }
//
//        void ProcessGarbageBin(int64 CurrentFrame)
//        {
//            const int64 SafetyGap = 3; 
//            while (!GarbageSnapshotBin.IsEmpty())
//            {
//                FGarbageItem* PeekItem = GarbageSnapshotBin.Peek();
//                if (PeekItem && (CurrentFrame - PeekItem->DeletedFrame > SafetyGap))
//                {
//                    FGarbageItem Item;
//                    GarbageSnapshotBin.Dequeue(Item);
//                    delete Item.Snapshot;
//                }
//                else break;
//            }
//        }
//
//    private:
//        FHktSpatialSystem SpatialSystem;
//        FHktWorldState WorldState;
//        FHktVMProcessor VMProcessor;
//
//        void* volatile PublishedSnapshotPtr;
//
//        struct FGarbageItem
//        {
//            FHktFrameSnapshot* Snapshot;
//            int64 DeletedFrame;
//        };
//        TQueue<FGarbageItem> GarbageSnapshotBin;
//    };