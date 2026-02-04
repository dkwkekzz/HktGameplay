#include "HktVMInterpreter.h"
#include "HktVMProgram.h"
#include "HktVMStore.h"
#include "HktCoreInterfaces.h"

// Helper
const FString& FHktVMInterpreter::GetString(FHktVMRuntime& Runtime, int32 Index)
{
    static FString Empty;
    if (Runtime.Program && Index >= 0 && Index < Runtime.Program->Strings.Num())
        return Runtime.Program->Strings[Index];
    return Empty;
}

// Entity Management
void FHktVMInterpreter::Op_SpawnEntity(FHktVMRuntime& Runtime, int32 StringIndex)
{
    const FString& ClassPath = GetString(Runtime, StringIndex);
    UE_LOG(LogTemp, Log, TEXT("[VM] SpawnEntity: %s"), *ClassPath);
    
    if (Stash)
    {
        EntityId NewEntity = Stash->AllocateEntity();
        Runtime.SetRegEntity(Reg::Spawned, NewEntity);
        
        // 소유자 설정 (Store를 통해 버퍼링)
        if (Runtime.Store)
        {
            Runtime.Store->WriteEntity(NewEntity, PropertyId::OwnerEntity, Runtime.GetRegEntity(Reg::Self));
            Runtime.Store->WriteEntity(NewEntity, PropertyId::EntityType, HktEntityType::Projectile);
        }
    }
}

void FHktVMInterpreter::Op_DestroyEntity(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    EntityId E = Runtime.GetRegEntity(Entity);
    UE_LOG(LogTemp, Log, TEXT("[VM] DestroyEntity: %u"), E.RawValue);
    
    // 엔티티 제거는 즉시 적용 (다른 VM이 참조하지 못하게)
    if (Stash)
    {
        Stash->FreeEntity(E);
    }
}

// Position & Movement
void FHktVMInterpreter::Op_GetPosition(FHktVMRuntime& Runtime, RegisterIndex DstBase, RegisterIndex Entity)
{
    if (Runtime.Store)
    {
        EntityId E = Runtime.GetRegEntity(Entity);
        Runtime.SetReg(DstBase, Runtime.Store->ReadEntity(E, PropertyId::PosX));
        Runtime.SetReg(DstBase + 1, Runtime.Store->ReadEntity(E, PropertyId::PosY));
        Runtime.SetReg(DstBase + 2, Runtime.Store->ReadEntity(E, PropertyId::PosZ));
    }
}

void FHktVMInterpreter::Op_SetPosition(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex SrcBase)
{
    if (Runtime.Store)
    {
        EntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Store->WriteEntity(E, PropertyId::PosX, Runtime.GetReg(SrcBase));
        Runtime.Store->WriteEntity(E, PropertyId::PosY, Runtime.GetReg(SrcBase + 1));
        Runtime.Store->WriteEntity(E, PropertyId::PosZ, Runtime.GetReg(SrcBase + 2));
    }
}

void FHktVMInterpreter::Op_GetDistance(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2)
{
    if (Runtime.Store)
    {
        EntityId E1 = Runtime.GetRegEntity(Entity1);
        EntityId E2 = Runtime.GetRegEntity(Entity2);
        
        int32 X1 = Runtime.Store->ReadEntity(E1, PropertyId::PosX);
        int32 Y1 = Runtime.Store->ReadEntity(E1, PropertyId::PosY);
        int32 Z1 = Runtime.Store->ReadEntity(E1, PropertyId::PosZ);
        
        int32 X2 = Runtime.Store->ReadEntity(E2, PropertyId::PosX);
        int32 Y2 = Runtime.Store->ReadEntity(E2, PropertyId::PosY);
        int32 Z2 = Runtime.Store->ReadEntity(E2, PropertyId::PosZ);
        
        int64 DX = X2 - X1;
        int64 DY = Y2 - Y1;
        int64 DZ = Z2 - Z1;
        
        int32 DistSq = static_cast<int32>(FMath::Min((int64)INT32_MAX, DX*DX + DY*DY + DZ*DZ));
        Runtime.SetReg(Dst, FMath::Sqrt(static_cast<float>(DistSq)));
    }
}

void FHktVMInterpreter::Op_MoveToward(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex TargetBase, int32 Speed)
{
    if (Runtime.Store)
    {
        EntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Store->WriteEntity(E, PropertyId::MoveTargetX, Runtime.GetReg(TargetBase));
        Runtime.Store->WriteEntity(E, PropertyId::MoveTargetY, Runtime.GetReg(TargetBase + 1));
        Runtime.Store->WriteEntity(E, PropertyId::MoveTargetZ, Runtime.GetReg(TargetBase + 2));
        Runtime.Store->WriteEntity(E, PropertyId::MoveSpeed, Speed);
        Runtime.Store->WriteEntity(E, PropertyId::IsMoving, 1);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] MoveToward: Entity %u, Speed %d"), (int32)Runtime.GetRegEntity(Entity), Speed);
}

void FHktVMInterpreter::Op_MoveForward(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 Speed)
{
    if (Runtime.Store)
    {
        EntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Store->WriteEntity(E, PropertyId::MoveSpeed, Speed);
        Runtime.Store->WriteEntity(E, PropertyId::IsMoving, 1);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] MoveForward: Entity %u, Speed %d"), (int32)Runtime.GetRegEntity(Entity), Speed);
}

void FHktVMInterpreter::Op_StopMovement(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    if (Runtime.Store)
    {
        EntityId E = Runtime.GetRegEntity(Entity);
        Runtime.Store->WriteEntity(E, PropertyId::IsMoving, 0);
    }
    UE_LOG(LogTemp, Log, TEXT("[VM] StopMovement: Entity %u"), (int32)Runtime.GetRegEntity(Entity));
}

// Spatial Query
void FHktVMInterpreter::Op_FindInRadius(FHktVMRuntime& Runtime, RegisterIndex CenterEntity, int32 RadiusCm)
{
    Runtime.SpatialQuery.Reset();
    
    if (Stash && Runtime.Store)
    {
        EntityId Center = Runtime.GetRegEntity(CenterEntity);
        
        // 중심 위치는 Store에서 읽기 (현재 VM의 로컬 캐시 반영)
        int32 CX = Runtime.Store->ReadEntity(Center, PropertyId::PosX);
        int32 CY = Runtime.Store->ReadEntity(Center, PropertyId::PosY);
        int32 CZ = Runtime.Store->ReadEntity(Center, PropertyId::PosZ);
        int32 Team = Runtime.Store->ReadEntity(Center, PropertyId::Team);
        
        int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;
        
        // 다른 엔티티는 Stash에서 직접 읽기 (커밋된 상태)
        for (int32 E = 0; E < 1024; ++E)
        {
            if (!Stash->IsValidEntity(E) || E == Center)
                continue;
            
            if (Stash->GetProperty(E, PropertyId::Team) == Team)
                continue;
            
            int32 EX = Stash->GetProperty(E, PropertyId::PosX);
            int32 EY = Stash->GetProperty(E, PropertyId::PosY);
            int32 EZ = Stash->GetProperty(E, PropertyId::PosZ);
            
            int64 DX = EX - CX;
            int64 DY = EY - CY;
            int64 DZ = EZ - CZ;
            
            if (DX*DX + DY*DY + DZ*DZ <= RadiusSq)
                Runtime.SpatialQuery.Entities.Add(E);
        }
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

// Combat
void FHktVMInterpreter::Op_ApplyDamage(FHktVMRuntime& Runtime, RegisterIndex Target, RegisterIndex Amount)
{
    EntityId E = Runtime.GetRegEntity(Target);
    int32 Dmg = Runtime.GetReg(Amount);
    
    UE_LOG(LogTemp, Log, TEXT("[VM] ApplyDamage: Entity %u takes %d damage"), (int32)E, Dmg);
    
    if (Runtime.Store && Stash && Stash->IsValidEntity(E))
    {
        int32 Health = Runtime.Store->ReadEntity(E, PropertyId::Health);
        int32 Defense = Runtime.Store->ReadEntity(E, PropertyId::Defense);
        
        int32 ActualDmg = FMath::Max(1, Dmg - Defense);
        int32 NewHealth = FMath::Max(0, Health - ActualDmg);
        
        Runtime.Store->WriteEntity(E, PropertyId::Health, NewHealth);
    }
}

void FHktVMInterpreter::Op_ApplyEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex)
{
    EntityId E = Runtime.GetRegEntity(Target);
    const FString& Effect = GetString(Runtime, StringIndex);
    UE_LOG(LogTemp, Log, TEXT("[VM] ApplyEffect: Entity %u, Effect %s"), (int32)E, *Effect);
}

void FHktVMInterpreter::Op_RemoveEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex)
{
    EntityId E = Runtime.GetRegEntity(Target);
    const FString& Effect = GetString(Runtime, StringIndex);
    UE_LOG(LogTemp, Log, TEXT("[VM] RemoveEffect: Entity %u, Effect %s"), (int32)E, *Effect);
}

// Animation & VFX
void FHktVMInterpreter::Op_PlayAnim(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlayAnim: Entity %u, Anim %s"), 
        (int32)Runtime.GetRegEntity(Entity), *GetString(Runtime, StringIndex));
}

void FHktVMInterpreter::Op_PlayAnimMontage(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlayAnimMontage: Entity %u, Montage %s"), 
        (int32)Runtime.GetRegEntity(Entity), *GetString(Runtime, StringIndex));
}

void FHktVMInterpreter::Op_StopAnim(FHktVMRuntime& Runtime, RegisterIndex Entity)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] StopAnim: Entity %u"), (int32)Runtime.GetRegEntity(Entity));
}

void FHktVMInterpreter::Op_PlayVFX(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlayVFX: (%d,%d,%d), VFX %s"), 
        Runtime.GetReg(PosBase), Runtime.GetReg(PosBase+1), Runtime.GetReg(PosBase+2),
        *GetString(Runtime, StringIndex));
}

void FHktVMInterpreter::Op_PlayVFXAttached(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlayVFXAttached: Entity %u, VFX %s"), 
        (int32)Runtime.GetRegEntity(Entity), *GetString(Runtime, StringIndex));
}

// Audio
void FHktVMInterpreter::Op_PlaySound(FHktVMRuntime& Runtime, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlaySound: %s"), *GetString(Runtime, StringIndex));
}

void FHktVMInterpreter::Op_PlaySoundAtLocation(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM] PlaySoundAtLocation: (%d,%d,%d), Sound %s"), 
        Runtime.GetReg(PosBase), Runtime.GetReg(PosBase+1), Runtime.GetReg(PosBase+2),
        *GetString(Runtime, StringIndex));
}

// Equipment
void FHktVMInterpreter::Op_SpawnEquipment(FHktVMRuntime& Runtime, RegisterIndex Owner, int32 Slot, int32 StringIndex)
{
    EntityId OwnerEntity = Runtime.GetRegEntity(Owner);
    const FString& EquipClass = GetString(Runtime, StringIndex);
    
    UE_LOG(LogTemp, Log, TEXT("[VM] SpawnEquipment: Owner %u, Slot %d, Class %s"), (int32)OwnerEntity, Slot, *EquipClass);
    
    if (Stash && Runtime.Store)
    {
        EntityId NewEquip = Stash->AllocateEntity();
        Runtime.Store->WriteEntity(NewEquip, PropertyId::EntityType, HktEntityType::Equipment);
        Runtime.Store->WriteEntity(NewEquip, PropertyId::OwnerEntity, OwnerEntity);
        Runtime.SetRegEntity(Reg::Spawned, NewEquip);
    }
}

// Utility
void FHktVMInterpreter::Op_Log(FHktVMRuntime& Runtime, int32 StringIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[VM Log] %s"), *GetString(Runtime, StringIndex));
}