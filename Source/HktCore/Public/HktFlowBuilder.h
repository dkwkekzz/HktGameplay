// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"
#include "HktFlowTypes.h"

struct FHktVMProgram;

// ============================================================================
// Fluent Builder API - 자연어 스타일
// ============================================================================

/**
 * FHktFlowBuilder - 자연어처럼 읽히는 Flow 정의
 * 
 * Public 헤더: 외부 모듈에서 #include "HktFlowBuilder.h" 로 Flow 정의 가능
 * 
 * 사용 예:
 *   Flow(TEXT("Ability.Skill.Fireball"))
 *       .PlayAnim(Self, TAG_Anim_CastStart)
 *       .WaitSeconds(1.0f)
 *       .SpawnEntity(HktType::Projectile, TAG_Entity_Fireball).MoveForward(500)
 *       .OnCollision()
 *           .DestroyEntity(Spawned)
 *           .ApplyDamageConst(Hit, 100)
 *           .ForEachInRadius(Hit, 300)
 *               .ApplyDamageConst(Iter, 50)
 *           .EndForEach()
 *       .End();
 */
class HKTCORE_API FHktFlowBuilder
{
public:
    static FHktFlowBuilder Create(const FGameplayTag& Tag);
    static FHktFlowBuilder Create(const FName& TagName);

    // ========== Flow Policy ==========

    /** 같은 엔티티에 동일 이벤트가 중복 발생 시 기존 VM을 취소 (예: MoveTo) */
    FHktFlowBuilder& CancelOnDuplicate();

    // ========== Control Flow ==========

    /** 라벨 정의 (점프 대상) */
    FHktFlowBuilder& Label(const FString& Name);

    /** 무조건 점프 */
    FHktFlowBuilder& Jump(const FString& LabelName);

    /** 조건부 점프 */
    FHktFlowBuilder& JumpIf(RegisterIndex Cond, const FString& LabelName);
    FHktFlowBuilder& JumpIfNot(RegisterIndex Cond, const FString& LabelName);

    /** 다음 프레임까지 대기 */
    FHktFlowBuilder& Yield(int32 Frames = 1);

    /** N초 대기 */
    FHktFlowBuilder& WaitSeconds(float Seconds);

    /** 프로그램 종료 */
    FHktFlowBuilder& Halt();

    // ========== Event Wait ==========

    /** 충돌 대기 - 충돌 시 Hit 레지스터에 대상 저장 */
    FHktFlowBuilder& WaitCollision(RegisterIndex WatchEntity = Reg::Spawned);

    /** 애니메이션 종료 대기 */
    FHktFlowBuilder& WaitAnimEnd(RegisterIndex Entity = Reg::Self);

    /** 이동 완료 대기 */
    FHktFlowBuilder& WaitMoveEnd(RegisterIndex Entity = Reg::Self);

    // ========== Data Operations ==========

    FHktFlowBuilder& LoadConst(RegisterIndex Dst, int32 Value);
    FHktFlowBuilder& LoadStore(RegisterIndex Dst, uint16 PropertyId);
    FHktFlowBuilder& LoadEntityProperty(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId);
    FHktFlowBuilder& SaveStore(uint16 PropertyId, RegisterIndex Src);
    FHktFlowBuilder& SaveEntityProperty(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src);
    FHktFlowBuilder& Move(RegisterIndex Dst, RegisterIndex Src);

    // ========== Arithmetic ==========

    FHktFlowBuilder& Add(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& Sub(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& Mul(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& Div(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& AddImm(RegisterIndex Dst, RegisterIndex Src, int32 Imm);

    // ========== Comparison ==========

    FHktFlowBuilder& CmpEq(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& CmpNe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& CmpLt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& CmpLe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& CmpGt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktFlowBuilder& CmpGe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);

    // ========== Entity Management ==========

    /** 엔티티 스폰 → Spawned 레지스터에 저장 */
    FHktFlowBuilder& SpawnEntity(FHktTypeId TypeId, const FGameplayTag& ClassTag);

    /** 엔티티 제거 */
    FHktFlowBuilder& DestroyEntity(RegisterIndex Entity);

    // ========== Position & Movement ==========

    /** 위치 가져오기: (Dst, Dst+1, Dst+2) = Position */
    FHktFlowBuilder& GetPosition(RegisterIndex DstBase, RegisterIndex Entity);

    /** 위치 설정: Position = (SrcBase, SrcBase+1, SrcBase+2) */
    FHktFlowBuilder& SetPosition(RegisterIndex Entity, RegisterIndex SrcBase);

    /** 목표 위치로 이동 시작 (Force 단위, F=ma) */
    FHktFlowBuilder& MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Force);

    /** 전방으로 이동 (투사체용, Force 단위) */
    FHktFlowBuilder& MoveForward(RegisterIndex Entity, int32 Force);

    /** 이동 중지 */
    FHktFlowBuilder& StopMovement(RegisterIndex Entity);

    /** 거리 계산 */
    FHktFlowBuilder& GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2);

    // ========== Spatial Query ==========

    /** 범위 내 엔티티 검색 시작 */
    FHktFlowBuilder& FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm);

    /** 다음 검색 결과 → Iter, 끝이면 Flag=0 */
    FHktFlowBuilder& NextFound();

    /** ForEach 편의 메서드 (FindInRadius + 루프) */
    FHktFlowBuilder& ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm);
    FHktFlowBuilder& EndForEach();

    // ========== Combat ==========

    /** 데미지 적용 */
    FHktFlowBuilder& ApplyDamage(RegisterIndex Target, RegisterIndex Amount);
    FHktFlowBuilder& ApplyDamageConst(RegisterIndex Target, int32 Amount);

    /** 이펙트 적용 (버프/디버프) */
    FHktFlowBuilder& ApplyEffect(RegisterIndex Target, const FGameplayTag& EffectTag);

    /** 이펙트 제거 */
    FHktFlowBuilder& RemoveEffect(RegisterIndex Target, const FGameplayTag& EffectTag);

    // ========== Animation & VFX ==========

    /** 애니메이션 재생 */
    FHktFlowBuilder& PlayAnim(RegisterIndex Entity, const FGameplayTag& AnimTag);

    /** 몽타주 재생 */
    FHktFlowBuilder& PlayAnimMontage(RegisterIndex Entity, const FGameplayTag& MontageTag);

    /** 애니메이션 중지 */
    FHktFlowBuilder& StopAnim(RegisterIndex Entity);

    /** VFX 재생 (위치) */
    FHktFlowBuilder& PlayVFX(RegisterIndex PosBase, const FGameplayTag& VFXTag);

    /** VFX 재생 (엔티티에 부착) */
    FHktFlowBuilder& PlayVFXAttached(RegisterIndex Entity, const FGameplayTag& VFXTag);

    // ========== Audio ==========

    FHktFlowBuilder& PlaySound(const FGameplayTag& SoundTag);
    FHktFlowBuilder& PlaySoundAtLocation(RegisterIndex PosBase, const FGameplayTag& SoundTag);

    // ========== Equipment ==========

    /** 장비 스폰 및 부착 */
    FHktFlowBuilder& SpawnEquipment(RegisterIndex Owner, int32 Slot, const FGameplayTag& EquipTag);

    // ========== Utility ==========

    FHktFlowBuilder& Log(const FString& Message);

    // ========== Build ==========

    TSharedRef<FHktVMProgram> Build();
    void BuildAndRegister();

private:
    explicit FHktFlowBuilder(const FGameplayTag& Tag);

    void Emit(FInstruction Inst);
    int32 AddString(const FString& Str);
    int32 AddConstant(int32 Value);
    int32 TagToInt(const FGameplayTag& Tag);
    void ResolveLabels();

private:
    TSharedRef<FHktVMProgram> Program;
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
inline FHktFlowBuilder Flow(FGameplayTag TagName)
{
    return FHktFlowBuilder::Create(TagName);
}
