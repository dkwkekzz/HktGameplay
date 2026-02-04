#pragma once

#include "CoreMinimal.h"
#include "HktVMTypes.h"

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

/**
 * FHktVMProgramRegistry - EventTag → Program 매핑 관리
 */
class FHktVMProgramRegistry
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
// Fluent Builder API - 자연어 스타일
// ============================================================================

/**
 * FFlowBuilder - 자연어처럼 읽히는 Flow 정의
 * 
 * 사용 예:
 *   Flow("Ability.Skill.Fireball")
 *       .PlayAnim(Self, "CastStart")
 *       .WaitSeconds(1.0f)
 *       .SpawnProjectile("Fireball").MoveForward(500)
 *       .OnCollision()
 *           .DestroyEntity(Spawned)
 *           .DamageTarget(100)
 *           .ForEachInRadius(300)
 *               .Damage(50)
 *               .ApplyBurn()
 *           .EndForEach()
 *       .End();
 */
class FFlowBuilder
{
public:
    static FFlowBuilder Create(const FGameplayTag& Tag);
    static FFlowBuilder Create(const FName& TagName);
    
    // ========== Control Flow ==========
    
    /** 라벨 정의 (점프 대상) */
    FFlowBuilder& Label(const FString& Name);
    
    /** 무조건 점프 */
    FFlowBuilder& Jump(const FString& Label);
    
    /** 조건부 점프 */
    FFlowBuilder& JumpIf(RegisterIndex Cond, const FString& Label);
    FFlowBuilder& JumpIfNot(RegisterIndex Cond, const FString& Label);
    
    /** 다음 프레임까지 대기 */
    FFlowBuilder& Yield(int32 Frames = 1);
    
    /** N초 대기 */
    FFlowBuilder& WaitSeconds(float Seconds);
    
    /** 프로그램 종료 */
    FFlowBuilder& Halt();
    
    // ========== Event Wait ==========
    
    /** 충돌 대기 - 충돌 시 Hit 레지스터에 대상 저장 */
    FFlowBuilder& WaitCollision(RegisterIndex WatchEntity = Reg::Spawned);
    
    /** 애니메이션 종료 대기 */
    FFlowBuilder& WaitAnimEnd(RegisterIndex Entity = Reg::Self);
    
    /** 이동 완료 대기 */
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
    
    /** 엔티티 스폰 → Spawned 레지스터에 저장 */
    FFlowBuilder& SpawnEntity(const FString& ClassPath);
    
    /** 엔티티 제거 */
    FFlowBuilder& DestroyEntity(RegisterIndex Entity);
    
    // ========== Position & Movement ==========
    
    /** 위치 가져오기: (Dst, Dst+1, Dst+2) = Position */
    FFlowBuilder& GetPosition(RegisterIndex DstBase, RegisterIndex Entity);
    
    /** 위치 설정: Position = (SrcBase, SrcBase+1, SrcBase+2) */
    FFlowBuilder& SetPosition(RegisterIndex Entity, RegisterIndex SrcBase);
    
    /** 목표 위치로 이동 시작 */
    FFlowBuilder& MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Speed);
    
    /** 전방으로 이동 (투사체용) */
    FFlowBuilder& MoveForward(RegisterIndex Entity, int32 Speed);
    
    /** 이동 중지 */
    FFlowBuilder& StopMovement(RegisterIndex Entity);
    
    /** 거리 계산 */
    FFlowBuilder& GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2);
    
    // ========== Spatial Query ==========
    
    /** 범위 내 엔티티 검색 시작 */
    FFlowBuilder& FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm);
    
    /** 다음 검색 결과 → Iter, 끝이면 Flag=0 */
    FFlowBuilder& NextFound();
    
    /** ForEach 편의 메서드 (FindInRadius + 루프) */
    FFlowBuilder& ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm);
    FFlowBuilder& EndForEach();
    
    // ========== Combat ==========
    
    /** 데미지 적용 */
    FFlowBuilder& ApplyDamage(RegisterIndex Target, RegisterIndex Amount);
    FFlowBuilder& ApplyDamageConst(RegisterIndex Target, int32 Amount);
    
    /** 이펙트 적용 (버프/디버프) */
    FFlowBuilder& ApplyEffect(RegisterIndex Target, const FString& EffectTag);
    
    /** 이펙트 제거 */
    FFlowBuilder& RemoveEffect(RegisterIndex Target, const FString& EffectTag);
    
    // ========== Animation & VFX ==========
    
    /** 애니메이션 재생 */
    FFlowBuilder& PlayAnim(RegisterIndex Entity, const FString& AnimName);
    
    /** 몽타주 재생 */
    FFlowBuilder& PlayAnimMontage(RegisterIndex Entity, const FString& MontageName);
    
    /** 애니메이션 중지 */
    FFlowBuilder& StopAnim(RegisterIndex Entity);
    
    /** VFX 재생 (위치) */
    FFlowBuilder& PlayVFX(RegisterIndex PosBase, const FString& VFXPath);
    
    /** VFX 재생 (엔티티에 부착) */
    FFlowBuilder& PlayVFXAttached(RegisterIndex Entity, const FString& VFXPath);
    
    // ========== Audio ==========
    
    FFlowBuilder& PlaySound(const FString& SoundPath);
    FFlowBuilder& PlaySoundAtLocation(RegisterIndex PosBase, const FString& SoundPath);
    
    // ========== Equipment ==========
    
    /** 장비 스폰 및 부착 */
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
    
    // ForEach 스택 (중첩 지원)
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
