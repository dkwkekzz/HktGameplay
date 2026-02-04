#include "HktVMProgram.h"

// ============================================================================
// FHktVMProgramRegistry
// ============================================================================

FHktVMProgramRegistry& FHktVMProgramRegistry::Get()
{
    static FHktVMProgramRegistry Instance;
    return Instance;
}

const FHktVMProgram* FHktVMProgramRegistry::FindProgram(const FGameplayTag& Tag) const
{
    FRWScopeLock ReadLock(Lock, SLT_ReadOnly);
    if (const TSharedPtr<FHktVMProgram>* Found = Programs.Find(Tag))
    {
        return Found->Get();
    }
    return nullptr;
}

void FHktVMProgramRegistry::RegisterProgram(FHktVMProgram&& Program)
{
    FRWScopeLock WriteLock(Lock, SLT_Write);
    FGameplayTag Tag = Program.Tag;
    Programs.Add(Tag, MakeShared<FHktVMProgram>(MoveTemp(Program)));
}

void FHktVMProgramRegistry::Clear()
{
    FRWScopeLock WriteLock(Lock, SLT_Write);
    Programs.Empty();
}

// ============================================================================
// FFlowBuilder - Construction
// ============================================================================

FFlowBuilder FFlowBuilder::Create(const FGameplayTag& Tag)
{
    return FFlowBuilder(Tag);
}

FFlowBuilder FFlowBuilder::Create(const FName& TagName)
{
    return FFlowBuilder(FGameplayTag::RequestGameplayTag(TagName));
}

FFlowBuilder::FFlowBuilder(const FGameplayTag& Tag)
{
    Program.Tag = Tag;
    
    // Self 레지스터 초기화 명령 자동 추가는 VM 생성 시 처리
}

void FFlowBuilder::Emit(FInstruction Inst)
{
    Program.Code.Add(Inst);
}

int32 FFlowBuilder::AddString(const FString& Str)
{
    int32 Index = Program.Strings.IndexOfByKey(Str);
    if (Index == INDEX_NONE)
    {
        Index = Program.Strings.Num();
        Program.Strings.Add(Str);
    }
    return Index;
}

int32 FFlowBuilder::AddConstant(int32 Value)
{
    int32 Index = Program.Constants.IndexOfByKey(Value);
    if (Index == INDEX_NONE)
    {
        Index = Program.Constants.Num();
        Program.Constants.Add(Value);
    }
    return Index;
}

// ============================================================================
// Control Flow
// ============================================================================

FFlowBuilder& FFlowBuilder::Label(const FString& Name)
{
    Labels.Add(Name, Program.Code.Num());
    return *this;
}

FFlowBuilder& FFlowBuilder::Jump(const FString& Label)
{
    Fixups.Add({Program.Code.Num(), Label});
    Emit(FInstruction::MakeImm(EOpCode::Jump, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::JumpIf(RegisterIndex Cond, const FString& Label)
{
    Fixups.Add({Program.Code.Num(), Label});
    Emit(FInstruction::Make(EOpCode::JumpIf, 0, Cond, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::JumpIfNot(RegisterIndex Cond, const FString& Label)
{
    Fixups.Add({Program.Code.Num(), Label});
    Emit(FInstruction::Make(EOpCode::JumpIfNot, 0, Cond, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::Yield(int32 Frames)
{
    Emit(FInstruction::Make(EOpCode::Yield, 0, 0, 0, FMath::Max(1, Frames)));
    return *this;
}

FFlowBuilder& FFlowBuilder::WaitSeconds(float Seconds)
{
    // 밀리초/10 단위로 저장 (최대 ~10초)
    int32 DeciMillis = FMath::RoundToInt(Seconds * 100.0f);
    Emit(FInstruction::MakeImm(EOpCode::YieldSeconds, 0, DeciMillis));
    return *this;
}

FFlowBuilder& FFlowBuilder::Halt()
{
    Emit(FInstruction::Make(EOpCode::Halt));
    return *this;
}

// ============================================================================
// Event Wait
// ============================================================================

FFlowBuilder& FFlowBuilder::WaitCollision(RegisterIndex WatchEntity)
{
    Emit(FInstruction::Make(EOpCode::WaitCollision, Reg::Hit, WatchEntity, 0, 0));
    return *this;
}

// ============================================================================
// Data Operations
// ============================================================================

FFlowBuilder& FFlowBuilder::LoadConst(RegisterIndex Dst, int32 Value)
{
    // 20비트로 표현 가능한 범위인지 확인
    if (Value >= -524288 && Value <= 524287)
    {
        Emit(FInstruction::MakeImm(EOpCode::LoadConst, Dst, Value));
    }
    else
    {
        // 32비트 상수는 두 명령어로 분할
        Emit(FInstruction::MakeImm(EOpCode::LoadConst, Dst, Value & 0xFFFFF));
        Emit(FInstruction::Make(EOpCode::LoadConstHigh, Dst, 0, 0, (Value >> 20) & 0xFFF));
    }
    return *this;
}

FFlowBuilder& FFlowBuilder::LoadStore(RegisterIndex Dst, uint16 PropertyId)
{
    Emit(FInstruction::Make(EOpCode::LoadStore, Dst, 0, 0, PropertyId));
    return *this;
}

FFlowBuilder& FFlowBuilder::LoadEntityProperty(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId)
{
    Emit(FInstruction::Make(EOpCode::LoadStoreEntity, Dst, Entity, 0, PropertyId));
    return *this;
}

FFlowBuilder& FFlowBuilder::SaveStore(uint16 PropertyId, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::SaveStore, 0, Src, 0, PropertyId));
    return *this;
}

FFlowBuilder& FFlowBuilder::SaveEntityProperty(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::SaveStoreEntity, 0, Entity, Src, PropertyId));
    return *this;
}

FFlowBuilder& FFlowBuilder::Move(RegisterIndex Dst, RegisterIndex Src)
{
    Emit(FInstruction::Make(EOpCode::Move, Dst, Src, 0, 0));
    return *this;
}

// ============================================================================
// Arithmetic
// ============================================================================

FFlowBuilder& FFlowBuilder::Add(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Add, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::Sub(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Sub, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::Mul(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Mul, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::Div(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::Div, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::AddImm(RegisterIndex Dst, RegisterIndex Src, int32 Imm)
{
    Emit(FInstruction::Make(EOpCode::AddImm, Dst, Src, 0, Imm & 0xFFF));
    return *this;
}

// ============================================================================
// Comparison
// ============================================================================

FFlowBuilder& FFlowBuilder::CmpEq(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpEq, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::CmpNe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpNe, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::CmpLt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpLt, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::CmpLe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpLe, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::CmpGt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpGt, Dst, Src1, Src2, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::CmpGe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2)
{
    Emit(FInstruction::Make(EOpCode::CmpGe, Dst, Src1, Src2, 0));
    return *this;
}

// ============================================================================
// Entity Management
// ============================================================================

FFlowBuilder& FFlowBuilder::SpawnEntity(const FString& ClassPath)
{
    int32 StrIdx = AddString(ClassPath);
    Emit(FInstruction::MakeImm(EOpCode::SpawnEntity, Reg::Spawned, StrIdx));
    return *this;
}

FFlowBuilder& FFlowBuilder::DestroyEntity(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::DestroyEntity, 0, Entity, 0, 0));
    return *this;
}

// ============================================================================
// Position & Movement
// ============================================================================

FFlowBuilder& FFlowBuilder::GetPosition(RegisterIndex DstBase, RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::GetPosition, DstBase, Entity, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::SetPosition(RegisterIndex Entity, RegisterIndex SrcBase)
{
    Emit(FInstruction::Make(EOpCode::SetPosition, Entity, SrcBase, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Speed)
{
    Emit(FInstruction::Make(EOpCode::MoveToward, Entity, TargetPosBase, 0, Speed & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::MoveForward(RegisterIndex Entity, int32 Speed)
{
    Emit(FInstruction::Make(EOpCode::MoveForward, 0, Entity, 0, Speed & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::StopMovement(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::StopMovement, 0, Entity, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2)
{
    Emit(FInstruction::Make(EOpCode::GetDistance, Dst, Entity1, Entity2, 0));
    return *this;
}

// ============================================================================
// Spatial Query
// ============================================================================

FFlowBuilder& FFlowBuilder::FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm)
{
    Emit(FInstruction::Make(EOpCode::FindInRadius, Reg::Count, CenterEntity, 0, RadiusCm & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::NextFound()
{
    Emit(FInstruction::Make(EOpCode::NextFound, Reg::Iter, 0, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm)
{
    FForEachContext Ctx;
    Ctx.LoopLabel = FString::Printf(TEXT("__foreach_%d_loop"), ForEachCounter);
    Ctx.EndLabel = FString::Printf(TEXT("__foreach_%d_end"), ForEachCounter);
    ForEachCounter++;
    ForEachStack.Push(Ctx);
    
    // FindInRadius(Center, Radius) → Count
    FindInRadius(CenterEntity, RadiusCm);
    
    // Loop:
    Label(Ctx.LoopLabel);
    
    // NextFound() → Iter, Flag
    NextFound();
    
    // JumpIfNot Flag, End
    JumpIfNot(Reg::Flag, Ctx.EndLabel);
    
    return *this;
}

FFlowBuilder& FFlowBuilder::EndForEach()
{
    check(ForEachStack.Num() > 0);
    FForEachContext Ctx = ForEachStack.Pop();
    
    // Jump Loop
    Jump(Ctx.LoopLabel);
    
    // End:
    Label(Ctx.EndLabel);
    
    return *this;
}

// ============================================================================
// Combat
// ============================================================================

FFlowBuilder& FFlowBuilder::ApplyDamage(RegisterIndex Target, RegisterIndex Amount)
{
    Emit(FInstruction::Make(EOpCode::ApplyDamage, 0, Target, Amount, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::ApplyDamageConst(RegisterIndex Target, int32 Amount)
{
    LoadConst(Reg::Temp, Amount);
    ApplyDamage(Target, Reg::Temp);
    return *this;
}

FFlowBuilder& FFlowBuilder::ApplyEffect(RegisterIndex Target, const FString& EffectTag)
{
    int32 StrIdx = AddString(EffectTag);
    Emit(FInstruction::Make(EOpCode::ApplyEffect, 0, Target, 0, StrIdx & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::RemoveEffect(RegisterIndex Target, const FString& EffectTag)
{
    int32 StrIdx = AddString(EffectTag);
    Emit(FInstruction::Make(EOpCode::RemoveEffect, 0, Target, 0, StrIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Animation & VFX
// ============================================================================

FFlowBuilder& FFlowBuilder::PlayAnim(RegisterIndex Entity, const FString& AnimName)
{
    int32 StrIdx = AddString(AnimName);
    Emit(FInstruction::Make(EOpCode::PlayAnim, 0, Entity, 0, StrIdx & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::PlayAnimMontage(RegisterIndex Entity, const FString& MontageName)
{
    int32 StrIdx = AddString(MontageName);
    Emit(FInstruction::Make(EOpCode::PlayAnimMontage, 0, Entity, 0, StrIdx & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::StopAnim(RegisterIndex Entity)
{
    Emit(FInstruction::Make(EOpCode::StopAnim, 0, Entity, 0, 0));
    return *this;
}

FFlowBuilder& FFlowBuilder::PlayVFX(RegisterIndex PosBase, const FString& VFXPath)
{
    int32 StrIdx = AddString(VFXPath);
    Emit(FInstruction::Make(EOpCode::PlayVFX, 0, PosBase, 0, StrIdx & 0xFFF));
    return *this;
}

FFlowBuilder& FFlowBuilder::PlayVFXAttached(RegisterIndex Entity, const FString& VFXPath)
{
    int32 StrIdx = AddString(VFXPath);
    Emit(FInstruction::Make(EOpCode::PlayVFXAttached, 0, Entity, 0, StrIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Audio
// ============================================================================

FFlowBuilder& FFlowBuilder::PlaySound(const FString& SoundPath)
{
    int32 StrIdx = AddString(SoundPath);
    Emit(FInstruction::MakeImm(EOpCode::PlaySound, 0, StrIdx));
    return *this;
}

FFlowBuilder& FFlowBuilder::PlaySoundAtLocation(RegisterIndex PosBase, const FString& SoundPath)
{
    int32 StrIdx = AddString(SoundPath);
    Emit(FInstruction::Make(EOpCode::PlaySoundAtLocation, 0, PosBase, 0, StrIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Equipment
// ============================================================================

FFlowBuilder& FFlowBuilder::SpawnEquipment(RegisterIndex Owner, int32 Slot, const FString& EquipClass)
{
    int32 StrIdx = AddString(EquipClass);
    // Slot은 Src2, StringIdx는 Imm12
    Emit(FInstruction::Make(EOpCode::SpawnEquipment, Reg::Spawned, Owner, Slot & 0xF, StrIdx & 0xFFF));
    return *this;
}

// ============================================================================
// Utility
// ============================================================================

FFlowBuilder& FFlowBuilder::Log(const FString& Message)
{
    int32 StrIdx = AddString(Message);
    Emit(FInstruction::MakeImm(EOpCode::Log, 0, StrIdx));
    return *this;
}

// ============================================================================
// Build
// ============================================================================

void FFlowBuilder::ResolveLabels()
{
    for (const auto& Fixup : Fixups)
    {
        int32 CodeIndex = Fixup.Key;
        const FString& LabelName = Fixup.Value;
        
        if (const int32* Target = Labels.Find(LabelName))
        {
            FInstruction& Inst = Program.Code[CodeIndex];
            
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
            UE_LOG(LogTemp, Error, TEXT("Unresolved label: %s in Flow %s"), *LabelName, *Program.Tag.ToString());
        }
    }
}

FHktVMProgram FFlowBuilder::Build()
{
    if (Program.Code.Num() == 0 || Program.Code.Last().GetOpCode() != EOpCode::Halt)
    {
        Halt();
    }
    
    ResolveLabels();
    return MoveTemp(Program);
}

void FFlowBuilder::BuildAndRegister()
{
    FHktVMProgramRegistry::Get().RegisterProgram(Build());
}