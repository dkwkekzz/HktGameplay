// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMInterpreter.h"
#include "HktVMProgram.h"
#include "HktVMContext.h"
#include "HktVMWorldStateProxy.h"
#include "HktCollisionLayers.h"
#include "HktCoreArchetype.h"
#include "GameplayTagsManager.h"
#include "HktCoreLog.h"
#include "HktCoreEventLog.h"
#include "Terrain/HktTerrainState.h"
#include "Terrain/HktTerrainDestructibility.h"
#include "HktSimulationSystems.h"

// ============================================================================
// Helper
// ============================================================================

const FString& FHktVMInterpreter::GetString(FHktVMRuntime& Runtime, int32 Index)
{
    static FString Empty;
    if (Runtime.Program && Index >= 0 && Index < Runtime.Program->Strings.Num())
        return Runtime.Program->Strings[Index];
    return Empty;
}

FGameplayTag FHktVMInterpreter::ResolveTag(int32 TagIndex)
{
    FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(TagIndex));
    return FGameplayTag::RequestGameplayTag(TagName, false);
}

// ============================================================================
// Entity
// ============================================================================

void FHktVMInterpreter::Op_SpawnEntity(FHktVMRuntime& Runtime, int32 TagIndex)
{
    if (WorldState)
    {
        FHktEntityId NewEntity = WorldState->AllocateEntity();
        Runtime.SetRegEntity(Reg::Spawned, NewEntity);

        // ClassTag를 영구 태그로 부여
        FGameplayTag ClassTag = ResolveTag(TagIndex);
        HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_SpawnEntity Id=%d ClassTag=%s Story=%s"),
                NewEntity, *ClassTag.ToString(),
                Runtime.Program ? *Runtime.Program->Tag.ToString() : TEXT("?")),
            NewEntity);
        if (ClassTag.IsValid() && VMProxy)
        {
            VMProxy->AddTag(*WorldState, NewEntity, ClassTag);

            // ClassTag에서 Archetype 자동 설정
            EHktArchetype Arch = FHktArchetypeRegistry::Get().FindByTag(ClassTag);
            if (Arch != EHktArchetype::None)
            {
                WorldState->SetArchetype(NewEntity, Arch);
            }
        }

#if ENABLE_HKT_INSIGHTS
        // 엔티티 디버그 정보 기록: 어떤 Story에서 어떤 ClassTag로 생성되었는지
        {
            FString StoryTag = Runtime.Program ? Runtime.Program->Tag.ToString() : TEXT("Unknown");
            int32 Slot = WorldState->GetSlot(NewEntity);
            WorldState->SetEntityDebugInfo(Slot, StoryTag, ClassTag.ToString(), WorldState->FrameNumber);
        }
#endif

        if (Runtime.Context)
        {
            FHktEntityId SelfEntity = Runtime.GetRegEntity(Reg::Self);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::OwnerEntity, SelfEntity);

            // EntitySpawnTag: net index for presentation visual lookup
            FGameplayTagNetIndex NetIndex = ClassTag.IsValid()
                ? UGameplayTagsManager::Get().GetNetIndexFromTag(ClassTag)
                : FGameplayTagNetIndex(0);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::EntitySpawnTag, static_cast<int32>(NetIndex));

            Runtime.Context->WriteEntity(NewEntity, PropertyId::Mass, 1);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::MaxSpeed, 100);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::CollisionRadius, 50);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::IsGrounded, 1);  // 기본: 지면 접지 (투사체 등은 Story에서 0으로 설정)

            // 시전자(Self)의 Team을 상속 (시전자가 없거나 유효하지 않으면 건너뜀)
            if (WorldState->IsValidEntity(SelfEntity))
            {
                int32 SelfTeam = Runtime.Context->ReadEntity(SelfEntity, PropertyId::Team);
                if (SelfTeam != 0)
                {
                    Runtime.Context->WriteEntity(NewEntity, PropertyId::Team, SelfTeam);
                }
            }

            // ClassTag 기반 기본 CollisionLayer/Mask 자동 설정
            Runtime.Context->WriteEntity(NewEntity, PropertyId::CollisionLayer,
                static_cast<int32>(GetDefaultCollisionLayer(ClassTag)));
            Runtime.Context->WriteEntity(NewEntity, PropertyId::CollisionMask,
                static_cast<int32>(GetDefaultCollisionMask(ClassTag)));

            if (Runtime.PlayerUid != 0 && VMProxy)
            {
                VMProxy->SetOwnerUid(*WorldState, NewEntity, Runtime.PlayerUid);
            }
        }
    }
}

void FHktVMInterpreter::Op_DestroyEntity(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    FHktEntityId E = Runtime.GetRegEntity(Entity);
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_DestroyEntity Id=%d"), E), E);

    WorldState->RemoveEntity(E);
}

// ============================================================================
// Spatial Query
// ============================================================================

void FHktVMInterpreter::Op_GetDistance(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2)
{
    if (Runtime.Context && WorldState)
    {
        FHktEntityId E1 = Runtime.GetRegEntity(Entity1);
        FHktEntityId E2 = Runtime.GetRegEntity(Entity2);

        int32 X1 = Runtime.Context->ReadEntity(E1, PropertyId::PosX);
        int32 Y1 = Runtime.Context->ReadEntity(E1, PropertyId::PosY);
        int32 Z1 = Runtime.Context->ReadEntity(E1, PropertyId::PosZ);

        int32 X2 = Runtime.Context->ReadEntity(E2, PropertyId::PosX);
        int32 Y2 = Runtime.Context->ReadEntity(E2, PropertyId::PosY);
        int32 Z2 = Runtime.Context->ReadEntity(E2, PropertyId::PosZ);

        int64 DX = X2 - X1;
        int64 DY = Y2 - Y1;
        int64 DZ = Z2 - Z1;

        int32 DistSq = static_cast<int32>(FMath::Min(static_cast<int64>(MAX_int32), DX * DX + DY * DY + DZ * DZ));
        Runtime.SetReg(Dst, static_cast<int32>(FMath::Sqrt(static_cast<float>(DistSq))));
    }
}

void FHktVMInterpreter::Op_LookAt(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex TargetEntity)
{
    if (Runtime.Context && VMProxy && WorldState)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        FHktEntityId T = Runtime.GetRegEntity(TargetEntity);

        int32 X1 = Runtime.Context->ReadEntity(E, PropertyId::PosX);
        int32 Y1 = Runtime.Context->ReadEntity(E, PropertyId::PosY);
        int32 X2 = Runtime.Context->ReadEntity(T, PropertyId::PosX);
        int32 Y2 = Runtime.Context->ReadEntity(T, PropertyId::PosY);

        float DX = static_cast<float>(X2 - X1);
        float DY = static_cast<float>(Y2 - Y1);

        if (DX * DX + DY * DY > 1.0f)
        {
            int32 YawDeg = FMath::RoundToInt(FMath::Atan2(DY, DX) * (180.0f / PI));
            VMProxy->SetPropertyDirty(*WorldState, E, PropertyId::RotYaw, YawDeg);
        }
    }
}

void FHktVMInterpreter::Op_FindInRadius(FHktVMRuntime& Runtime, RegisterIndex CenterEntity, int32 RadiusCm)
{
    Runtime.SpatialQuery.Reset();

    if (WorldState && Runtime.Context)
    {
        FHktEntityId Center = Runtime.GetRegEntity(CenterEntity);

        int32 CX = Runtime.Context->ReadEntity(Center, PropertyId::PosX);
        int32 CY = Runtime.Context->ReadEntity(Center, PropertyId::PosY);
        int32 CZ = Runtime.Context->ReadEntity(Center, PropertyId::PosZ);
        uint32 FilterMask = static_cast<uint32>(Runtime.Context->ReadEntity(Center, PropertyId::CollisionMask));

        int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

        WorldState->ForEachEntity([&](FHktEntityId E, int32 /*SlotIndex*/)
        {
            if (E == Center)
                return;

            // CollisionLayer 기반 필터링 (FilterMask == 0이면 모든 레이어 허용)
            if (FilterMask != 0)
            {
                uint32 TargetLayer = static_cast<uint32>(WorldState->GetProperty(E, PropertyId::CollisionLayer));
                if (TargetLayer != 0 && !(TargetLayer & FilterMask))
                    return;
            }

            FIntVector EP = WorldState->GetPosition(E);
            int64 DX = EP.X - CX;
            int64 DY = EP.Y - CY;
            int64 DZ = EP.Z - CZ;

            if (DX * DX + DY * DY + DZ * DZ <= RadiusSq)
                Runtime.SpatialQuery.Entities.Add(E);
        });

        HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("FindInRadius Center=%d Radius=%d Mask=0x%X Found=%d"),
                Center, RadiusCm, FilterMask, Runtime.SpatialQuery.Entities.Num()));
    }

    Runtime.SetReg(Reg::Count, Runtime.SpatialQuery.Entities.Num());
}

void FHktVMInterpreter::Op_FindInRadiusEx(FHktVMRuntime& Runtime, RegisterIndex CenterEntity, RegisterIndex FilterMaskReg, RegisterIndex RadiusReg)
{
    Runtime.SpatialQuery.Reset();

    if (WorldState && Runtime.Context)
    {
        FHktEntityId Center = Runtime.GetRegEntity(CenterEntity);
        uint32 FilterMask = static_cast<uint32>(Runtime.GetReg(FilterMaskReg));

        int32 CX = Runtime.Context->ReadEntity(Center, PropertyId::PosX);
        int32 CY = Runtime.Context->ReadEntity(Center, PropertyId::PosY);
        int32 CZ = Runtime.Context->ReadEntity(Center, PropertyId::PosZ);

        int32 RadiusCm = Runtime.GetReg(RadiusReg);
        int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

        WorldState->ForEachEntity([&](FHktEntityId E, int32 /*SlotIndex*/)
        {
            if (E == Center)
                return;

            // 명시적 마스크 필터링
            uint32 TargetLayer = static_cast<uint32>(WorldState->GetProperty(E, PropertyId::CollisionLayer));
            if (FilterMask != 0 && !(TargetLayer & FilterMask))
                return;

            FIntVector EP = WorldState->GetPosition(E);
            int64 DX = EP.X - CX;
            int64 DY = EP.Y - CY;
            int64 DZ = EP.Z - CZ;

            if (DX * DX + DY * DY + DZ * DZ <= RadiusSq)
                Runtime.SpatialQuery.Entities.Add(E);
        });

        HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("FindInRadiusEx Center=%d Radius=%d Mask=0x%X Found=%d"),
                Center, RadiusCm, FilterMask, Runtime.SpatialQuery.Entities.Num()));
    }

    Runtime.SetReg(Reg::Count, Runtime.SpatialQuery.Entities.Num());
}

void FHktVMInterpreter::Op_NextFound(FHktVMRuntime& Runtime)
{
    if (Runtime.SpatialQuery.HasNext())
    {
        Runtime.SetRegEntity(Reg::Iter, Runtime.SpatialQuery.Next());
        Runtime.SetReg(Reg::Flag, 1);
    }
    else
    {
        Runtime.SetRegEntity(Reg::Iter, InvalidEntityId);
        Runtime.SetReg(Reg::Flag, 0);
    }
}

// ============================================================================
// Presentation
// ============================================================================

void FHktVMInterpreter::Op_ApplyEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 TagIndex)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    FGameplayTag Tag = ResolveTag(TagIndex);
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_ApplyEffect Id=%d Effect=%s"), E, *Tag.ToString()), E);
}

void FHktVMInterpreter::Op_RemoveEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 TagIndex)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    FGameplayTag Tag = ResolveTag(TagIndex);
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_RemoveEffect Id=%d Effect=%s"), E, *Tag.ToString()), E);
}

void FHktVMInterpreter::Op_PlayVFX(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 TagIndex)
{
    if (!VMProxy) return;

    const int32 X = Runtime.GetReg(PosBase);
    const int32 Y = Runtime.GetReg(PosBase + 1);
    const int32 Z = Runtime.GetReg(PosBase + 2);

    FGameplayTag Tag = ResolveTag(TagIndex);
    if (!Tag.IsValid()) return;

    VMProxy->PendingVFXEvents.Add({ Tag, FIntVector(X, Y, Z) });

    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlayVFX Pos=(%d,%d,%d) VFX=%s"), X, Y, Z, *Tag.ToString()));
}

void FHktVMInterpreter::Op_PlayVFXAttached(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 TagIndex)
{
    if (!WorldState || !VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);
    FGameplayTag Tag = ResolveTag(TagIndex);
    if (!Tag.IsValid()) return;

    FIntVector Pos = WorldState->GetPosition(E);
    VMProxy->PendingVFXEvents.Add({ Tag, Pos });

    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlayVFXAttached Id=%d VFX=%s"), E, *Tag.ToString()), E);
}

void FHktVMInterpreter::Op_PlayAnim(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 TagIndex)
{
    if (!VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);
    FGameplayTag Tag = ResolveTag(TagIndex);
    if (!Tag.IsValid()) return;

    VMProxy->PendingAnimEvents.Add({ Tag, E });

    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlayAnim Id=%d Anim=%s"), E, *Tag.ToString()), E);
}

void FHktVMInterpreter::Op_PlaySound(FHktVMRuntime& Runtime, int32 TagIndex)
{
    FGameplayTag Tag = ResolveTag(TagIndex);
    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlaySound Sound=%s"), *Tag.ToString()));
}

void FHktVMInterpreter::Op_PlaySoundAtLocation(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 TagIndex)
{
    FGameplayTag Tag = ResolveTag(TagIndex);
    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlaySoundAtLocation Pos=(%d,%d,%d) Sound=%s"),
            Runtime.GetReg(PosBase), Runtime.GetReg(PosBase + 1), Runtime.GetReg(PosBase + 2),
            *Tag.ToString()));
}

// ============================================================================
// Tags
// ============================================================================

void FHktVMInterpreter::Op_AddTag(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 TagIndex)
{
    if (!WorldState || !VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);
    FGameplayTag Tag = ResolveTag(TagIndex);
    if (Tag.IsValid())
    {
        HKT_EVENT_LOG_TAG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_AddTag Id=%d Tag=%s"), E, *Tag.ToString()), E, Tag);
        VMProxy->AddTag(*WorldState, E, Tag);
    }
}

void FHktVMInterpreter::Op_RemoveTag(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 TagIndex)
{
    if (!WorldState || !VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);
    FGameplayTag Tag = ResolveTag(TagIndex);
    if (Tag.IsValid())
    {
        HKT_EVENT_LOG_TAG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_RemoveTag Id=%d Tag=%s"), E, *Tag.ToString()), E, Tag);
        VMProxy->RemoveTag(*WorldState, E, Tag);
    }
}

void FHktVMInterpreter::Op_HasTag(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity, int32 TagIndex)
{
    bool bHas = false;
    if (WorldState)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        FGameplayTag Tag = ResolveTag(TagIndex);
        if (Tag.IsValid())
            bHas = WorldState->HasTag(E, Tag);
    }
    Runtime.SetReg(Dst, bHas ? 1 : 0);
}

// ============================================================================
// NPC Spawning
// ============================================================================

void FHktVMInterpreter::Op_CountByTag(FHktVMRuntime& Runtime, RegisterIndex Dst, int32 TagIndex)
{
    int32 Count = 0;
    if (WorldState)
    {
        FGameplayTag Tag = ResolveTag(TagIndex);
        if (Tag.IsValid())
        {
            WorldState->ForEachEntity([&](FHktEntityId E, int32 /*Slot*/)
            {
                if (WorldState->HasTag(E, Tag))
                    ++Count;
            });
        }
    }
    Runtime.SetReg(Dst, Count);
}

void FHktVMInterpreter::Op_GetWorldTime(FHktVMRuntime& Runtime, RegisterIndex Dst)
{
    if (WorldState)
    {
        Runtime.SetReg(Dst, static_cast<int32>(WorldState->FrameNumber & 0x7FFFFFFF));
    }
    else
    {
        Runtime.SetReg(Dst, 0);
    }
}

void FHktVMInterpreter::Op_RandomInt(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex ModulusReg)
{
    int32 Modulus = Runtime.GetReg(ModulusReg);
    if (Modulus <= 0 || !WorldState)
    {
        Runtime.SetReg(Dst, 0);
        return;
    }

    int32 Hash = static_cast<int32>(WorldState->FrameNumber * 2654435761) ^ (WorldState->RandomSeed + Runtime.PC);
    Hash = (Hash < 0) ? -Hash : Hash;
    Runtime.SetReg(Dst, Hash % Modulus);
}

void FHktVMInterpreter::Op_HasPlayerInGroup(FHktVMRuntime& Runtime, RegisterIndex Dst)
{
    static const FGameplayTag CharacterTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Entity.Character")), false);
    bool bHasPlayer = false;
    if (WorldState)
    {
        for (int32 S = 0; S < WorldState->SlotToEntity.Num() && !bHasPlayer; ++S)
        {
            if (WorldState->SlotToEntity[S] != InvalidEntityId
                && WorldState->OwnerUids[S] != 0
                && WorldState->GetTagsBySlot(S).HasTag(CharacterTag))
                bHasPlayer = true;
        }
    }
    Runtime.SetReg(Dst, bHasPlayer ? 1 : 0);
}

// ============================================================================
// Item System
// ============================================================================

void FHktVMInterpreter::Op_CountByOwner(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex OwnerEntity, int32 TagIndex)
{
    int32 Count = 0;
    if (WorldState)
    {
        FGameplayTag FilterTag = ResolveTag(TagIndex);
        FHktEntityId OwnerId = Runtime.GetRegEntity(OwnerEntity);

        if (FilterTag.IsValid())
        {
            WorldState->ForEachEntity([&](FHktEntityId /*E*/, int32 Slot)
            {
                if (WorldState->Get(Slot, PropertyId::OwnerEntity) == OwnerId
                    && WorldState->GetTagsBySlot(Slot).HasTag(FilterTag))
                    ++Count;
            });
        }
    }
    Runtime.SetReg(Dst, Count);
}

void FHktVMInterpreter::Op_FindByOwner(FHktVMRuntime& Runtime, RegisterIndex OwnerEntity, int32 TagIndex)
{
    Runtime.SpatialQuery.Reset();

    if (WorldState)
    {
        FGameplayTag FilterTag = ResolveTag(TagIndex);
        FHktEntityId OwnerId = Runtime.GetRegEntity(OwnerEntity);

        if (FilterTag.IsValid())
        {
            WorldState->ForEachEntity([&](FHktEntityId E, int32 Slot)
            {
                if (WorldState->Get(Slot, PropertyId::OwnerEntity) == OwnerId
                    && WorldState->GetTagsBySlot(Slot).HasTag(FilterTag))
                    Runtime.SpatialQuery.Entities.Add(E);
            });
        }
    }

    Runtime.SetReg(Reg::Count, Runtime.SpatialQuery.Entities.Num());
}

void FHktVMInterpreter::Op_SetOwnerUid(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    if (WorldState && VMProxy && Runtime.PlayerUid != 0)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_SetOwnerUid Id=%d Uid=%lld"), E, Runtime.PlayerUid), E);
        VMProxy->SetOwnerUid(*WorldState, E, Runtime.PlayerUid);
    }
}

void FHktVMInterpreter::Op_ClearOwnerUid(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    if (WorldState && VMProxy)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_ClearOwnerUid Id=%d"), E), E);
        VMProxy->SetOwnerUid(*WorldState, E, 0);
    }
}

// ============================================================================
// Event Dispatch
// ============================================================================

void FHktVMInterpreter::Op_DispatchEvent(FHktVMRuntime& Runtime, int32 TagNetIndex)
{
    FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(TagNetIndex));
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TagName);
    if (!EventTag.IsValid())
    {
        HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Error, LogSource, FString::Printf(TEXT("Op_DispatchEvent: invalid NetIndex %d"), TagNetIndex));
        return;
    }

    FHktEvent Event;
    Event.EventTag = EventTag;
    Event.SourceEntity = Runtime.Context ? Runtime.Context->SourceEntity : InvalidEntityId;
    Event.TargetEntity = Runtime.Context ? Runtime.Context->TargetEntity : InvalidEntityId;
    Event.PlayerUid = Runtime.PlayerUid;
    if (Runtime.Context)
    {
        Event.Location = FVector(
            static_cast<float>(Runtime.Context->EventTargetPosX),
            static_cast<float>(Runtime.Context->EventTargetPosY),
            static_cast<float>(Runtime.Context->EventTargetPosZ));
        Event.Param0 = Runtime.Context->EventParam0;
        Event.Param1 = Runtime.Context->EventParam1;
    }

    Runtime.PendingDispatchedEvents.Add(Event);

    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_DispatchEvent: %s Src=%d Tgt=%d"),
            *EventTag.ToString(), Event.SourceEntity, Event.TargetEntity),
        Event.SourceEntity);
}

void FHktVMInterpreter::Op_DispatchEventTo(FHktVMRuntime& Runtime, RegisterIndex TargetReg, int32 TagNetIndex)
{
    FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(TagNetIndex));
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TagName);
    if (!EventTag.IsValid())
    {
        UE_LOG(LogHktCore, Error, TEXT("Op_DispatchEventTo: invalid NetIndex %d"), TagNetIndex);
        return;
    }

    FHktEvent Event;
    Event.EventTag = EventTag;
    Event.SourceEntity = Runtime.Context ? Runtime.Context->SourceEntity : InvalidEntityId;
    Event.TargetEntity = Runtime.GetRegEntity(TargetReg);
    Event.PlayerUid = Runtime.PlayerUid;
    if (Runtime.Context)
    {
        Event.Location = FVector(
            static_cast<float>(Runtime.Context->EventTargetPosX),
            static_cast<float>(Runtime.Context->EventTargetPosY),
            static_cast<float>(Runtime.Context->EventTargetPosZ));
        Event.Param0 = Runtime.Context->EventParam0;
        Event.Param1 = Runtime.Context->EventParam1;
    }

    Runtime.PendingDispatchedEvents.Add(Event);

    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_DispatchEventTo: %s Src=%d Tgt=%d"),
            *EventTag.ToString(), Event.SourceEntity, Event.TargetEntity),
        Event.SourceEntity);
}

void FHktVMInterpreter::Op_DispatchEventFrom(FHktVMRuntime& Runtime, RegisterIndex SourceReg, int32 TagNetIndex)
{
    FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(TagNetIndex));
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TagName);
    if (!EventTag.IsValid())
    {
        UE_LOG(LogHktCore, Error, TEXT("Op_DispatchEventFrom: invalid NetIndex %d"), TagNetIndex);
        return;
    }

    FHktEvent Event;
    Event.EventTag = EventTag;
    Event.SourceEntity = Runtime.GetRegEntity(SourceReg);
    Event.TargetEntity = Runtime.Context ? Runtime.Context->TargetEntity : InvalidEntityId;
    Event.PlayerUid = Runtime.PlayerUid;
    if (Runtime.Context)
    {
        Event.Location = FVector(
            static_cast<float>(Runtime.Context->EventTargetPosX),
            static_cast<float>(Runtime.Context->EventTargetPosY),
            static_cast<float>(Runtime.Context->EventTargetPosZ));
        Event.Param0 = Runtime.Context->EventParam0;
        Event.Param1 = Runtime.Context->EventParam1;
    }

    Runtime.PendingDispatchedEvents.Add(Event);

    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_DispatchEventFrom: %s Src=%d Tgt=%d"),
            *EventTag.ToString(), Event.SourceEntity, Event.TargetEntity),
        Event.SourceEntity);
}

// ============================================================================
// Movement
// ============================================================================

void FHktVMInterpreter::Op_SetForwardTarget(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    if (Runtime.Context && VMProxy && WorldState)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);

        const int32 PosX = Runtime.Context->ReadEntity(E, PropertyId::PosX);
        const int32 PosY = Runtime.Context->ReadEntity(E, PropertyId::PosY);
        const int32 PosZ = Runtime.Context->ReadEntity(E, PropertyId::PosZ);
        const int32 YawDeg = Runtime.Context->ReadEntity(E, PropertyId::RotYaw);

        const float YawRad = static_cast<float>(YawDeg) * (PI / 180.0f);
        constexpr float ForwardRange = 100000.0f; // 1km

        const int32 TgtX = PosX + FMath::RoundToInt(FMath::Cos(YawRad) * ForwardRange);
        const int32 TgtY = PosY + FMath::RoundToInt(FMath::Sin(YawRad) * ForwardRange);
        const int32 TgtZ = PosZ;

        VMProxy->SetPropertyDirty(*WorldState, E, PropertyId::MoveTargetX, TgtX);
        VMProxy->SetPropertyDirty(*WorldState, E, PropertyId::MoveTargetY, TgtY);
        VMProxy->SetPropertyDirty(*WorldState, E, PropertyId::MoveTargetZ, TgtZ);
    }
}

// ============================================================================
// Terrain
// ============================================================================

void FHktVMInterpreter::Op_GetTerrainHeight(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex XReg, RegisterIndex YReg)
{
    if (!TerrainState)
    {
        Runtime.SetReg(Dst, 0);
        return;
    }

    const int32 VoxelX = Runtime.GetReg(XReg);
    const int32 VoxelY = Runtime.GetReg(YReg);
    const int32 Height = TerrainState->GetSurfaceHeightAt(VoxelX, VoxelY);
    Runtime.SetReg(Dst, Height);

    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_GetTerrainHeight VoxelXY=(%d,%d) Height=%d"), VoxelX, VoxelY, Height));
}

void FHktVMInterpreter::Op_GetVoxelType(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex PosBase, RegisterIndex ZReg)
{
    if (!TerrainState)
    {
        Runtime.SetReg(Dst, 0);
        return;
    }

    const int32 X = Runtime.GetReg(PosBase);
    const int32 Y = Runtime.GetReg(static_cast<RegisterIndex>(PosBase + 1));
    const int32 Z = Runtime.GetReg(ZReg);
    const uint16 TypeID = TerrainState->GetVoxelType(X, Y, Z);
    Runtime.SetReg(Dst, static_cast<int32>(TypeID));

    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_GetVoxelType Pos=(%d,%d,%d) TypeID=%d"), X, Y, Z, TypeID));
}

void FHktVMInterpreter::Op_SetVoxel(FHktVMRuntime& Runtime, RegisterIndex PosBase, RegisterIndex TypeReg)
{
    if (!TerrainState || !PendingVoxelDeltas)
    {
        return;
    }

    const int32 X = Runtime.GetReg(PosBase);
    const int32 Y = Runtime.GetReg(static_cast<RegisterIndex>(PosBase + 1));
    const int32 Z = Runtime.GetReg(static_cast<RegisterIndex>(PosBase + 2));
    const uint16 TypeID = static_cast<uint16>(Runtime.GetReg(TypeReg));

    FHktTerrainVoxel Voxel;
    Voxel.TypeID = TypeID;
    TerrainState->SetVoxel(X, Y, Z, Voxel, *PendingVoxelDeltas);

    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_SetVoxel Pos=(%d,%d,%d) TypeID=%d"), X, Y, Z, TypeID));
}

void FHktVMInterpreter::Op_IsTerrainSolid(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex PosBase, RegisterIndex ZReg)
{
    if (!TerrainState)
    {
        Runtime.SetReg(Dst, 0);
        return;
    }

    const int32 X = Runtime.GetReg(PosBase);
    const int32 Y = Runtime.GetReg(static_cast<RegisterIndex>(PosBase + 1));
    const int32 Z = Runtime.GetReg(ZReg);
    const bool bSolid = TerrainState->IsSolid(X, Y, Z);
    Runtime.SetReg(Dst, bSolid ? 1 : 0);
}

// ============================================================================
// Terrain — FindTerrainInRadius (entity + terrain 통합 검색)
// ============================================================================

void FHktVMInterpreter::Op_FindTerrainInRadius(FHktVMRuntime& Runtime, RegisterIndex CenterEntity, int32 RadiusCm)
{
    Runtime.SpatialQuery.Reset();

    if (!WorldState || !Runtime.Context)
    {
        Runtime.SetReg(Reg::Count, 0);
        return;
    }

    const FHktEntityId Center = Runtime.GetRegEntity(CenterEntity);
    const int32 CX = Runtime.Context->ReadEntity(Center, PropertyId::PosX);
    const int32 CY = Runtime.Context->ReadEntity(Center, PropertyId::PosY);
    const int32 CZ = Runtime.Context->ReadEntity(Center, PropertyId::PosZ);
    const uint32 FilterMask = static_cast<uint32>(Runtime.Context->ReadEntity(Center, PropertyId::CollisionMask));
    const int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

    // --- Phase 1: entity 검색 (기존 FindInRadius 동일) ---
    WorldState->ForEachEntity([&](FHktEntityId E, int32 /*SlotIndex*/)
    {
        if (E == Center)
            return;

        if (FilterMask != 0)
        {
            uint32 TargetLayer = static_cast<uint32>(WorldState->GetProperty(E, PropertyId::CollisionLayer));
            if (TargetLayer != 0 && !(TargetLayer & FilterMask))
                return;
        }

        FIntVector EP = WorldState->GetPosition(E);
        int64 DX = EP.X - CX;
        int64 DY = EP.Y - CY;
        int64 DZ = EP.Z - CZ;

        if (DX * DX + DY * DY + DZ * DZ <= RadiusSq)
            Runtime.SpatialQuery.Entities.Add(E);
    });

    // --- Phase 2: terrain voxel 스캔 ---
    if (TerrainState && PendingVoxelDeltas && VMProxy)
    {
        const FIntVector CenterVoxel = FHktTerrainSystem::CmToVoxel(CX, CY, CZ);
        const int32 VoxelRadius = FMath::CeilToInt(static_cast<float>(RadiusCm) / FHktTerrainSystem::VoxelSizeCm);

        static constexpr int32 MaxDebrisPerQuery = 8;  // 한 번의 공격에 생성되는 최대 Debris 수
        int32 DebrisCount = 0;

        // Entity.Debris ClassTag 해석 (정적 캐싱)
        static FGameplayTag DebrisClassTag;
        if (!DebrisClassTag.IsValid())
        {
            DebrisClassTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Entity.Debris")));
        }

        // DebrisLifecycle Story Tag
        static FGameplayTag DebrisLifecycleTag;
        if (!DebrisLifecycleTag.IsValid())
        {
            DebrisLifecycleTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Story.Flow.Debris.Lifecycle")));
        }

        for (int32 dz = -VoxelRadius; dz <= VoxelRadius && DebrisCount < MaxDebrisPerQuery; ++dz)
        {
            for (int32 dy = -VoxelRadius; dy <= VoxelRadius && DebrisCount < MaxDebrisPerQuery; ++dy)
            {
                for (int32 dx = -VoxelRadius; dx <= VoxelRadius && DebrisCount < MaxDebrisPerQuery; ++dx)
                {
                    const int32 VX = CenterVoxel.X + dx;
                    const int32 VY = CenterVoxel.Y + dy;
                    const int32 VZ = CenterVoxel.Z + dz;

                    // cm 단위 거리 체크
                    const FIntVector VCm = FHktTerrainSystem::VoxelToCm(VX, VY, VZ);
                    const int64 DDX = VCm.X - CX;
                    const int64 DDY = VCm.Y - CY;
                    const int64 DDZ = VCm.Z - CZ;
                    if (DDX * DDX + DDY * DDY + DDZ * DDZ > RadiusSq)
                        continue;

                    // 파괴 가능 여부 확인
                    const uint16 TypeId = TerrainState->GetVoxelType(VX, VY, VZ);
                    if (TypeId == 0 || !HktTerrainDestructibility::IsDestructible(TypeId))
                        continue;

                    // a. voxel 제거 (Air로 설정)
                    FHktTerrainVoxel EmptyVoxel;
                    EmptyVoxel.TypeID = 0;
                    EmptyVoxel.PaletteIndex = 0;
                    EmptyVoxel.Flags = 0;
                    TerrainState->SetVoxel(VX, VY, VZ, EmptyVoxel, *PendingVoxelDeltas);

                    // b. Debris entity 생성
                    const FHktEntityId DebrisId = WorldState->AllocateEntity();

                    VMProxy->AddTag(*WorldState, DebrisId, DebrisClassTag);
                    WorldState->SetArchetype(DebrisId, EHktArchetype::Debris);

                    // 프로퍼티 설정
                    const int32 DebrisHealth = HktTerrainDestructibility::GetHealth(TypeId);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::PosX, VCm.X);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::PosY, VCm.Y);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::PosZ, VCm.Z);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::Health, DebrisHealth);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::MaxHealth, DebrisHealth);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::TerrainTypeId, static_cast<int32>(TypeId));
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::CollisionRadius, 15);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::Mass, 1);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::IsGrounded, 1);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::CollisionLayer,
                        static_cast<int32>(GetDefaultCollisionLayer(DebrisClassTag)));
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::CollisionMask,
                        static_cast<int32>(GetDefaultCollisionMask(DebrisClassTag)));

                    // EntitySpawnTag for presentation
                    FGameplayTagNetIndex NetIndex = UGameplayTagsManager::Get().GetNetIndexFromTag(DebrisClassTag);
                    Runtime.Context->WriteEntity(DebrisId, PropertyId::EntitySpawnTag, static_cast<int32>(NetIndex));

                    // c. lifecycle dispatch
                    FHktEvent LifecycleEvt;
                    LifecycleEvt.EventTag = DebrisLifecycleTag;
                    LifecycleEvt.SourceEntity = DebrisId;
                    LifecycleEvt.PlayerUid = Runtime.PlayerUid;
                    Runtime.PendingDispatchedEvents.Add(LifecycleEvt);

                    // d. SpatialQuery에 추가
                    Runtime.SpatialQuery.Entities.Add(DebrisId);
                    ++DebrisCount;
                }
            }
        }
    }

    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("FindTerrainInRadius Center=%d Radius=%d Found=%d"),
            Center, RadiusCm, Runtime.SpatialQuery.Entities.Num()));

    Runtime.SetReg(Reg::Count, Runtime.SpatialQuery.Entities.Num());
}

// ============================================================================
// Utility
// ============================================================================

void FHktVMInterpreter::Op_Log(FHktVMRuntime& Runtime, int32 StringIndex)
{
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_Log: %s"), *GetString(Runtime, StringIndex)),
        Runtime.Context ? Runtime.Context->SourceEntity : InvalidEntityId);
}
