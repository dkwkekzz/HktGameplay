// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMInterpreter.h"
#include "HktVMProgram.h"
#include "HktVMContext.h"
#include "HktVMWorldStateProxy.h"
#include "GameplayTagsManager.h"
#include "HktCoreLog.h"
#include "HktCoreEventLog.h"

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

// ============================================================================
// Entity
// ============================================================================

void FHktVMInterpreter::Op_SpawnEntity(FHktVMRuntime& Runtime, int32 StringIndex)
{
    if (WorldState)
    {
        FHktEntityId NewEntity = WorldState->AllocateEntity();
        Runtime.SetRegEntity(Reg::Spawned, NewEntity);

        // ClassTag를 영구 태그로 부여
        const FString& TagName = GetString(Runtime, StringIndex);
        HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_SpawnEntity Id=%d ClassTag=%s Story=%s"),
                NewEntity, *TagName,
                Runtime.Program ? *Runtime.Program->Tag.ToString() : TEXT("?")),
            NewEntity);
        FGameplayTag ClassTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        if (ClassTag.IsValid() && VMProxy)
        {
            VMProxy->AddTag(*WorldState, NewEntity, ClassTag);
        }

#if ENABLE_HKT_INSIGHTS
        // 엔티티 디버그 정보 기록: 어떤 Story에서 어떤 ClassTag로 생성되었는지
        {
            FString StoryTag = Runtime.Program ? Runtime.Program->Tag.ToString() : TEXT("Unknown");
            int32 Slot = WorldState->GetSlot(NewEntity);
            WorldState->SetEntityDebugInfo(Slot, StoryTag, TagName, WorldState->FrameNumber);
        }
#endif

        if (Runtime.Context)
        {
            Runtime.Context->WriteEntity(NewEntity, PropertyId::OwnerEntity, Runtime.GetRegEntity(Reg::Self));

            // EntitySpawnTag: net index for presentation visual lookup
            FGameplayTagNetIndex NetIndex = ClassTag.IsValid()
                ? UGameplayTagsManager::Get().GetNetIndexFromTag(ClassTag)
                : FGameplayTagNetIndex(0);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::EntitySpawnTag, static_cast<int32>(NetIndex));

            Runtime.Context->WriteEntity(NewEntity, PropertyId::Mass, 1);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::MaxSpeed, 100);
            Runtime.Context->WriteEntity(NewEntity, PropertyId::CollisionRadius, 50);

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
        int32 Team = Runtime.Context->ReadEntity(Center, PropertyId::Team);

        int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

        WorldState->ForEachEntity([&](FHktEntityId E, int32 /*SlotIndex*/)
        {
            if (E == Center)
                return;

            if (WorldState->GetProperty(E, PropertyId::Team) == Team)
                return;

            FIntVector EP = WorldState->GetPosition(E);
            int64 DX = EP.X - CX;
            int64 DY = EP.Y - CY;
            int64 DZ = EP.Z - CZ;

            if (DX * DX + DY * DY + DZ * DZ <= RadiusSq)
                Runtime.SpatialQuery.Entities.Add(E);
        });
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

void FHktVMInterpreter::Op_ApplyEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    const FString& Effect = GetString(Runtime, StringIndex);
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_ApplyEffect Id=%d Effect=%s"), E, *Effect), E);
}

void FHktVMInterpreter::Op_RemoveEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    const FString& Effect = GetString(Runtime, StringIndex);
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_RemoveEffect Id=%d Effect=%s"), E, *Effect), E);
}

void FHktVMInterpreter::Op_PlayVFX(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex)
{
    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlayVFX Pos=(%d,%d,%d) VFX=%s"),
            Runtime.GetReg(PosBase), Runtime.GetReg(PosBase + 1), Runtime.GetReg(PosBase + 2),
            *GetString(Runtime, StringIndex)));
}

void FHktVMInterpreter::Op_PlayVFXAttached(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    if (!WorldState || !VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);
    const FString& VFXName = GetString(Runtime, StringIndex);
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*VFXName), false);
    if (Tag.IsValid())
    {
        VMProxy->AddTag(*WorldState, E, Tag);
    }
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlayVFXAttached Id=%d VFX=%s"), E, *VFXName), E);
}

void FHktVMInterpreter::Op_PlaySound(FHktVMRuntime& Runtime, int32 StringIndex)
{
    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlaySound Sound=%s"), *GetString(Runtime, StringIndex)));
}

void FHktVMInterpreter::Op_PlaySoundAtLocation(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex)
{
    HKT_EVENT_LOG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_PlaySoundAtLocation Pos=(%d,%d,%d) Sound=%s"),
            Runtime.GetReg(PosBase), Runtime.GetReg(PosBase + 1), Runtime.GetReg(PosBase + 2),
            *GetString(Runtime, StringIndex)));
}

// ============================================================================
// Tags
// ============================================================================

void FHktVMInterpreter::Op_AddTag(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    if (!WorldState || !VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);

    const FString& TagName = GetString(Runtime, StringIndex);
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (Tag.IsValid())
    {
        HKT_EVENT_LOG_TAG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_AddTag Id=%d Tag=%s"), E, *TagName), E, Tag);
        VMProxy->AddTag(*WorldState, E, Tag);
    }
}

void FHktVMInterpreter::Op_RemoveTag(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    if (!WorldState || !VMProxy) return;

    FHktEntityId E = Runtime.GetRegEntity(Entity);

    const FString& TagName = GetString(Runtime, StringIndex);
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (Tag.IsValid())
    {
        HKT_EVENT_LOG_TAG(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
            FString::Printf(TEXT("Op_RemoveTag Id=%d Tag=%s"), E, *TagName), E, Tag);
        VMProxy->RemoveTag(*WorldState, E, Tag);
    }
}

void FHktVMInterpreter::Op_HasTag(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity, int32 StringIndex)
{
    bool bHas = false;
    if (WorldState)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        const FString& TagName = GetString(Runtime, StringIndex);
        FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        if (Tag.IsValid())
            bHas = WorldState->HasTag(E, Tag);
    }
    Runtime.SetReg(Dst, bHas ? 1 : 0);
}

// ============================================================================
// NPC Spawning
// ============================================================================

void FHktVMInterpreter::Op_CountByTag(FHktVMRuntime& Runtime, RegisterIndex Dst, int32 StringIndex)
{
    int32 Count = 0;
    if (WorldState)
    {
        const FString& TagName = GetString(Runtime, StringIndex);
        FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
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

void FHktVMInterpreter::Op_CountByOwner(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex OwnerEntity, int32 StringIndex)
{
    int32 Count = 0;
    if (WorldState)
    {
        const FString& TagName = GetString(Runtime, StringIndex);
        FGameplayTag FilterTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
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

void FHktVMInterpreter::Op_FindByOwner(FHktVMRuntime& Runtime, RegisterIndex OwnerEntity, int32 StringIndex)
{
    Runtime.SpatialQuery.Reset();

    if (WorldState)
    {
        const FString& TagName = GetString(Runtime, StringIndex);
        FGameplayTag FilterTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
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

// ============================================================================
// Utility
// ============================================================================

void FHktVMInterpreter::Op_Log(FHktVMRuntime& Runtime, int32 StringIndex)
{
    HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM, EHktLogLevel::Info, LogSource,
        FString::Printf(TEXT("Op_Log: %s"), *GetString(Runtime, StringIndex)),
        Runtime.Context ? Runtime.Context->SourceEntity : InvalidEntityId);
}
