// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/HktTypes.h"

// ============================================================================
// VM 상태
// ============================================================================

enum class EVMStatus : uint8
{
    Ready,          // 실행 대기
    Running,        // 실행 중
    Yielded,        // yield 상태 (다음 틱에 재개)
    WaitingEvent,   // 이벤트 대기 중
    Completed,      // 정상 완료
    Failed,         // 오류로 중단
};

/**
 * EWaitEventType - VM이 대기하는 이벤트 타입
 */
enum class EWaitEventType : uint8
{
    None,
    Timer,
    Collision,
};

/**
 * FHktPendingEvent - 외부에서 주입된 이벤트 (큐에 적재, Execute에서 일괄 처리)
 */
struct FHktPendingEvent
{
    EWaitEventType Type = EWaitEventType::None;
    FHktEntityId WatchedEntity = InvalidEntityId;
    FHktEntityId HitEntity = InvalidEntityId;  // Collision 전용
};

// ============================================================================
// OpCode 정의
// ============================================================================

enum class EOpCode : uint8
{
    // Control Flow
    Nop = 0,
    Halt,                   // 프로그램 종료
    Yield,                  // 다음 틱까지 대기
    YieldSeconds,           // N초 대기
    Jump,                   // 무조건 점프
    JumpIf,                 // 조건부 점프
    JumpIfNot,              // 조건부 점프 (반전)

    // Event Wait
    WaitCollision,          // 충돌 이벤트 대기

    // Data Operations
    LoadConst,              // 상수 → 레지스터
    LoadConstHigh,          // 상수 상위 비트 로드
    LoadStore,              // Store 속성 → 레지스터
    LoadStoreEntity,        // 엔티티 속성 → 레지스터
    SaveStore,              // 레지스터 → Store 속성 (버퍼링됨)
    SaveStoreEntity,        // 레지스터 → 엔티티 속성 (버퍼링됨)
    Move,                   // 레지스터 → 레지스터

    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    AddImm,                 // 즉시값 더하기

    // Comparison (결과를 플래그 레지스터에 저장)
    CmpEq,
    CmpNe,
    CmpLt,
    CmpLe,
    CmpGt,
    CmpGe,

    // Entity Management
    SpawnEntity,            // 엔티티 생성
    DestroyEntity,          // 엔티티 제거

    // Position & Movement
    GetPosition,            // 위치 가져오기
    SetPosition,            // 위치 설정
    GetDistance,             // 거리 계산
    MoveToward,             // 목표로 이동
    MoveForward,            // 전방 이동
    StopMovement,           // 이동 중지

    // Spatial Query
    FindInRadius,           // 반경 내 검색
    NextFound,              // 다음 검색 결과

    // Combat
    ApplyDamage,            // 데미지 적용
    ApplyEffect,            // 이펙트 적용
    RemoveEffect,           // 이펙트 제거

    // Animation & VFX
    PlayAnim,               // 애니메이션 재생
    PlayAnimMontage,        // 몽타주 재생
    StopAnim,               // 애니메이션 중지
    PlayVFX,                // VFX 재생 (위치)
    PlayVFXAttached,        // VFX 재생 (엔티티 부착)

    // Audio
    PlaySound,              // 사운드 재생
    PlaySoundAtLocation,    // 위치에서 사운드 재생

    // Equipment
    SpawnEquipment,         // 장비 생성 및 부착

    // Utility
    Log,                    // 디버그 로그

    Max
};

// ============================================================================
// 명령어 인코딩
// ============================================================================

/**
 * 32비트 명령어 포맷:
 * [OpCode:8][Dst:4][Src1:4][Src2:4][Imm12:12] - 3-operand
 * [OpCode:8][Dst:4][Imm20:20]                 - Load immediate
 */
struct FInstruction
{
    union
    {
        uint32 Raw;
        struct
        {
            uint32 OpCode : 8;
            uint32 Dst : 4;
            uint32 Src1 : 4;
            uint32 Src2 : 4;
            uint32 Imm12 : 12;
        };
        struct
        {
            uint32 _Op : 8;
            uint32 _Dst : 4;
            uint32 Imm20 : 20;
        };
    };

    FInstruction() : Raw(0) {}
    explicit FInstruction(uint32 InRaw) : Raw(InRaw) {}

    EOpCode GetOpCode() const { return static_cast<EOpCode>(OpCode); }

    // 빌더 헬퍼
    static FInstruction Make(EOpCode Op, uint8 Dst = 0, uint8 Src1 = 0, uint8 Src2 = 0, uint16 Imm = 0)
    {
        FInstruction I;
        I.OpCode = static_cast<uint8>(Op);
        I.Dst = Dst;
        I.Src1 = Src1;
        I.Src2 = Src2;
        I.Imm12 = Imm;
        return I;
    }

    static FInstruction MakeImm(EOpCode Op, uint8 Dst, int32 Imm)
    {
        FInstruction I;
        I.OpCode = static_cast<uint8>(Op);
        I._Dst = Dst;
        I.Imm20 = static_cast<uint32>(Imm) & 0xFFFFF;
        return I;
    }

    int32 GetSignedImm12() const
    {
        int32 Val = Imm12;
        if (Val & 0x800)
        {
            Val |= 0xFFFFF000;
        }
        return Val;
    }

    int32 GetSignedImm20() const
    {
        int32 Val = Imm20;
        if (Val & 0x80000)
        {
            Val |= 0xFFF00000;
        }
        return Val;
    }
};

static_assert(sizeof(FInstruction) == 4, "Instruction must be 32 bits");
