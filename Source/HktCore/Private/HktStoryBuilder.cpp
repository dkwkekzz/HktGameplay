// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryBuilder.h"
#include "HktCoreLog.h"
#include "HktCoreProperties.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "VM/HktVMProgram.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY(LogHktCore);

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

FHktStoryBuilder::FHktStoryBuilder(FHktStoryBuilder&& Other) noexcept
    : Program(MoveTemp(Other.Program))
    , MainSection(MoveTemp(Other.MainSection))
    , PreconditionSection(MoveTemp(Other.PreconditionSection))
    , ForEachStack(MoveTemp(Other.ForEachStack))
    , ForEachCounter(Other.ForEachCounter)
    , InternalLabelCounter(Other.InternalLabelCounter)
{
    // ActiveSection 포인터 재조정: 원본이 어느 섹션을 가리키고 있었는지에 따라 결정
    ActiveSection = (Other.ActiveSection == &Other.PreconditionSection)
        ? &PreconditionSection
        : &MainSection;
}

void FHktStoryBuilder::Emit(FInstruction Inst)
{
    ActiveSection->Code.Add(Inst);
}

int32 FHktStoryBuilder::AddString(const FString& Str)
{
    int32 Index = ActiveSection->Strings.IndexOfByKey(Str);
    if (Index == INDEX_NONE)
    {
        Index = ActiveSection->Strings.Num();
        ActiveSection->Strings.Add(Str);
    }
    return Index;
}

int32 FHktStoryBuilder::AddConstant(int32 Value)
{
    int32 Index = ActiveSection->Constants.IndexOfByKey(Value);
    if (Index == INDEX_NONE)
    {
        Index = ActiveSection->Constants.Num();
        ActiveSection->Constants.Add(Value);
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

FHktStoryBuilder& FHktStoryBuilder::BeginPrecondition()
{
    check(ActiveSection == &MainSection);
    ActiveSection = &PreconditionSection;
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::EndPrecondition()
{
    check(ActiveSection == &PreconditionSection);
    ResolveLabels(PreconditionSection, Program->Tag);
    ActiveSection = &MainSection;
    return *this;
}

// ============================================================================
// Control Flow
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::Label(const FString& Name)
{
    ActiveSection->Labels.Add(Name, ActiveSection->Code.Num());
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::Jump(const FString& LabelName)
{
    ActiveSection->Fixups.Add({ActiveSection->Code.Num(), LabelName});
    Emit(FInstruction::MakeImm(EOpCode::Jump, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::JumpIf(RegisterIndex Cond, const FString& LabelName)
{
    ActiveSection->Fixups.Add({ActiveSection->Code.Num(), LabelName});
    Emit(FInstruction::Make(EOpCode::JumpIf, 0, Cond, 0, 0));
    return *this;
}

FHktStoryBuilder& FHktStoryBuilder::JumpIfNot(RegisterIndex Cond, const FString& LabelName)
{
    ActiveSection->Fixups.Add({ActiveSection->Code.Num(), LabelName});
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
// Item Skill
// ============================================================================

FHktStoryBuilder& FHktStoryBuilder::SetItemSkillTag(RegisterIndex Entity, const FGameplayTag& SkillTag)
{
    int32 TagIdx = TagToInt(SkillTag);
    SaveConstEntity(Entity, PropertyId::ItemSkillTag, TagIdx);
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

void FHktStoryBuilder::ResolveLabels(FCodeSection& Section, const FGameplayTag& Tag)
{
    for (const auto& Fixup : Section.Fixups)
    {
        int32 CodeIndex = Fixup.Key;
        const FString& LabelName = Fixup.Value;

        if (const int32* Target = Section.Labels.Find(LabelName))
        {
            FInstruction& Inst = Section.Code[CodeIndex];

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
            UE_LOG(LogHktCore, Error, TEXT("Unresolved label: %s in Flow %s"), *LabelName, *Tag.ToString());
        }
    }
}

TSharedPtr<FHktVMProgram> FHktStoryBuilder::Build()
{
    if (MainSection.Code.Num() == 0 || MainSection.Code.Last().GetOpCode() != EOpCode::Halt)
    {
        Halt();
    }

    ResolveLabels(MainSection, Program->Tag);

    // MainSection → Program
    Program->Code = MoveTemp(MainSection.Code);
    Program->Constants = MoveTemp(MainSection.Constants);
    Program->Strings = MoveTemp(MainSection.Strings);

    if (!ValidateEntityFlow())
    {
        UE_LOG(LogHktCore, Error,
            TEXT("Story BUILD FAILED: %s — 엔티티 레지스터 검증 실패. 이 Story는 등록되지 않습니다."),
            *Program->Tag.ToString());
        return nullptr;
    }

    // 범용 레지스터 흐름 검증 (Warning — 빌드 중단 없음)
    ValidateRegisterFlow();

    // PreconditionSection → Program
    if (PreconditionSection.Code.Num() > 0)
    {
        Program->PreconditionCode = MoveTemp(PreconditionSection.Code);
        Program->PreconditionConstants = MoveTemp(PreconditionSection.Constants);
        Program->PreconditionStrings = MoveTemp(PreconditionSection.Strings);
    }

    return Program;
}

// ============================================================================
// Build-time Entity Register Validation
// ============================================================================

bool FHktStoryBuilder::ValidateEntityFlow()
{
    bool bValid = true;
    // Self(R10), Target(R11)은 이벤트에서 항상 초기화됨
    // Spawned(R12), Hit(R13), Iter(R14)는 특정 Op 실행 후에만 유효
    uint16 EntityRegs = (1 << Reg::Self) | (1 << Reg::Target);

    auto GetEntityRegName = [](RegisterIndex R) -> const TCHAR*
    {
        switch (R)
        {
        case Reg::Self:    return TEXT("Self");
        case Reg::Target:  return TEXT("Target");
        case Reg::Spawned: return TEXT("Spawned");
        case Reg::Hit:     return TEXT("Hit");
        case Reg::Iter:    return TEXT("Iter");
        default:           return nullptr;
        }
    };

    // 특수 엔티티 레지스터(R10~R14)가 초기화되기 전에 사용되는지 검사
    auto CheckEntityReg = [&](int32 PC, EOpCode Op, RegisterIndex R)
    {
        // R0~R9, Flag/Count(R15)는 범용이므로 검사 대상이 아님
        const TCHAR* Name = GetEntityRegName(R);
        if (!Name)
            return;

        if (!(EntityRegs & (1 << R)))
        {
            UE_LOG(LogHktCore, Error,
                TEXT("Story BUILD: %s PC=%d Op=%s — Reg %s (R%d) 가 엔티티로 사용되었지만 이전에 초기화되지 않았습니다. "
                     "SpawnEntity/WaitCollision/NextFound 호출 순서를 확인하세요."),
                *Program->Tag.ToString(), PC, GetOpCodeName(Op), Name, R);
            bValid = false;
        }
    };

    for (int32 PC = 0; PC < Program->Code.Num(); ++PC)
    {
        const FInstruction& Inst = Program->Code[PC];
        EOpCode Op = Inst.GetOpCode();

        switch (Op)
        {
        // --- Entity register writers ---
        case EOpCode::SpawnEntity:
            EntityRegs |= (1 << Reg::Spawned);
            break;
        case EOpCode::WaitCollision:
            CheckEntityReg(PC, Op, Inst.Src1);
            EntityRegs |= (1 << Reg::Hit);
            break;
        case EOpCode::NextFound:
            EntityRegs |= (1 << Reg::Iter);
            break;

        // --- Entity register readers (Src1 = entity) ---
        case EOpCode::LoadStoreEntity:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::SaveStoreEntity:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::DestroyEntity:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::FindInRadius:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::GetDistance:
            CheckEntityReg(PC, Op, Inst.Src1);
            CheckEntityReg(PC, Op, Inst.Src2);
            break;
        case EOpCode::AddTag:
        case EOpCode::RemoveTag:
        case EOpCode::HasTag:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::PlayVFXAttached:
        case EOpCode::ApplyEffect:
        case EOpCode::RemoveEffect:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::SetOwnerUid:
        case EOpCode::ClearOwnerUid:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::CountByOwner:
        case EOpCode::FindByOwner:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;
        case EOpCode::WaitMoveEnd:
            CheckEntityReg(PC, Op, Inst.Src1);
            break;

        default:
            break;
        }
    }

    return bValid;
}

// ============================================================================
// Build-time General Register Flow Validation
// ============================================================================

void FHktStoryBuilder::ValidateRegisterFlow()
{
    /**
     * 범용 레지스터(R0~R8) 흐름을 선형 스캔하여 두 가지 패턴을 감지:
     *
     * 1. Read-before-Write: 초기화 안 된 레지스터를 읽는 경우
     *    → 이전 Story 실행의 잔류값에 의존하는 잠재 버그
     *
     * 2. Dead Write (Write-Write-without-Read): 값을 쓰고 읽지 않고 다시 덮어쓰는 경우
     *    → Snippet 파라미터 레지스터가 내부 temp에 의해 덮어씌워지는 유형의 버그
     *
     * Label(합류점)에서는 상태를 보수적으로 리셋하여 오탐을 방지한다.
     */
    constexpr int32 NumGPRegs = 9;  // R0~R8

    // Unknown=초기화 안 됨, Written=쓰기 후 읽기 전, Read=정상 소비됨
    enum class ERegState : uint8 { Unknown, Written, Read };
    ERegState State[NumGPRegs];
    int32 WritePC[NumGPRegs];       // Dead Write 경고 시 첫 Write 위치 보고용
    for (int32 i = 0; i < NumGPRegs; ++i)
    {
        State[i] = ERegState::Unknown;
        WritePC[i] = -1;
    }

    // Label 위치 수집 (합류점 판정용)
    TSet<int32> LabelPCs;
    for (const auto& Pair : MainSection.Labels)
    {
        LabelPCs.Add(Pair.Value);
    }

    auto MarkRead = [&](int32 PC, EOpCode Op, RegisterIndex R)
    {
        if (R >= NumGPRegs) return;
        if (State[R] == ERegState::Unknown)
        {
            UE_LOG(LogHktCore, Warning,
                TEXT("Story REGFLOW: %s PC=%d Op=%s — R%d Read-before-Write. "
                     "초기화되지 않은 레지스터를 읽고 있습니다."),
                *Program->Tag.ToString(), PC, GetOpCodeName(Op), R);
        }
        State[R] = ERegState::Read;
    };

    auto MarkWrite = [&](int32 PC, EOpCode Op, RegisterIndex R)
    {
        if (R >= NumGPRegs) return;
        if (State[R] == ERegState::Written)
        {
            UE_LOG(LogHktCore, Warning,
                TEXT("Story REGFLOW: %s PC=%d Op=%s — R%d Dead Write. "
                     "PC=%d에서 쓴 값을 읽지 않고 덮어쓰고 있습니다. 레지스터 충돌을 확인하세요."),
                *Program->Tag.ToString(), PC, GetOpCodeName(Op), R, WritePC[R]);
        }
        State[R] = ERegState::Written;
        WritePC[R] = PC;
    };

    for (int32 PC = 0; PC < Program->Code.Num(); ++PC)
    {
        // Label 합류점: 다른 경로에서 올 수 있으므로 상태를 보수적으로 Read로 리셋
        // (오탐 방지 — 초기화되었을 수도 있고 아닐 수도 있음)
        if (LabelPCs.Contains(PC))
        {
            for (int32 i = 0; i < NumGPRegs; ++i)
            {
                State[i] = ERegState::Read;
            }
        }

        const FInstruction& Inst = Program->Code[PC];
        EOpCode Op = Inst.GetOpCode();
        FOpRegInfo Info = GetOpRegInfo(Op);

        // Read를 먼저 처리 (같은 명령에서 Read+Write면 Read가 먼저 발생)
        if (Info.Src1 == ERegRole::Read)
            MarkRead(PC, Op, Inst.Src1);
        if (Info.Src2 == ERegRole::Read)
            MarkRead(PC, Op, Inst.Src2);

        // Write 처리
        if (Info.Dst == ERegRole::Write)
            MarkWrite(PC, Op, Inst.Dst);
    }
}

void FHktStoryBuilder::BuildAndRegister()
{
    TSharedPtr<FHktVMProgram> BuiltProgram = Build();
    if (BuiltProgram.IsValid())
    {
        FHktVMProgramRegistry::Get().RegisterProgram(BuiltProgram.ToSharedRef());
    }
}



// ============================================================================
// Public Query API
// ============================================================================

bool HktStory::ValidateEvent(const FHktWorldState& WorldState, const FHktEvent& Event)
{
    return FHktVMProgramRegistry::Get().ValidateEvent(WorldState, Event);
}
