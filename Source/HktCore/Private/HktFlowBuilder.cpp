// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFlowBuilder.h"
#include "VM/HktVMProgram.h"
#include "GameplayTagsManager.h"

// ============================================================================
// FHktFlowBuilder - Construction
// ============================================================================

FHktFlowBuilder FHktFlowBuilder::Create(const FGameplayTag& Tag)
{
    return FHktFlowBuilder(Tag);
}

FHktFlowBuilder FHktFlowBuilder::Create(const FName& TagName)
{
    return FHktFlowBuilder(FGameplayTag::RequestGameplayTag(TagName));
}

FHktFlowBuilder::FHktFlowBuilder(const FGameplayTag& Tag)
    : Program(MakeShared<FHktVMProgram>())
{
    Program->Tag = Tag;
}

void FHktFlowBuilder::Emit(FInstruction Inst)
{
    Program->Code.Add(Inst);
}

int32 FHktFlowBuilder::AddString(const FString& Str)
{
    int32 Index = Program->Strings.IndexOfByKey(Str);
    if (Index == INDEX_NONE)
    {
        Index = Program->Strings.Num();
        Program->Strings.Add(Str);
    }
    return Index;
}

int32 FHktFlowBuilder::AddConstant(int32 Value)
{
    int32 Index = Program->Constants.IndexOfByKey(Value);
    if (Index == INDEX_NONE)
    {
        Index = Program->Constants.Num();
        Program->Constants.Add(Value);
    }
    return Index;
}

int32 FHktFlowBuilder::TagToInt(const FGameplayTag& Tag)
{
    if (Tag.IsValid())
    {
        FGameplayTagNetIndex NetIndex = UGameplayTagsManager::Get().GetNetIndexFromTag(Tag);
        return static_cast<int32>(NetIndex);
    }
    return 0;
}

// ============================================================================
// Control Flow
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::Label(const FString& Name)
{
    Labels.Add(Name, Program->Code.Num());
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Jump(const FString& LabelName)
{
    Fixups.Add({Program->Code.Num(), LabelName});
    Emit(FInstruction::MakeImm(EOpCode::Jump, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::JumpIf(RegisterIndex Cond, const FString& LabelName)
{
    Fixups.Add({Program->Code.Num(), LabelName});
    Emit(FInstruction::Make(EOpCode::JumpIf, 0, Cond, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::JumpIfNot(RegisterIndex Cond, const FString& LabelName)
{
    Fixups.Add({Program->Code.Num(), LabelName});
    Emit(FInstruction::Make(EOpCode::JumpIfNot, 0, Cond, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Yield(int32 Frames)
{
    Emit(FInstruction::Make(EOpCode::Yield, 0, 0, 0, FMath::Max(1, Frames)));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::WaitSeconds(float Seconds)
{
    int32 DeciMillis = FMath::RoundToInt(Seconds * 100.0f);
    Emit(FInstruction::MakeImm(EOpCode::YieldSeconds, 0, DeciMillis));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Halt()
{
    Emit(FInstruction::Make(EOpCode::Halt));
    return *this;
}

// ============================================================================
// Event Wait
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::WaitCollision(RegisterIndex WatchEntity)
{
    Emit(FInstruction::Make(EOpCode::WaitCollision, Reg::Hit, WatchEntity, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::WaitAnimEnd(RegisterIndex Entity)
{
    // 구현: 프레임 대기로 대체 (향후 AnimEnd 이벤트 도입 가능)
    Yield(1);
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::WaitMoveEnd(RegisterIndex Entity)
{
    // 구현: 프레임 대기로 대체 (향후 MoveEnd 이벤트 도입 가능)
    Yield(1);
    return *this;
}

// ============================================================================
// Data Operations
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::LoadConst(RegisterIndex Dst, int32 Value)
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

FHktFlowBuilder& FHktFlowBuilder::LoadStore(RegisterIndex Dst, uint16 PropertyId)
{
    Emit(FInstruction::Make(EOpCode::LoadStore, Dst, 0, 0, PropertyId));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::LoadEntityProperty(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId)
{
    Emit(FInstruction::Make(EOpCode::LoadStoreEntity, Dst, Entity, 0, PropertyId));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::SaveStore(uint16 PropertyId, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::SaveStore, 0, Src, 0, PropertyId));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::SaveEntityProperty(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::SaveStoreEntity, 0, Entity, Src, PropertyId));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Move(RegisterIndex Dst, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::Move, Dst, Src, 0, 0));
    return *this;
}

// ============================================================================
// Arithmetic
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::Add(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Add, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Sub(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Sub, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Mul(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Mul, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::Div(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Div, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::AddImm(RegisterIndex Dst, RegisterIndex Src, int32 Imm)
{
    Emit(FInstruction::Make(EOpCode::AddImm, Dst, Src, 0, Imm & 0xFFF));
    return *this;
}

// ============================================================================
// Comparison
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::CmpEq(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpEq, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::CmpNe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpNe, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::CmpLt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpLt, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::CmpLe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpLe, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::CmpGt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpGt, Dst, Src1, Src2, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::CmpGe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpGe, Dst, Src1, Src2, 0));
    return *this;
}

// ============================================================================
// Entity Management
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::SpawnEntity(const FGameplayTag& ClassTag)
{
    int32 TagIdx = TagToInt(ClassTag);
    Emit(FInstruction::MakeImm(EOpCode::SpawnEntity, Reg::Spawned, TagIdx));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::DestroyEntity(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::DestroyEntity, 0, Entity, 0, 0));
    return *this;
}

// ============================================================================
// Position & Movement
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::GetPosition(RegisterIndex DstBase, RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::GetPosition, DstBase, Entity, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::SetPosition(RegisterIndex Entity, RegisterIndex SrcBase)
{
    Emit(FInstruction::Make(EOpCode::SetPosition, Entity, SrcBase, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Speed)
{
    Emit(FInstruction::Make(EOpCode::MoveToward, Entity, TargetPosBase, 0, Speed & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::MoveForward(RegisterIndex Entity, int32 Speed)
{
    Emit(FInstruction::Make(EOpCode::MoveForward, 0, Entity, 0, Speed & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::StopMovement(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::StopMovement, 0, Entity, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2)
{
    Emit(FInstruction::Make(EOpCode::GetDistance, Dst, Entity1, Entity2, 0));
    return *this;
}

// ============================================================================
// Spatial Query
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm)
{
    Emit(FInstruction::Make(EOpCode::FindInRadius, Reg::Count, CenterEntity, 0, RadiusCm & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::NextFound()
{
    Emit(FInstruction::Make(EOpCode::NextFound, Reg::Iter, 0, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm)
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

FHktFlowBuilder& FHktFlowBuilder::EndForEach()
{
    check(ForEachStack.Num() > 0);
    FForEachContext Ctx = ForEachStack.Pop();

    Jump(Ctx.LoopLabel);
    Label(Ctx.EndLabel);

    return *this;
}

// ============================================================================
// Combat
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::ApplyDamage(RegisterIndex Target, RegisterIndex Amount)
{
    Emit(FInstruction::Make(EOpCode::ApplyDamage, 0, Target, Amount, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::ApplyDamageConst(RegisterIndex Target, int32 Amount)
{
    LoadConst(Reg::Temp, Amount);
    ApplyDamage(Target, Reg::Temp);
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::ApplyEffect(RegisterIndex Target, const FGameplayTag& EffectTag)
{
    int32 TagIdx = TagToInt(EffectTag);
    Emit(FInstruction::Make(EOpCode::ApplyEffect, 0, Target, 0, TagIdx & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::RemoveEffect(RegisterIndex Target, const FGameplayTag& EffectTag)
{
    int32 TagIdx = TagToInt(EffectTag);
    Emit(FInstruction::Make(EOpCode::RemoveEffect, 0, Target, 0, TagIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Animation & VFX
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::PlayAnim(RegisterIndex Entity, const FGameplayTag& AnimTag)
{
    int32 TagIdx = TagToInt(AnimTag);
    Emit(FInstruction::Make(EOpCode::PlayAnim, 0, Entity, 0, TagIdx & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::PlayAnimMontage(RegisterIndex Entity, const FGameplayTag& MontageTag)
{
    int32 TagIdx = TagToInt(MontageTag);
    Emit(FInstruction::Make(EOpCode::PlayAnimMontage, 0, Entity, 0, TagIdx & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::StopAnim(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::StopAnim, 0, Entity, 0, 0));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::PlayVFX(RegisterIndex PosBase, const FGameplayTag& VFXTag)
{
    int32 TagIdx = TagToInt(VFXTag);
    Emit(FInstruction::Make(EOpCode::PlayVFX, 0, PosBase, 0, TagIdx & 0xFFF));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::PlayVFXAttached(RegisterIndex Entity, const FGameplayTag& VFXTag)
{
    int32 TagIdx = TagToInt(VFXTag);
    Emit(FInstruction::Make(EOpCode::PlayVFXAttached, 0, Entity, 0, TagIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Audio
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::PlaySound(const FGameplayTag& SoundTag)
{
    int32 TagIdx = TagToInt(SoundTag);
    Emit(FInstruction::MakeImm(EOpCode::PlaySound, 0, TagIdx));
    return *this;
}

FHktFlowBuilder& FHktFlowBuilder::PlaySoundAtLocation(RegisterIndex PosBase, const FGameplayTag& SoundTag)
{
    int32 TagIdx = TagToInt(SoundTag);
    Emit(FInstruction::Make(EOpCode::PlaySoundAtLocation, 0, PosBase, 0, TagIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Equipment
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::SpawnEquipment(RegisterIndex Owner, int32 Slot, const FGameplayTag& EquipTag)
{
    int32 TagIdx = TagToInt(EquipTag);
    Emit(FInstruction::Make(EOpCode::SpawnEquipment, Reg::Spawned, Owner, Slot & 0xF, TagIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Utility
// ============================================================================

FHktFlowBuilder& FHktFlowBuilder::Log(const FString& Message)
{
    int32 StrIdx = AddString(Message);
    Emit(FInstruction::MakeImm(EOpCode::Log, 0, StrIdx));
    return *this;
}

// ============================================================================
// Build
// ============================================================================

void FHktFlowBuilder::ResolveLabels()
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

TSharedRef<FHktVMProgram> FHktFlowBuilder::Build()
{
    if (Program->Code.Num() == 0 || Program->Code.Last().GetOpCode() != EOpCode::Halt)
    {
        Halt();
    }

    ResolveLabels();
    return Program;
}

void FHktFlowBuilder::BuildAndRegister()
{
    FHktVMProgramRegistry::Get().RegisterProgram(Build());
}
