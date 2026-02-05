#include "HktVMInterpreter.h"
#include "HktVMProgram.h"
#include "HktVMStore.h"
#include "HktCoreInterfaces.h"

void FHktVMInterpreter::Initialize(IHktStashInterface* InStash)
{
    Stash = InStash;
}

EVMStatus FHktVMInterpreter::Execute(FHktVMRuntime& Runtime)
{
    if (!Runtime.Program || !Runtime.Program->IsValid())
        return EVMStatus::Failed;
    
    if (Runtime.Status == EVMStatus::WaitingEvent)
        return EVMStatus::WaitingEvent;
    
    const FHktVMProgram& Program = *Runtime.Program;
    int32 InstructionCount = 0;
    
    while (InstructionCount < MaxInstructionsPerTick)
    {
        if (Runtime.PC < 0 || Runtime.PC >= Program.CodeSize())
            return EVMStatus::Completed;
        
        const FInstruction& Inst = Program.Code[Runtime.PC];
        Runtime.PC++;
        InstructionCount++;
        
        EVMStatus Status = ExecuteInstruction(Runtime, Inst);
        if (Status != EVMStatus::Running)
            return Status;
    }
    
    return EVMStatus::Yielded;
}

EVMStatus FHktVMInterpreter::ExecuteInstruction(FHktVMRuntime& Runtime, const FInstruction& Inst)
{
    switch (Inst.GetOpCode())
    {
    case EOpCode::Nop: break;
    case EOpCode::Halt: return EVMStatus::Completed;
    case EOpCode::Yield: return Op_Yield(Runtime, Inst.Imm12);
    case EOpCode::YieldSeconds: return Op_YieldSeconds(Runtime, Inst.GetSignedImm20());
    case EOpCode::Jump: Op_Jump(Runtime, Inst.Imm20); break;
    case EOpCode::JumpIf: Op_JumpIf(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::JumpIfNot: Op_JumpIfNot(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::WaitCollision: return Op_WaitCollision(Runtime, Inst.Src1);
    case EOpCode::LoadConst: Op_LoadConst(Runtime, Inst._Dst, Inst.GetSignedImm20()); break;
    case EOpCode::LoadConstHigh: Op_LoadConstHigh(Runtime, Inst.Dst, Inst.Imm12); break;
    case EOpCode::LoadStore: Op_LoadStore(Runtime, Inst.Dst, Inst.Imm12); break;
    case EOpCode::LoadStoreEntity: Op_LoadStoreEntity(Runtime, Inst.Dst, Inst.Src1, Inst.Imm12); break;
    case EOpCode::SaveStore: Op_SaveStore(Runtime, Inst.Imm12, Inst.Src1); break;
    case EOpCode::SaveStoreEntity: Op_SaveStoreEntity(Runtime, Inst.Src1, Inst.Imm12, Inst.Src2); break;
    case EOpCode::Move: Op_Move(Runtime, Inst.Dst, Inst.Src1); break;
    case EOpCode::Add: Op_Add(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::Sub: Op_Sub(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::Mul: Op_Mul(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::Div: Op_Div(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::Mod: Op_Mod(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::AddImm: Op_AddImm(Runtime, Inst.Dst, Inst.Src1, Inst.GetSignedImm12()); break;
    case EOpCode::CmpEq: Op_CmpEq(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::CmpNe: Op_CmpNe(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::CmpLt: Op_CmpLt(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::CmpLe: Op_CmpLe(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::CmpGt: Op_CmpGt(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::CmpGe: Op_CmpGe(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::SpawnEntity: Op_SpawnEntity(Runtime, Inst.GetSignedImm20()); break;
    case EOpCode::DestroyEntity: Op_DestroyEntity(Runtime, Inst.Src1); break;
    case EOpCode::GetPosition: Op_GetPosition(Runtime, Inst.Dst, Inst.Src1); break;
    case EOpCode::SetPosition: Op_SetPosition(Runtime, Inst.Dst, Inst.Src1); break;
    case EOpCode::GetDistance: Op_GetDistance(Runtime, Inst.Dst, Inst.Src1, Inst.Src2); break;
    case EOpCode::MoveToward: Op_MoveToward(Runtime, Inst.Dst, Inst.Src1, Inst.Imm12); break;
    case EOpCode::MoveForward: Op_MoveForward(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::StopMovement: Op_StopMovement(Runtime, Inst.Src1); break;
    case EOpCode::FindInRadius: Op_FindInRadius(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::NextFound: Op_NextFound(Runtime); break;
    case EOpCode::ApplyDamage: Op_ApplyDamage(Runtime, Inst.Src1, Inst.Src2); break;
    case EOpCode::ApplyEffect: Op_ApplyEffect(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::RemoveEffect: Op_RemoveEffect(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::PlayAnim: Op_PlayAnim(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::PlayAnimMontage: Op_PlayAnimMontage(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::StopAnim: Op_StopAnim(Runtime, Inst.Src1); break;
    case EOpCode::PlayVFX: Op_PlayVFX(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::PlayVFXAttached: Op_PlayVFXAttached(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::PlaySound: Op_PlaySound(Runtime, Inst.GetSignedImm20()); break;
    case EOpCode::PlaySoundAtLocation: Op_PlaySoundAtLocation(Runtime, Inst.Src1, Inst.Imm12); break;
    case EOpCode::SpawnEquipment: Op_SpawnEquipment(Runtime, Inst.Src1, Inst.Src2, Inst.Imm12); break;
    case EOpCode::Log: Op_Log(Runtime, Inst.GetSignedImm20()); break;
    default: return EVMStatus::Failed;
    }
    return EVMStatus::Running;
}

// Control Flow
void FHktVMInterpreter::Op_Nop(FHktVMRuntime& Runtime) {}
EVMStatus FHktVMInterpreter::Op_Halt(FHktVMRuntime& Runtime) { return EVMStatus::Completed; }
EVMStatus FHktVMInterpreter::Op_Yield(FHktVMRuntime& Runtime, int32 Frames) { Runtime.WaitFrames = FMath::Max(1, Frames); return EVMStatus::Yielded; }
EVMStatus FHktVMInterpreter::Op_YieldSeconds(FHktVMRuntime& Runtime, int32 DeciMillis) { Runtime.EventWait.Type = EWaitEventType::Timer; Runtime.EventWait.RemainingTime = DeciMillis / 100.0f; return EVMStatus::WaitingEvent; }
void FHktVMInterpreter::Op_Jump(FHktVMRuntime& Runtime, int32 Target) { Runtime.PC = Target; }
void FHktVMInterpreter::Op_JumpIf(FHktVMRuntime& Runtime, RegisterIndex Cond, int32 Target) { if (Runtime.GetReg(Cond) != 0) Runtime.PC = Target; }
void FHktVMInterpreter::Op_JumpIfNot(FHktVMRuntime& Runtime, RegisterIndex Cond, int32 Target) { if (Runtime.GetReg(Cond) == 0) Runtime.PC = Target; }

// Event Wait
EVMStatus FHktVMInterpreter::Op_WaitCollision(FHktVMRuntime& Runtime, RegisterIndex WatchEntity) { Runtime.EventWait.Type = EWaitEventType::Collision; Runtime.EventWait.WatchedEntity = Runtime.GetRegEntity(WatchEntity); return EVMStatus::WaitingEvent; }

// Data
void FHktVMInterpreter::Op_LoadConst(FHktVMRuntime& Runtime, RegisterIndex Dst, int32 Value) { Runtime.SetReg(Dst, Value); }
void FHktVMInterpreter::Op_LoadConstHigh(FHktVMRuntime& Runtime, RegisterIndex Dst, int32 HighBits) { Runtime.SetReg(Dst, (Runtime.GetReg(Dst) & 0xFFFFF) | (HighBits << 20)); }
void FHktVMInterpreter::Op_LoadStore(FHktVMRuntime& Runtime, RegisterIndex Dst, uint16 PropertyId) { if (Runtime.Store) Runtime.SetReg(Dst, Runtime.Store->Read(PropertyId)); }
void FHktVMInterpreter::Op_LoadStoreEntity(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Entity, uint16 PropertyId) { if (Stash) Runtime.SetReg(Dst, Stash->GetProperty(Runtime.GetRegEntity(Entity), PropertyId)); }
void FHktVMInterpreter::Op_SaveStore(FHktVMRuntime& Runtime, uint16 PropertyId, RegisterIndex Src) { if (Runtime.Store) Runtime.Store->Write(PropertyId, Runtime.GetReg(Src)); }
void FHktVMInterpreter::Op_SaveStoreEntity(FHktVMRuntime& Runtime, RegisterIndex Entity, uint16 PropertyId, RegisterIndex Src) { if (Runtime.Store) { FHktVMStore::FPendingWrite W; W.Entity = Runtime.GetRegEntity(Entity); W.PropertyId = PropertyId; W.Value = Runtime.GetReg(Src); Runtime.Store->PendingWrites.Add(W); } }
void FHktVMInterpreter::Op_Move(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src) { Runtime.SetReg(Dst, Runtime.GetReg(Src)); }

// Arithmetic
void FHktVMInterpreter::Op_Add(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) + Runtime.GetReg(Src2)); }
void FHktVMInterpreter::Op_Sub(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) - Runtime.GetReg(Src2)); }
void FHktVMInterpreter::Op_Mul(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) * Runtime.GetReg(Src2)); }
void FHktVMInterpreter::Op_Div(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { int32 D = Runtime.GetReg(Src2); Runtime.SetReg(Dst, D != 0 ? Runtime.GetReg(Src1) / D : 0); }
void FHktVMInterpreter::Op_Mod(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { int32 D = Runtime.GetReg(Src2); Runtime.SetReg(Dst, D != 0 ? Runtime.GetReg(Src1) % D : 0); }
void FHktVMInterpreter::Op_AddImm(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src, int32 Imm) { Runtime.SetReg(Dst, Runtime.GetReg(Src) + Imm); }

// Comparison
void FHktVMInterpreter::Op_CmpEq(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) == Runtime.GetReg(Src2) ? 1 : 0); }
void FHktVMInterpreter::Op_CmpNe(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) != Runtime.GetReg(Src2) ? 1 : 0); }
void FHktVMInterpreter::Op_CmpLt(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) < Runtime.GetReg(Src2) ? 1 : 0); }
void FHktVMInterpreter::Op_CmpLe(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) <= Runtime.GetReg(Src2) ? 1 : 0); }
void FHktVMInterpreter::Op_CmpGt(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) > Runtime.GetReg(Src2) ? 1 : 0); }
void FHktVMInterpreter::Op_CmpGe(FHktVMRuntime& Runtime, RegisterIndex Dst, RegisterIndex Src1, RegisterIndex Src2) { Runtime.SetReg(Dst, Runtime.GetReg(Src1) >= Runtime.GetReg(Src2) ? 1 : 0); }