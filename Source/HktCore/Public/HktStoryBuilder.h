// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"
#include "HktStoryTypes.h"

struct FHktVMProgram;
struct FHktWorldState;
struct FHktEvent;

/**
 * FHktEventPrecondition — Story 사전조건 검증 함수
 *
 * 각 Story가 자신의 실행 조건을 C++ 함수로 등록한다.
 * 클라이언트는 Proxy WorldState로 호출하여 요청 가능 여부를 사전 판단하고,
 * 서버는 Story 바이트코드 내부 검증이 권위적 최종 검증으로 작동한다.
 */
using FHktEventPrecondition = TFunction<bool(const FHktWorldState& WorldState, const FHktEvent& Event)>;

// ============================================================================
// Fluent Builder API - 자연어 스타일
// ============================================================================

/**
 * FHktStoryBuilder - 자연어처럼 읽히는 Flow 정의
 *
 * VM은 근본 연산만 opcode로 제공:
 *  - Entity 생성/파괴
 *  - Entity Property 읽기/쓰기 (LoadStore, SaveStore 등)
 *  - Entity Tag 추가/제거
 *
 * 조합 연산(Position, Movement, Damage 등)은 이 Builder에서 기본 opcode를 조합하여 구현.
 *
 * 사용 예:
 *   Story(TEXT("Ability.Skill.Fireball"))
 *       .AddTag(Self, TAG_Anim_UpperBody_Cast_Fireball)
 *       .WaitSeconds(1.0f)
 *       .SpawnEntity(TAG_Entity_Fireball).MoveForward(Spawned, 500)
 *       .WaitCollision()
 *           .DestroyEntity(Spawned)
 *           .ApplyDamageConst(Hit, 100)
 *           .ForEachInRadius(Hit, 300)
 *               .ApplyDamageConst(Iter, 50)
 *           .EndForEach()
 *       .RemoveTag(Self, TAG_Anim_UpperBody_Cast_Fireball)
 *       .End();
 */
/**
 * FCodeSection — Builder 내부 코드 섹션 (Main / Precondition 공용)
 *
 * Emit, AddString, AddConstant, Label, Jump 등이 모두 ActiveSection 포인터를 통해
 * 이 구조체에 쓰기하므로, 새로운 섹션 추가 시 분기 코드가 불필요하다.
 */
struct FCodeSection
{
    TArray<FInstruction> Code;
    TArray<int32> Constants;
    TArray<FString> Strings;
    TMap<FString, int32> Labels;
    TArray<TPair<int32, FString>> Fixups;
};

class HKTCORE_API FHktStoryBuilder
{
public:
    static FHktStoryBuilder Create(const FGameplayTag& Tag);
    static FHktStoryBuilder Create(const FName& TagName);

    // ActiveSection이 자기 멤버(MainSection/PreconditionSection)를 가리키므로
    // implicit copy/move는 댕글링 포인터를 만든다. 복사 금지, move는 재조정.
    FHktStoryBuilder(const FHktStoryBuilder&) = delete;
    FHktStoryBuilder& operator=(const FHktStoryBuilder&) = delete;
    FHktStoryBuilder(FHktStoryBuilder&& Other) noexcept;
    FHktStoryBuilder& operator=(FHktStoryBuilder&&) = delete;

    // ========== Story Policy ==========

    /** 같은 엔티티에 동일 이벤트가 중복 발생 시 기존 VM을 취소 (예: MoveTo) */
    FHktStoryBuilder& CancelOnDuplicate();

    /** Story 사전조건 등록 — 클라이언트/서버 양측에서 호출 가능한 검증 함수 */
    FHktStoryBuilder& SetPrecondition(FHktEventPrecondition InPrecondition);

    /**
     * Precondition 바이트코드 모드 — Begin/End 사이의 모든 Emit은 PreconditionCode로 전달.
     * 기존 step ops와 동일한 fluent API를 사용하되, 읽기 전용 ops만 허용.
     * 실행 후 Flag 레지스터 != 0이면 precondition pass.
     */
    FHktStoryBuilder& BeginPrecondition();
    FHktStoryBuilder& EndPrecondition();

    // ========== Control Flow ==========

    /** 라벨 정의 (점프 대상) */
    FHktStoryBuilder& Label(const FString& Name);

    /** 무조건 점프 */
    FHktStoryBuilder& Jump(const FString& LabelName);

    /** 조건부 점프 */
    FHktStoryBuilder& JumpIf(RegisterIndex Cond, const FString& LabelName);
    FHktStoryBuilder& JumpIfNot(RegisterIndex Cond, const FString& LabelName);

    /** 다음 프레임까지 대기 */
    FHktStoryBuilder& Yield(int32 Frames = 1);

    /** N초 대기 */
    FHktStoryBuilder& WaitSeconds(float Seconds);

    /** 프로그램 종료 */
    FHktStoryBuilder& Halt();

    /** 검증 실패로 프로그램 종료 — EVMStatus::Failed 반환, 에러 로그 출력 */
    FHktStoryBuilder& Fail();

    // ========== Event Wait ==========

    /** 충돌 대기 - 충돌 시 Hit 레지스터에 대상 저장 */
    FHktStoryBuilder& WaitCollision(RegisterIndex WatchEntity = Reg::Spawned);

    /** 애니메이션 종료 대기 (프레임 대기로 대체) */
    FHktStoryBuilder& WaitAnimEnd(RegisterIndex Entity = Reg::Self);

    /** 이동 완료 대기 */
    FHktStoryBuilder& WaitMoveEnd(RegisterIndex Entity = Reg::Self);

    // ========== Data Operations (근본 opcode 래퍼) ==========

    FHktStoryBuilder& LoadConst(RegisterIndex Dst, int32 Value);

    /** SourceEntity 프로퍼티 읽기 → Dst */
    FHktStoryBuilder& LoadStore(RegisterIndex Dst, uint16 PropertyId);

    /** 임의 Entity 프로퍼티 읽기 → Dst */
    FHktStoryBuilder& LoadStoreEntity(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId);

    /** Src → SourceEntity 프로퍼티 쓰기 */
    FHktStoryBuilder& SaveStore(uint16 PropertyId, RegisterIndex Src);

    /** Src → 임의 Entity 프로퍼티 쓰기 */
    FHktStoryBuilder& SaveStoreEntity(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src);

    /** LoadStoreEntity 별칭 */
    FHktStoryBuilder& LoadEntityProperty(RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId)
    { return LoadStoreEntity(Dst, Entity, PropertyId); }

    /** SaveStoreEntity 별칭 */
    FHktStoryBuilder& SaveEntityProperty(RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src)
    { return SaveStoreEntity(Entity, PropertyId, Src); }

    /** 상수 값을 SourceEntity 프로퍼티에 직접 저장 (LoadConst + SaveStore 조합) — Reg::Temp 클로버 */
    FHktStoryBuilder& SaveConst(uint16 PropertyId, int32 Value);

    /** 상수 값을 임의 엔티티 프로퍼티에 직접 저장 (LoadConst + SaveStoreEntity 조합) — Reg::Temp 클로버 */
    FHktStoryBuilder& SaveConstEntity(RegisterIndex Entity, uint16 PropertyId, int32 Value);

    FHktStoryBuilder& Move(RegisterIndex Dst, RegisterIndex Src);

    // ========== Arithmetic ==========

    FHktStoryBuilder& Add(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& Sub(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& Mul(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& Div(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& AddImm(RegisterIndex Dst, RegisterIndex Src, int32 Imm);

    // ========== Comparison ==========

    FHktStoryBuilder& CmpEq(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& CmpNe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& CmpLt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& CmpLe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& CmpGt(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    FHktStoryBuilder& CmpGe(RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);

    // ========== Entity Management ==========

    /** 엔티티 스폰 → Spawned 레지스터에 저장. ClassTag는 영구 태그로 부여됨. */
    FHktStoryBuilder& SpawnEntity(const FGameplayTag& ClassTag);

    /** 엔티티 제거 */
    FHktStoryBuilder& DestroyEntity(RegisterIndex Entity);

    // ========== Position & Movement (조합 연산) ==========

    /** 위치 가져오기: (Dst, Dst+1, Dst+2) = Position */
    FHktStoryBuilder& GetPosition(RegisterIndex DstBase, RegisterIndex Entity);

    /** 위치 설정: Position = (SrcBase, SrcBase+1, SrcBase+2) */
    FHktStoryBuilder& SetPosition(RegisterIndex Entity, RegisterIndex SrcBase);

    /** 목표 위치로 이동 시작 (Force 단위, F=ma) */
    FHktStoryBuilder& MoveToward(RegisterIndex Entity, RegisterIndex TargetPosBase, int32 Force);

    /** 전방으로 이동 (투사체용, Force 단위) */
    FHktStoryBuilder& MoveForward(RegisterIndex Entity, int32 Force);

    /** 이동 중지 */
    FHktStoryBuilder& StopMovement(RegisterIndex Entity);

    /** 거리 계산 (VM opcode — sqrt 필요) */
    FHktStoryBuilder& GetDistance(RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2);

    // ========== Spatial Query ==========

    /** 범위 내 엔티티 검색 시작 */
    FHktStoryBuilder& FindInRadius(RegisterIndex CenterEntity, int32 RadiusCm);

    /** 다음 검색 결과 → Iter, 끝이면 Flag=0 */
    FHktStoryBuilder& NextFound();

    /** ForEach 편의 메서드 (FindInRadius + 루프) */
    FHktStoryBuilder& ForEachInRadius(RegisterIndex CenterEntity, int32 RadiusCm);
    FHktStoryBuilder& EndForEach();

    // ========== Combat (조합 연산) ==========

    /** 데미지 적용 (R7-R9 클로버) */
    FHktStoryBuilder& ApplyDamage(RegisterIndex Target, RegisterIndex Amount);
    FHktStoryBuilder& ApplyDamageConst(RegisterIndex Target, int32 Amount);

    // ========== Tags ==========

    /** 엔티티에 태그 추가 */
    FHktStoryBuilder& AddTag(RegisterIndex Entity, const FGameplayTag& Tag);

    /** 엔티티에서 태그 제거 */
    FHktStoryBuilder& RemoveTag(RegisterIndex Entity, const FGameplayTag& Tag);

    /** 엔티티가 태그를 가지고 있는지 확인 → Dst (1/0) */
    FHktStoryBuilder& HasTag(RegisterIndex Dst, RegisterIndex Entity, const FGameplayTag& Tag);

    // ========== Presentation ==========

    /** 이펙트 적용 (버프/디버프) */
    FHktStoryBuilder& ApplyEffect(RegisterIndex Target, const FGameplayTag& EffectTag);

    /** 이펙트 제거 */
    FHktStoryBuilder& RemoveEffect(RegisterIndex Target, const FGameplayTag& EffectTag);

    /** VFX 재생 (위치) */
    FHktStoryBuilder& PlayVFX(RegisterIndex PosBase, const FGameplayTag& VFXTag);

    /** VFX 재생 (엔티티에 부착) */
    FHktStoryBuilder& PlayVFXAttached(RegisterIndex Entity, const FGameplayTag& VFXTag);

    FHktStoryBuilder& PlaySound(const FGameplayTag& SoundTag);
    FHktStoryBuilder& PlaySoundAtLocation(RegisterIndex PosBase, const FGameplayTag& SoundTag);

    // ========== NPC Spawning ==========

    /** 특정 태그를 가진 엔티티 수 카운트 → Dst */
    FHktStoryBuilder& CountByTag(RegisterIndex Dst, const FGameplayTag& Tag);

    /** 현재 프레임 번호 → Dst */
    FHktStoryBuilder& GetWorldTime(RegisterIndex Dst);

    /** 결정론적 랜덤 [0, ModulusReg) → Dst */
    FHktStoryBuilder& RandomInt(RegisterIndex Dst, RegisterIndex ModulusReg);

    /** 현재 relevancy group에 플레이어 존재 여부 → Dst (1/0) */
    FHktStoryBuilder& HasPlayerInGroup(RegisterIndex Dst);

    // ========== Item System ==========

    /** 특정 엔티티가 소유한 Tag 매칭 엔티티 수 카운트 → Dst */
    FHktStoryBuilder& CountByOwner(RegisterIndex Dst, RegisterIndex OwnerEntity, const FGameplayTag& Tag);

    /** 특정 엔티티가 소유한 Tag 매칭 엔티티 검색 → NextFound()로 순회 */
    FHktStoryBuilder& FindByOwner(RegisterIndex OwnerEntity, const FGameplayTag& Tag);

    /** 현재 Runtime.PlayerUid를 엔티티의 OwnerUid로 설정 */
    FHktStoryBuilder& SetOwnerUid(RegisterIndex Entity);

    /** 엔티티의 OwnerUid를 0으로 초기화 (무주물 전환) */
    FHktStoryBuilder& ClearOwnerUid(RegisterIndex Entity);

    // ========== Stance ==========

    /** Stance 태그 설정 */
    FHktStoryBuilder& SetStance(RegisterIndex Entity, const FGameplayTag& StanceTag);

    // ========== Item Skill ==========

    /** 아이템의 스킬 태그 설정 (GameplayTag → NetIndex로 저장) */
    FHktStoryBuilder& SetItemSkillTag(RegisterIndex Entity, const FGameplayTag& SkillTag);

    // ========== Utility ==========

    FHktStoryBuilder& Log(const FString& Message);

    // ========== Internal Label (Snippet용 고유 라벨 생성) ==========

    /** 고유 내부 라벨 생성 — Snippet 함수에서 라벨 충돌 방지에 사용 */
    FString MakeInternalLabel(const TCHAR* Prefix);

    // ========== Build ==========

    /** 빌드 — 검증 실패 시 nullptr 반환, 실패한 Story는 등록되지 않음 */
    TSharedPtr<FHktVMProgram> Build();

    /** 빌드 + 레지스트리 등록 — 검증 실패 시 등록하지 않음 */
    void BuildAndRegister();

private:
    explicit FHktStoryBuilder(const FGameplayTag& Tag);

    void Emit(FInstruction Inst);
    int32 AddString(const FString& Str);
    int32 AddConstant(int32 Value);
    int32 TagToInt(const FGameplayTag& Tag);
    static void ResolveLabels(FCodeSection& Section, const FGameplayTag& Tag);

    /** 빌드 타임 엔티티 레지스터 초기화 순서 검증 — 실패 시 false */
    bool ValidateEntityFlow();

    /** 빌드 타임 범용 레지스터(R0~R8) 흐름 검증
     *  - Read-before-Write: 초기화 안 된 레지스터 읽기 감지
     *  - Dead Write: 값을 쓰고 읽지 않고 다시 덮어쓰기 감지
     *  Warning 레벨 — 빌드는 중단하지 않지만 잠재 버그 경고 */
    void ValidateRegisterFlow();

private:
    TSharedRef<FHktVMProgram> Program;

    FCodeSection MainSection;
    FCodeSection PreconditionSection;
    FCodeSection* ActiveSection = &MainSection;

    // ForEach 스택 (중첩 지원)
    struct FForEachContext
    {
        FString LoopLabel;
        FString EndLabel;
    };
    TArray<FForEachContext> ForEachStack;
    int32 ForEachCounter = 0;
    int32 InternalLabelCounter = 0;
};

// ============================================================================
// 편의 함수
// ============================================================================

/** 간단한 Story 생성 시작 */
inline FHktStoryBuilder Story(FGameplayTag TagName)
{
    return FHktStoryBuilder::Create(TagName);
}

// ============================================================================
// Public Query API
// ============================================================================

namespace HktStory
{
    /**
     * EventTag + WorldState로 Story 사전조건을 검증한다.
     *
     * 클라이언트: Proxy WorldState로 호출하여 UI 표시/요청 가능 여부 결정.
     * 서버: Story 바이트코드 내부 검증이 권위적 최종 검증 (이 함수는 힌트).
     *
     * Precondition 미등록 Story는 항상 true 반환.
     */
    HKTCORE_API bool ValidateEvent(const FHktWorldState& WorldState, const FHktEvent& Event);
}
