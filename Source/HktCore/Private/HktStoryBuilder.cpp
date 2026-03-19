// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "VM/HktVMProgram.h"
#include "GameplayTagsManager.h"

// ============================================================================
// FHktStoryBuilder - Construction
// ============================================================================

FHktStoryBuilder FHktStoryBuilder::Create(const FGameplayTag& Tag)
{
    return FHktStoryBuilder(Tag);
}

FHktStoryBuilder FHktStoryBuilder::Create(const FName& TagName)
{
    return FHktStoryBuilder(FGameplayTag::RequestGameplayTag(TagName));
}

FHktStoryBuilder::FHktStoryBuilder(const FGameplayTag& Tag)
    : Program(MakeShared<FHktVMProgram>())
{
    Program->Tag = Tag;
}

void FHktStoryBuilder::Emit(FInstruction Inst)
{
    Program->Code.Add(Inst);
}

int32 FHktStoryBuilder::AddString(const FString& Str)
{
    int32 Index = Program->Strings.IndexOfByKey(Str);
    if (Index == INDEX_NONE)
    {
        Index = Program->Strings.Num();
        Program->Strings.Add(Str);
    }
    return Index;
}

int32 FHktStoryBuilder::AddConstant(int32 Value)
{
    int32 Index = Program->Constants.IndexOfByKey(Value);
    if (Index == INDEX_NONE)
    {
        Index = Program->Constants.Num();
        Program->Constants.Add(Value);
    }
    return Index;
}

int32 FHktStoryBuilder::TagToInt(const FGameplayTag& Tag)
{
    if (Tag.IsValid())
    {
        FGameplayTagNetIndex NetIndex = UGameplayTagsManager::Get().GetNetIndexFromTag(Tag);
        return static_cast<int32>(NetIndex);
    }
    return 0;
}

FString FHktStoryBuilder::MakeInternalLabel(const TCHAR* Prefix)
{
    return FString::Printf(TEXT("__%s_%d"), Prefix, InternalLabelCounter++);
}

// ============================================================================
// Flow Policy
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::CancelOnDuplicate()
{
    Program->bCancelOnDuplicate = true;
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SetPrecondition(FHktEventPrecondition InPrecondition)
{
    Program->Precondition = MoveTemp(InPrecondition);
    return *this;
}

// ============================================================================
// Control Flow
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::Label(const FString& Name)
{
    Labels.Add(Name, Program->Code.Num());
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Jump(const FString& LabelName)
{
    Fixups.Add({Program->Code.Num(), LabelName});
    Emit(FInstruction::MakeImm(EOpCode::Jump, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::JumpIf(RegisterIndex Cond, const FString& LabelName)
{
    Fixups.Add({Program->Code.Num(), LabelName});
    Emit(FInstruction::Make(EOpCode::JumpIf, 0, Cond, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::JumpIfNot(RegisterIndex Cond, const FString& LabelName)
{
    Fixups.Add({Program->Code.Num(), LabelName});
    Emit(FInstruction::Make(EOpCode::JumpIfNot, 0, Cond, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Yield(int32 Frames)
{
    Emit(FInstruction::Make(EOpCode::Yield, 0, 0, 0, FMath::Max(1, Frames)));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::WaitSeconds(float Seconds)
{
    int32 DeciMillis = FMath::RoundToInt(Seconds * 100.0f);
    Emit(FInstruction::MakeImm(EOpCode::YieldSeconds, 0, DeciMillis));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Halt()
{
    Emit(FInstruction::Make(EOpCode::Halt));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Fail()
{
    Emit(FInstruction::Make(EOpCode::Fail));
    return *this;
}

// ============================================================================
// Event Wait
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::WaitCollision(RegisterIndex WatchEntity)
{
    Emit(FInstruction::Make(EOpCode::WaitCollision, Reg::Hit, WatchEntity, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::WaitAnimEnd(RegisterIndex Entity)
{
    Yield(1);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::WaitMoveEnd(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::WaitMoveEnd, 0, Entity, 0, 0));
    return *this;
}

// ============================================================================
// Data Operations (근본 opcode 래퍼)
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::LoadConst(RegisterIndex Dst, int32 Value)
{
    if (Value >= -524288 && Value <= 524287)
    {
        Emit(FInstruction::MakeImm(EOpCode::LoadConst, Dst, Value));
    }
    else
    {
        Emit(FInstruction::MakeImm(EOpCode::LoadConst, Dst, Value & 0xFFFFF));
        Emit(FInstruction::Make(EOpCode::LoadConstHigh, Dst, 0, 0, (Value >> 20) & 0xFFF));
    }
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::LoadStore(RegisterIndex Dst, uint16 PropertyId)
{
    Emit(FInstruction::Make(EOpCode::LoadStore, Dst, 0, 0, PropertyId));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::LoadStoreEntity(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId)
{
    Emit(FInstruction::Make(EOpCode::LoadStoreEntity, Dst, Entity, 0, PropertyId));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SaveStore(uint16 PropertyId, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::SaveStore, 0, Src, 0, PropertyId));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SaveStoreEntity(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::SaveStoreEntity, 0, Entity, Src, PropertyId));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SaveConst(uint16 PropertyId, int32 Value)
{
    LoadConst(Reg::Temp, Value);
    SaveStore(PropertyId, Reg::Temp);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SaveConstEntity(RegisterIndex Entity, uint16 PropertyId, int32 Value)
{
    LoadConst(Reg::Temp, Value);
    SaveStoreEntity(Entity, PropertyId, Reg::Temp);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Move(RegisterIndex Dst, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::Move, Dst, Src, 0, 0));
    return *this;
}

// ============================================================================
// Arithmetic
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::Add(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Add, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Sub(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Sub, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Mul(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Mul, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Div(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Div, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::AddImm(RegisterIndex Dst, RegisterIndex Src, int32 Imm)
{
    Emit(FInstruction::Make(EOpCode::AddImm, Dst, Src, 0, Imm & 0xFFF));
    return *this;
}

// ============================================================================
// Comparison
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::CmpEq(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpEq, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::CmpNe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpNe, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::CmpLt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpLt, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::CmpLe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpLe, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::CmpGt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpGt, Dst, Src1, Src2, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::CmpGe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpGe, Dst, Src1, Src2, 0));
    return *this;
}

// ============================================================================
// Entity Management
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::SpawnEntity(const FGameplayTag& ClassTag)
{
    int32 StrIdx = AddString(ClassTag.ToString());
    Emit(FInstruction::Make(EOpCode::SpawnEntity, 0, 0, 0, StrIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::DestroyEntity(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::DestroyEntity, 0, Entity, 0, 0));
    return *this;
}

// ============================================================================
// Position & Movement (조합 연산 — 기본 opcode로 분해)
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::GetPosition(RegisterIndex DstBase, RegisterIndex Entity)
{
    LoadStoreEntity(DstBase,     Entity, PropertyId::PosX);
    LoadStoreEntity(DstBase + 1, Entity, PropertyId::PosY);
    LoadStoreEntity(DstBase + 2, Entity, PropertyId::PosZ);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SetPosition(RegisterIndex Entity, RegisterIndex SrcBase)
{
    SaveStoreEntity(Entity, PropertyId::PosX, SrcBase);
    SaveStoreEntity(Entity, PropertyId::PosY, SrcBase + 1);
    SaveStoreEntity(Entity, PropertyId::PosZ, SrcBase + 2);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Force)
{
    SaveStoreEntity(Entity, PropertyId::MoveTargetX, TargetPosBase);
    SaveStoreEntity(Entity, PropertyId::MoveTargetY, TargetPosBase + 1);
    SaveStoreEntity(Entity, PropertyId::MoveTargetZ, TargetPosBase + 2);
    SaveConstEntity(Entity, PropertyId::MoveForce, Force);
    SaveConstEntity(Entity, PropertyId::IsMoving, 1);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::MoveForward(RegisterIndex Entity, int32 Force)
{
    SaveConstEntity(Entity, PropertyId::MoveForce, Force);
    SaveConstEntity(Entity, PropertyId::IsMoving, 1);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::StopMovement(RegisterIndex Entity)
{
    SaveConstEntity(Entity, PropertyId::IsMoving, 0);
    SaveConstEntity(Entity, PropertyId::VelX, 0);
    SaveConstEntity(Entity, PropertyId::VelY, 0);
    SaveConstEntity(Entity, PropertyId::VelZ, 0);
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2)
{
    Emit(FInstruction::Make(EOpCode::GetDistance, Dst, Entity1, Entity2, 0));
    return *this;
}

// ============================================================================
// Spatial Query
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm)
{
    Emit(FInstruction::Make(EOpCode::FindInRadius, Reg::Count, CenterEntity, 0, RadiusCm & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::NextFound()
{
    Emit(FInstruction::Make(EOpCode::NextFound, Reg::Iter, 0, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm)
{
    FForEachContext Ctx;
    Ctx.LoopLabel = FString::Printf(TEXT("__foreach_%d_loop"), ForEachCounter);
    Ctx.EndLabel = FString::Printf(TEXT("__foreach_%d_end"), ForEachCounter);
    ForEachCounter++;
    ForEachStack.Push(Ctx);

    FindInRadius(CenterEntity, RadiusCm);
    Label(Ctx.LoopLabel);
    NextFound();
    JumpIfNot(Reg::Flag, Ctx.EndLabel);

    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::EndForEach()
{
    check(ForEachStack.Num() > 0);
    FForEachContext Ctx = ForEachStack.Pop();

    Jump(Ctx.LoopLabel);
    Label(Ctx.EndLabel);

    return *this;
}

// ============================================================================
// Combat (조합 연산 — R7,R8,R9 사용)
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::ApplyDamage(RegisterIndex Target, RegisterIndex Amount)
{
    // ActualDmg = Max(1, Amount - Defense)
    // NewHealth = Max(0, Health - ActualDmg)
    // 사용 레지스터: R7(scratch), R8(scratch), R9(Temp), Flag
    // 주의: Amount가 R7/R8/R9일 경우를 위해 첫 번째 연산에서 즉시 소비

    Move(Reg::R7, Amount);                                    // R7 = Amount (즉시 복사하여 안전)
    LoadStoreEntity(Reg::R8, Target, PropertyId::Defense);    // R8 = Defense
    Sub(Reg::R7, Reg::R7, Reg::R8);                           // R7 = Dmg - Defense

    // Clamp to min 1
    LoadConst(Reg::R8, 1);
    CmpLt(Reg::Flag, Reg::R7, Reg::R8);                     // Flag = (R7 < 1)
    FString skipClamp1 = MakeInternalLabel(TEXT("dmg"));
    JumpIfNot(Reg::Flag, skipClamp1);
    Move(Reg::R7, Reg::R8);                                  // R7 = 1
    Label(skipClamp1);

    // NewHealth = Health - ActualDmg
    LoadStoreEntity(Reg::R8, Target, PropertyId::Health);    // R8 = Health
    Sub(Reg::R8, Reg::R8, Reg::R7);                          // R8 = Health - ActualDmg

    // Clamp to min 0
    LoadConst(Reg::Temp, 0);
    CmpLt(Reg::Flag, Reg::R8, Reg::Temp);                   // Flag = (R8 < 0)
    FString skipClamp2 = MakeInternalLabel(TEXT("dmg"));
    JumpIfNot(Reg::Flag, skipClamp2);
    Move(Reg::R8, Reg::Temp);                                // R8 = 0
    Label(skipClamp2);

    SaveStoreEntity(Target, PropertyId::Health, Reg::R8);    // Health = NewHealth
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::ApplyDamageConst(RegisterIndex Target, int32 Amount)
{
    LoadConst(Reg::Temp, Amount);
    ApplyDamage(Target, Reg::Temp);
    return *this;
}

// ============================================================================
// Presentation
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::ApplyEffect(RegisterIndex Target, const FGameplayTag& EffectTag)
{
    int32 TagIdx = TagToInt(EffectTag);
    Emit(FInstruction::Make(EOpCode::ApplyEffect, 0, Target, 0, TagIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::RemoveEffect(RegisterIndex Target, const FGameplayTag& EffectTag)
{
    int32 TagIdx = TagToInt(EffectTag);
    Emit(FInstruction::Make(EOpCode::RemoveEffect, 0, Target, 0, TagIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::PlayVFX(RegisterIndex PosBase, const FGameplayTag& VFXTag)
{
    int32 TagIdx = TagToInt(VFXTag);
    Emit(FInstruction::Make(EOpCode::PlayVFX, 0, PosBase, 0, TagIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::PlayVFXAttached(RegisterIndex Entity, const FGameplayTag& VFXTag)
{
    int32 TagIdx = TagToInt(VFXTag);
    Emit(FInstruction::Make(EOpCode::PlayVFXAttached, 0, Entity, 0, TagIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::PlaySound(const FGameplayTag& SoundTag)
{
    int32 TagIdx = TagToInt(SoundTag);
    Emit(FInstruction::MakeImm(EOpCode::PlaySound, 0, TagIdx));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::PlaySoundAtLocation(RegisterIndex PosBase, const FGameplayTag& SoundTag)
{
    int32 TagIdx = TagToInt(SoundTag);
    Emit(FInstruction::Make(EOpCode::PlaySoundAtLocation, 0, PosBase, 0, TagIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Tags
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::AddTag(RegisterIndex Entity, const FGameplayTag& Tag)
{
    int32 StrIdx = AddString(Tag.ToString());
    Emit(FInstruction::Make(EOpCode::AddTag, 0, Entity, 0, StrIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::RemoveTag(RegisterIndex Entity, const FGameplayTag& Tag)
{
    int32 StrIdx = AddString(Tag.ToString());
    Emit(FInstruction::Make(EOpCode::RemoveTag, 0, Entity, 0, StrIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::HasTag(RegisterIndex Dst, RegisterIndex Entity, const FGameplayTag& Tag)
{
    int32 StrIdx = AddString(Tag.ToString());
    Emit(FInstruction::Make(EOpCode::HasTag, Dst, Entity, 0, StrIdx & 0xFFF));
    return *this;
}

// ============================================================================
// NPC Spawning
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::CountByTag(RegisterIndex Dst, const FGameplayTag& Tag)
{
    int32 StrIdx = AddString(Tag.ToString());
    Emit(FInstruction::Make(EOpCode::CountByTag, Dst, 0, 0, StrIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::GetWorldTime(RegisterIndex Dst)
{
    Emit(FInstruction::Make(EOpCode::GetWorldTime, Dst, 0, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::RandomInt(RegisterIndex Dst, RegisterIndex ModulusReg)
{
    Emit(FInstruction::Make(EOpCode::RandomInt, Dst, ModulusReg, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::HasPlayerInGroup(RegisterIndex Dst)
{
    Emit(FInstruction::Make(EOpCode::HasPlayerInGroup, Dst, 0, 0, 0));
    return *this;
}

// ============================================================================
// Item System
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::CountByOwner(RegisterIndex Dst, RegisterIndex OwnerEntity, const FGameplayTag& Tag)
{
    int32 StrIdx = AddString(Tag.ToString());
    Emit(FInstruction::Make(EOpCode::CountByOwner, Dst, OwnerEntity, 0, StrIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::FindByOwner(RegisterIndex OwnerEntity, const FGameplayTag& Tag)
{
    int32 StrIdx = AddString(Tag.ToString());
    Emit(FInstruction::Make(EOpCode::FindByOwner, Reg::Count, OwnerEntity, 0, StrIdx & 0xFFF));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::SetOwnerUid(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::SetOwnerUid, 0, Entity));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::ClearOwnerUid(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::ClearOwnerUid, 0, Entity));
    return *this;
}

// ============================================================================
// Stance
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::SetStance(RegisterIndex Entity, const FGameplayTag& StanceTag)
{
    int32 TagIdx = TagToInt(StanceTag);
    SaveConstEntity(Entity, PropertyId::Stance, TagIdx);
    return *this;
}

// ============================================================================
// Utility
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::Log(const FString& Message)
{
    int32 StrIdx = AddString(Message);
    Emit(FInstruction::MakeImm(EOpCode::Log, 0, StrIdx));
    return *this;
}

// ============================================================================
// Build
// ============================================================================

void FHktStoryBuilder::ResolveLabels()
{
    for (const auto& Fixup : Fixups)
    {
        int32 CodeIndex = Fixup.Key;
        const FString& LabelName = Fixup.Value;

        if (const int32* Target = Labels.Find(LabelName))
        {
            FInstruction& Inst = Program->Code[CodeIndex];

            switch (Inst.GetOpCode())
            {
            case EOpCode::Jump:
                Inst.Imm20 = *Target;
                break;
            case EOpCode::JumpIf:
            case EOpCode::JumpIfNot:
                Inst.Imm12 = static_cast<uint16>(*Target);
                break;
            default:
                break;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Unresolved label: %s in Flow %s"), *LabelName, *Program->Tag.ToString());
        }
    }
}

TSharedRef<FHktVMProgram> FHktStoryBuilder::Build()
{
    if (Program->Code.Num() == 0 || Program->Code.Last().GetOpCode() != EOpCode::Halt)
    {
        Halt();
    }

    ResolveLabels();
    return Program;
}

void FHktStoryBuilder::BuildAndRegister()
{
    FHktVMProgramRegistry::Get().RegisterProgram(Build());
}

// ============================================================================
// Public Query API
// ============================================================================

bool HktStory::ValidateEvent(const FHktWorldState& WorldState, const FHktEvent& Event)
{
    return FHktVMProgramRegistry::Get().ValidateEvent(WorldState, Event);
}
