// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VM/HktInstruction.h"

// ============================================================================
// Program
// ============================================================================

/**
 * FHktVMProgram - 컴파일된 바이트코드 프로그램 (불변, 공유 가능)
 */
struct FHktVMProgram
{
    FGameplayTag Tag;
    TArray<FInstruction> Code;
    TArray<int32> Constants;
    TArray<FString> Strings;
    TArray<int32> LineNumbers;

    bool IsValid() const { return Code.Num() > 0; }
    int32 CodeSize() const { return Code.Num(); }
};

// ============================================================================
// Registry
// ============================================================================

/**
 * FHktVMProgramRegistry - EventTag → Program 매핑 관리
 */
class HKTCORE_API FHktVMProgramRegistry
{
public:
    static FHktVMProgramRegistry& Get();

    const FHktVMProgram* FindProgram(const FGameplayTag& Tag) const;
    void RegisterProgram(FHktVMProgram&& Program);
    void Clear();

private:
    FHktVMProgramRegistry() = default;

    TMap<FGameplayTag, TSharedPtr<FHktVMProgram>> Programs;
    mutable FRWLock Lock;
};

// ============================================================================
// Fluent Builder API
// ============================================================================

/**
 * FFlowBuilder - 자연어처럼 읽히는 Flow 정의
 *
 * 사용 예:
 *   Flow("Ability.Skill.Fireball")
 *       .PlayAnim(Self, "CastStart")
 *       .WaitSeconds(1.0f)
 *       .SpawnEntity("Fireball")
 *       .MoveForward(Spawned, 500)
 *       .WaitCollision()
 *       .ApplyDamageConst(Hit, 100)
 *       .Halt();
 */
class HKTCORE_API FFlowBuilder
{
public:
    static FFlowBuilder Create(const FGameplayTag& Tag);
    static FFlowBuilder Create(const FName& TagName);

    // ========== Control Flow ==========

    FFlowBuilder& Label(const FString& Name);
    FFlowBuilder& Jump(const FString& Label);
    FFlowBuilder& JumpIf(RegisterIndex Cond, const FString& Label);
    FFlowBuilder& JumpIfNot(RegisterIndex Cond, const FString& Label);
    FFlowBuilder& Yield(int32 Frames = 1);
    FFlowBuilder& WaitSeconds(float Seconds);
    FFlowBuilder& Halt();

    // ========== Event Wait ==========

    FFlowBuilder& WaitCollision(RegisterIndex WatchEntity = Reg::Spawned);
    FFlowBuilder& WaitAnimEnd(RegisterIndex Entity = Reg::Self);
    FFlowBuilder& WaitMoveEnd(RegisterIndex Entity = Reg::Self);

    // ========== Data Operations ==========

    FFlowBuilder& LoadConst(RegisterIndex Dst, int32 Value);
    FFlowBuilder& LoadStore(RegisterIndex Dst, uint16 PropertyId);
    FFlowBuilder& LoadEntityProperty(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId);
    FFlowBuilder& SaveStore(uint16 PropertyId, RegisterIndex Src);
    FFlowBuilder& SaveEntityProperty(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src);
    FFlowBuilder& Move(RegisterIndex Dst, RegisterIndex Src);

    // ========== Arithmetic ==========

    FFlowBuilder& Add(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& Sub(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& Mul(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& Div(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& AddImm(RegisterIndex Dst, RegisterIndex Src, int32 Imm);

    // ========== Comparison ==========

    FFlowBuilder& CmpEq(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& CmpNe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& CmpLt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& CmpLe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& CmpGt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FFlowBuilder& CmpGe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);

    // ========== Entity Management ==========

    FFlowBuilder& SpawnEntity(const FString& ClassPath);
    FFlowBuilder& DestroyEntity(RegisterIndex Entity);

    // ========== Position & Movement ==========

    FFlowBuilder& GetPosition(RegisterIndex DstBase, RegisterIndex Entity);
    FFlowBuilder& SetPosition(RegisterIndex Entity, RegisterIndex SrcBase);
    FFlowBuilder& MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Speed);
    FFlowBuilder& MoveForward(RegisterIndex Entity, int32 Speed);
    FFlowBuilder& StopMovement(RegisterIndex Entity);
    FFlowBuilder& GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2);

    // ========== Spatial Query ==========

    FFlowBuilder& FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm);
    FFlowBuilder& NextFound();
    FFlowBuilder& ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm);
    FFlowBuilder& EndForEach();

    // ========== Combat ==========

    FFlowBuilder& ApplyDamage(RegisterIndex Target, RegisterIndex Amount);
    FFlowBuilder& ApplyDamageConst(RegisterIndex Target, int32 Amount);
    FFlowBuilder& ApplyEffect(RegisterIndex Target, const FString& EffectTag);
    FFlowBuilder& RemoveEffect(RegisterIndex Target, const FString& EffectTag);

    // ========== Animation & VFX ==========

    FFlowBuilder& PlayAnim(RegisterIndex Entity, const FString& AnimName);
    FFlowBuilder& PlayAnimMontage(RegisterIndex Entity, const FString& MontageName);
    FFlowBuilder& StopAnim(RegisterIndex Entity);
    FFlowBuilder& PlayVFX(RegisterIndex PosBase, const FString& VFXPath);
    FFlowBuilder& PlayVFXAttached(RegisterIndex Entity, const FString& VFXPath);

    // ========== Audio ==========

    FFlowBuilder& PlaySound(const FString& SoundPath);
    FFlowBuilder& PlaySoundAtLocation(RegisterIndex PosBase, const FString& SoundPath);

    // ========== Equipment ==========

    FFlowBuilder& SpawnEquipment(RegisterIndex Owner, int32 Slot, const FString& EquipClass);

    // ========== Utility ==========

    FFlowBuilder& Log(const FString& Message);

    // ========== Build ==========

    FHktVMProgram Build();
    void BuildAndRegister();

private:
    explicit FFlowBuilder(const FGameplayTag& Tag);

    void Emit(FInstruction Inst);
    int32 AddString(const FString& Str);
    int32 AddConstant(int32 Value);
    void ResolveLabels();

private:
    FHktVMProgram Program;
    TMap<FString, int32> Labels;
    TArray<TPair<int32, FString>> Fixups;

    struct FForEachContext
    {
        FString LoopLabel;
        FString EndLabel;
    };
    TArray<FForEachContext> ForEachStack;
    int32 ForEachCounter = 0;
};

// ============================================================================
// 편의 함수
// ============================================================================

/** 간단한 Flow 생성 시작 */
inline FFlowBuilder Flow(const FName& TagName)
{
    return FFlowBuilder::Create(TagName);
}
