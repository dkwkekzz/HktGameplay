#pragma once

#include "CoreMinimal.h"
#include "HktVMTypes.h"
#include "HktVMRuntime.h"

// Forward declarations
class IHktStashInterface;

/**
 * FHktVMInterpreter - 바이트코드 인터프리터 (Pure C++)
 * 
 * 단일 VM을 yield 또는 종료까지 실행합니다.
 * UObject/UWorld 참조 없음 - HktCore의 순수성 유지
 */
class HKTCORE_API FHktVMInterpreter
{
public:
    void Initialize(IHktStashInterface* InStash);
    
    /** VM을 yield/완료/실패까지 실행 */
    EVMStatus Execute(FHktVMRuntime& Runtime);
    
    /** 이벤트 완료 알림 (외부에서 호출) */
    void NotifyCollision(FHktVMRuntime& Runtime, EntityId HitEntity);
    void NotifyAnimEnd(FHktVMRuntime& Runtime);
    void NotifyMoveEnd(FHktVMRuntime& Runtime);
    void UpdateTimer(FHktVMRuntime& Runtime, float DeltaSeconds);

private:
    EVMStatus ExecuteInstruction(FHktVMRuntime& Runtime, const FInstruction& Inst);
    
    // ===== Control Flow =====
    void Op_Nop(FHktVMRuntime& Runtime);
    EVMStatus Op_Halt(FHktVMRuntime& Runtime);
    EVMStatus Op_Yield(FHktVMRuntime& Runtime, int32 Frames);
    EVMStatus Op_YieldSeconds(FHktVMRuntime& Runtime, int32 DeciMillis);
    void Op_Jump(FHktVMRuntime& Runtime, int32 Target);
    void Op_JumpIf(FHktVMRuntime& Runtime, RegisterIndex Cond, int32 Target);
    void Op_JumpIfNot(FHktVMRuntime& Runtime, RegisterIndex Cond, int32 Target);
    
    // ===== Event Wait =====
    EVMStatus Op_WaitCollision(FHktVMRuntime& Runtime, RegisterIndex WatchEntity);
    EVMStatus Op_WaitAnimEnd(FHktVMRuntime& Runtime, RegisterIndex Entity);
    EVMStatus Op_WaitMoveEnd(FHktVMRuntime& Runtime, RegisterIndex Entity);
    
    // ===== Data Operations =====
    void Op_LoadConst(FHktVMRuntime& Runtime, RegisterIndex Dst, int32 Value);
    void Op_LoadConstHigh(FHktVMRuntime& Runtime, RegisterIndex Dst, int32 HighBits);
    void Op_LoadStore(FHktVMRuntime& Runtime, RegisterIndex Dst, uint16 PropertyId);
    void Op_LoadStoreEntity(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId);
    void Op_SaveStore(FHktVMRuntime& Runtime, uint16 PropertyId, RegisterIndex Src);
    void Op_SaveStoreEntity(FHktVMRuntime& Runtime, RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src);
    void Op_Move(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src);
    
    // ===== Arithmetic =====
    void Op_Add(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_Sub(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_Mul(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_Div(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_Mod(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_AddImm(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src, int32 Imm);
    
    // ===== Comparison =====
    void Op_CmpEq(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_CmpNe(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_CmpLt(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_CmpLe(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_CmpGt(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    void Op_CmpGe(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2);
    
    // ===== Entity Management =====
    void Op_SpawnEntity(FHktVMRuntime& Runtime, int32 StringIndex);
    void Op_DestroyEntity(FHktVMRuntime& Runtime, RegisterIndex Entity);
    
    // ===== Position & Movement =====
    void Op_GetPosition(FHktVMRuntime& Runtime, RegisterIndex DstBase, RegisterIndex Entity);
    void Op_SetPosition(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex SrcBase);
    void Op_GetDistance(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity1, RegisterIndex Entity2);
    void Op_MoveToward(FHktVMRuntime& Runtime, RegisterIndex Entity, RegisterIndex TargetBase, int32 Speed);
    void Op_MoveForward(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 Speed);
    void Op_StopMovement(FHktVMRuntime& Runtime, RegisterIndex Entity);
    
    // ===== Spatial Query =====
    void Op_FindInRadius(FHktVMRuntime& Runtime, RegisterIndex CenterEntity, int32 RadiusCm);
    void Op_NextFound(FHktVMRuntime& Runtime);
    
    // ===== Combat =====
    void Op_ApplyDamage(FHktVMRuntime& Runtime, RegisterIndex Target, RegisterIndex Amount);
    void Op_ApplyEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex);
    void Op_RemoveEffect(FHktVMRuntime& Runtime, RegisterIndex Target, int32 StringIndex);
    
    // ===== Animation & VFX =====
    void Op_PlayAnim(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex);
    void Op_PlayAnimMontage(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex);
    void Op_StopAnim(FHktVMRuntime& Runtime, RegisterIndex Entity);
    void Op_PlayVFX(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex);
    void Op_PlayVFXAttached(FHktVMRuntime& Runtime, RegisterIndex Entity, int32 StringIndex);
    
    // ===== Audio =====
    void Op_PlaySound(FHktVMRuntime& Runtime, int32 StringIndex);
    void Op_PlaySoundAtLocation(FHktVMRuntime& Runtime, RegisterIndex PosBase, int32 StringIndex);
    
    // ===== Equipment =====
    void Op_SpawnEquipment(FHktVMRuntime& Runtime, RegisterIndex Owner, int32 Slot, int32 StringIndex);
    
    // ===== Utility =====
    void Op_Log(FHktVMRuntime& Runtime, int32 StringIndex);
    
    // ===== Helper =====
    const FString& GetString(FHktVMRuntime& Runtime, int32 Index);

private:
    static constexpr int32 MaxInstructionsPerTick = 10000;
    
    IHktStashInterface* Stash = nullptr;
};