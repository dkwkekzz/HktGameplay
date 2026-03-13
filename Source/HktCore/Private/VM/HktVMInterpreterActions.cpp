// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMInterpreter.h"
#include "HktVMProgram.h"
#include "HktVMContext.h"
#include "HktVMWorldStateProxy.h"
#include "GameplayTagsManager.h"

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
// Entity Management
// ============================================================================

void FHktVMInterpreter::Op_SpawnEntity(FHktVMRuntime& Runtime, int32 StringIndex)
{
    if (WorldState)
    {
        FHktEntityId NewEntity = WorldState->AllocateEntity();
        Runtime.SetRegEntity(Reg::Spawned, NewEntity);

        // ClassTag를 영구 태그로 부여
        const FString& TagName = GetString(Runtime, StringIndex);
        FGameplayTag ClassTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        if (ClassTag.IsValid() && VMProxy)
        {
            VMProxy->AddTag(*WorldState, NewEntity, ClassTag);
        }

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
    UE_LOG(LogTemp, Log, TEXT("[VM] DestroyEntity: %d"), E);

    // 엔티티 제거는 즉시 적용 (다른 VM이 참조하지 못하게)
    if (WorldState)
    {
        WorldState->RemoveEntity(E);
    }
}

// ============================================================================
// Position & Movement
// ============================================================================

void FHktVMInterpreter::Op_GetPosition(FHktVMRuntime& Runtime, RegisterIndex DstBase, RegisterIndex Entity)
{
    if (Runtime.Context)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        Runtime.SetReg(DstBase, Runtime.Context->ReadEntity(E, PropertyId::PosX));
        Runtime.SetReg(DstBase + 1, Runtime.Context->ReadEntity(E, PropertyId::PosY));
        Runtime.SetReg(DstBase + 2, Runtime.Context->ReadEntity(E, PropertyId::PosZ));
    }
}

void FHktVMInterpreter::Op_SetPosition(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex SrcBase)
{
    if (Runtime.Context)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Context->WriteEntity(E, PropertyId::PosX, Runtime.GetReg(SrcBase));
        Runtime.Context->WriteEntity(E, PropertyId::PosY, Runtime.GetReg(SrcBase + 1));
        Runtime.Context->WriteEntity(E, PropertyId::PosZ, Runtime.GetReg(SrcBase + 2));
    }
}

void FHktVMInterpreter::Op_GetDistance(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2)
{
    if (Runtime.Context)
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

void FHktVMInterpreter::Op_MoveToward(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex TargetBase, int32 Speed)
{
    if (Runtime.Context)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Context->WriteEntity(E, PropertyId::MoveTargetX, Runtime.GetReg(TargetBase));
        Runtime.Context->WriteEntity(E, PropertyId::MoveTargetY, Runtime.GetReg(TargetBase + 1));
        Runtime.Context->WriteEntity(E, PropertyId::MoveTargetZ, Runtime.GetReg(TargetBase + 2));
        Runtime.Context->WriteEntity(E, PropertyId::MoveForce, Speed);

        // 관성 유지: 기존의 속도 초기화 코드를 제거했습니다. 
        // 이동 중에 새로운 타겟이 주어져도 현재 속도를 유지하며 부드럽게 선회합니다.
        Runtime.Context->WriteEntity(E, PropertyId::IsMoving, 1);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] MoveToward: Entity %d, Force %d"), Runtime.GetRegEntity(Entity), Speed);
}

void FHktVMInterpreter::Op_MoveForward(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 Speed)
{
    if (Runtime.Context)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Context->WriteEntity(E, PropertyId::MoveForce, Speed);
        // 여기서도 연속적인 이동 명령을 위해 속도를 강제 초기화하지 않는 것이 좋습니다.
        Runtime.Context->WriteEntity(E, PropertyId::IsMoving, 1);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] MoveForward: Entity %d, Force %d"), Runtime.GetRegEntity(Entity), Speed);
}

void FHktVMInterpreter::Op_StopMovement(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    if (Runtime.Context)
    {
        FHktEntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Context->WriteEntity(E, PropertyId::IsMoving, 0);
        Runtime.Context->WriteEntity(E, PropertyId::VelX, 0);
        Runtime.Context->WriteEntity(E, PropertyId::VelY, 0);
        Runtime.Context->WriteEntity(E, PropertyId::VelZ, 0);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] StopMovement: Entity %d"), Runtime.GetRegEntity(Entity));
}

// ============================================================================
// Spatial Query
// ============================================================================

void FHktVMInterpreter::Op_FindInRadius(FHktVMRuntime& Runtime, RegisterIndex CenterEntity, int32 RadiusCm)
{
    Runtime.SpatialQuery.Reset();

    if (WorldState && Runtime.Context)
    {
        FHktEntityId Center = Runtime.GetRegEntity(CenterEntity);

        // 중심 위치는 Store에서 읽기 (현재 VM의 로컬 캐시 반영)
        int32 CX = Runtime.Context->ReadEntity(Center, PropertyId::PosX);
        int32 CY = Runtime.Context->ReadEntity(Center, PropertyId::PosY);
        int32 CZ = Runtime.Context->ReadEntity(Center, PropertyId::PosZ);
        int32 Team = Runtime.Context->ReadEntity(Center, PropertyId::Team);

        int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

        // WorldState 순회
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
    UE_LOG(LogTemp, Log, TEXT("[VM] FindInRadius: Found %d entities"), Runtime.SpatialQuery.Entities.Num());
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
// Combat
// ============================================================================

void FHktVMInterpreter::Op_ApplyDamage(FHktVMRuntime& Runtime, RegisterIndex Target, RegisterIndex Amount)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    int32 Dmg = Runtime.GetReg(Amount);

    UE_LOG(LogTemp, Log, TEXT("[VM] ApplyDamage: Entity %d takes %d damage"), E, Dmg);

    if (Runtime.Context && WorldState && WorldState->IsValidEntity(E))
    {
        int32 Health = Runtime.Context->ReadEntity(E, PropertyId::Health);
        int32 Defense = Runtime.Context->ReadEntity(E, PropertyId::Defense);

        int32 ActualDmg = FMath::Max(1, Dmg - Defense);
        int32 NewHealth = FMath::Max(0, Health - ActualDmg);

        Runtime.Context->WriteEntity(E, PropertyId::Health, NewHealth);
    }
}

void FHktVMInterpreter::Op_ApplyEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    const FString& Effect = GetString(Runtime, StringIndex);
    UE_LOG(LogTemp, Log, TEXT("[VM] ApplyEffect: Entity %d, Effect %s"), E, *Effect);
}

void FHktVMInterpreter::Op_RemoveEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex)
{
    FHktEntityId E = Runtime.GetRegEntity(Target);
    const FString& Effect = GetString(Runtime, StringIndex);
    UE_LOG(LogTemp, Log, TEXT("[VM] RemoveEffect: Entity %d, Effect %s"), E, *Effect);
}

// ============================================================================
// VFX
// ============================================================================

void FHktVMInterpreter::Op_PlayVFX(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlayVFX: (%d,%d,%d), VFX %s"),
        Runtime.GetReg(PosBase), Runtime.GetReg(PosBase + 1), Runtime.GetReg(PosBase + 2),
        *GetString(Runtime, StringIndex));
}

void FHktVMInterpreter::Op_PlayVFXAttached(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    if (!WorldState || !VMProxy) return;
    const FString& VFXName = GetString(Runtime, StringIndex);
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*VFXName), false);
    if (Tag.IsValid())
    {
        VMProxy->AddTag(*WorldState, Runtime.GetRegEntity(Entity), Tag);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] PlayVFXAttached: Entity %d, VFX %s"), Runtime.GetRegEntity(Entity), *VFXName);
}

// ============================================================================
// Audio
// ============================================================================

void FHktVMInterpreter::Op_PlaySound(FHktVMRuntime& Runtime, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlaySound: %s"), *GetString(Runtime, StringIndex));
}

void FHktVMInterpreter::Op_PlaySoundAtLocation(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlaySoundAtLocation: (%d,%d,%d), Sound %s"),
        Runtime.GetReg(PosBase), Runtime.GetReg(PosBase + 1), Runtime.GetReg(PosBase + 2),
        *GetString(Runtime, StringIndex));
}

// ============================================================================
// Equipment
// ============================================================================

void FHktVMInterpreter::Op_SpawnEquipment(FHktVMRuntime& Runtime, RegisterIndex Owner, int32 Slot, int32 StringIndex)
{
    FHktEntityId OwnerEntity = Runtime.GetRegEntity(Owner);
    const FString& EquipClass = GetString(Runtime, StringIndex);

    UE_LOG(LogTemp, Log, TEXT("[VM] SpawnEquipment: Owner %d, Slot %d, Class %s"), OwnerEntity, Slot, *EquipClass);

    if (WorldState && Runtime.Context)
    {
        FHktEntityId NewEquip = WorldState->AllocateEntity();
        Runtime.Context->WriteEntity(NewEquip, PropertyId::OwnerEntity, OwnerEntity);
        Runtime.SetRegEntity(Reg::Spawned, NewEquip);

        // EquipTag를 영구 태그로 부여
        FGameplayTag EquipTag = FGameplayTag::RequestGameplayTag(FName(*EquipClass), false);
        if (EquipTag.IsValid() && VMProxy)
        {
            VMProxy->AddTag(*WorldState, NewEquip, EquipTag);
        }
    }
}

// ============================================================================
// Tags
// ============================================================================

void FHktVMInterpreter::Op_AddTag(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    if (!WorldState || !VMProxy) return;
    const FString& TagName = GetString(Runtime, StringIndex);
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (Tag.IsValid())
        VMProxy->AddTag(*WorldState, Runtime.GetRegEntity(Entity), Tag);
}

void FHktVMInterpreter::Op_RemoveTag(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    if (!WorldState || !VMProxy) return;
    const FString& TagName = GetString(Runtime, StringIndex);
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (Tag.IsValid())
        VMProxy->RemoveTag(*WorldState, Runtime.GetRegEntity(Entity), Tag);
}

void FHktVMInterpreter::Op_HasTag(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity, int32 StringIndex)
{
    bool bHas = false;
    if (WorldState)
    {
        const FString& TagName = GetString(Runtime, StringIndex);
        FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        if (Tag.IsValid())
            bHas = WorldState->HasTag(Runtime.GetRegEntity(Entity), Tag);
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

    // 결정론적 해시: seed + frame + PC → 같은 상태에서 항상 같은 결과
    int32 Hash = static_cast<int32>(WorldState->FrameNumber * 2654435761) ^ (WorldState->RandomSeed + Runtime.PC);
    Hash = (Hash < 0) ? -Hash : Hash;
    Runtime.SetReg(Dst, Hash % Modulus);
}

void FHktVMInterpreter::Op_HasPlayerInGroup(FHktVMRuntime& Runtime, RegisterIndex Dst)
{
    // OwnerUid가 0이 아닌 캐릭터 엔티티가 있는지 확인
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

// ============================================================================
// Utility
// ============================================================================

void FHktVMInterpreter::Op_Log(FHktVMRuntime& Runtime, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM Log] %s"), *GetString(Runtime, StringIndex));
}
