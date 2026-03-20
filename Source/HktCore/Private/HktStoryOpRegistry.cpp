// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryOpRegistry.h"
#include "HktStoryBuilder.h"
#include "HktStoryTypes.h"
#include "HktCoreProperties.h"

// ============================================================================
// 인자 접근 헬퍼 매크로
// ============================================================================

#define ARG_REG(Name) Args.FindChecked(TEXT(Name)).RegIdx
#define ARG_INT(Name) Args.FindChecked(TEXT(Name)).IntVal
#define ARG_FLOAT(Name) Args.FindChecked(TEXT(Name)).FloatVal
#define ARG_STR(Name) Args.FindChecked(TEXT(Name)).StrVal
#define ARG_TAG(Name) Args.FindChecked(TEXT(Name)).TagVal
#define ARG_PROP(Name) Args.FindChecked(TEXT(Name)).PropId

// 파라미터 정의 헬퍼
static FHktStoryParamDef PReg(const FString& Name, const FString& Desc = TEXT(""))
{
	return { Name, EHktStoryParamType::Register, false, 0, 0.f, Desc };
}

static FHktStoryParamDef PInt(const FString& Name, const FString& Desc = TEXT(""), bool bOptional = false, int32 Default = 0)
{
	return { Name, EHktStoryParamType::Int, bOptional, Default, 0.f, Desc };
}

static FHktStoryParamDef PFloat(const FString& Name, const FString& Desc = TEXT(""), bool bOptional = false, float Default = 0.f)
{
	return { Name, EHktStoryParamType::Float, bOptional, 0, Default, Desc };
}

static FHktStoryParamDef PStr(const FString& Name, const FString& Desc = TEXT(""))
{
	return { Name, EHktStoryParamType::String, false, 0, 0.f, Desc };
}

static FHktStoryParamDef PTag(const FString& Name, const FString& Desc = TEXT(""))
{
	return { Name, EHktStoryParamType::Tag, false, 0, 0.f, Desc };
}

static FHktStoryParamDef PProp(const FString& Name, const FString& Desc = TEXT(""))
{
	return { Name, EHktStoryParamType::PropertyId, false, 0, 0.f, Desc };
}

// ============================================================================
// Singleton
// ============================================================================

FHktStoryOpRegistry& FHktStoryOpRegistry::Get()
{
	static FHktStoryOpRegistry Instance;
	return Instance;
}

FHktStoryOpRegistry::FHktStoryOpRegistry()
{
	RegisterBuiltinOps();
}

// ============================================================================
// 등록 / 검색
// ============================================================================

void FHktStoryOpRegistry::Register(FHktStoryOpDef&& Def)
{
	Ops.Add(Def.Name, MoveTemp(Def));
}

const FHktStoryOpDef* FHktStoryOpRegistry::Find(const FString& OpName) const
{
	return Ops.Find(OpName);
}

TSet<FString> FHktStoryOpRegistry::GetValidOpNames() const
{
	TSet<FString> Names;
	for (const auto& Pair : Ops)
	{
		Names.Add(Pair.Key);
	}
	return Names;
}

// ============================================================================
// ParseRegister / ParsePropertyId
// ============================================================================

int32 FHktStoryOpRegistry::ParseRegister(const FString& RegStr)
{
	if (RegStr == TEXT("Self")) return Reg::Self;
	if (RegStr == TEXT("Target")) return Reg::Target;
	if (RegStr == TEXT("Spawned")) return Reg::Spawned;
	if (RegStr == TEXT("Hit")) return Reg::Hit;
	if (RegStr == TEXT("Iter")) return Reg::Iter;
	if (RegStr == TEXT("Flag")) return Reg::Flag;
	if (RegStr == TEXT("Count")) return Reg::Count;
	if (RegStr == TEXT("Temp")) return Reg::Temp;

	// R0-R9
	if (RegStr.StartsWith(TEXT("R")) && RegStr.Len() <= 3)
	{
		int32 Idx = FCString::Atoi(*RegStr.Mid(1));
		if (Idx >= 0 && Idx <= 9) return Idx;
	}

	return -1;
}

uint16 FHktStoryOpRegistry::ParsePropertyId(const FString& PropStr)
{
	#define HKT_PROP_PARSE(Name) if (PropStr == TEXT(#Name)) return PropertyId::Name;
	HKT_PROPERTY_LIST(HKT_PROP_PARSE)
	#undef HKT_PROP_PARSE
	return 0xFFFF;
}

// ============================================================================
// 내장 Operation 등록
// ============================================================================

void FHktStoryOpRegistry::RegisterBuiltinOps()
{
	// ======================== Control Flow ========================

	Register({
		TEXT("Label"), TEXT("control"),
		{ PStr(TEXT("name"), TEXT("라벨 이름")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Label(ARG_STR("name")); }
	});

	Register({
		TEXT("Jump"), TEXT("control"),
		{ PStr(TEXT("label"), TEXT("점프 대상 라벨")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Jump(ARG_STR("label")); }
	});

	Register({
		TEXT("JumpIf"), TEXT("control"),
		{ PReg(TEXT("cond"), TEXT("조건 레지스터")), PStr(TEXT("label"), TEXT("점프 대상")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.JumpIf(ARG_REG("cond"), ARG_STR("label")); }
	});

	Register({
		TEXT("JumpIfNot"), TEXT("control"),
		{ PReg(TEXT("cond"), TEXT("조건 레지스터")), PStr(TEXT("label"), TEXT("점프 대상")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.JumpIfNot(ARG_REG("cond"), ARG_STR("label")); }
	});

	Register({
		TEXT("Yield"), TEXT("control"),
		{ PInt(TEXT("frames"), TEXT("대기 프레임 수 (기본 1)"), true, 1) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Yield(ARG_INT("frames")); }
	});

	Register({
		TEXT("WaitSeconds"), TEXT("control"),
		{ PFloat(TEXT("seconds"), TEXT("대기 초")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.WaitSeconds(ARG_FLOAT("seconds")); }
	});

	Register({
		TEXT("Halt"), TEXT("control"),
		{},
		[](FHktStoryBuilder& B, const auto& Args) { B.Halt(); }
	});

	Register({
		TEXT("Fail"), TEXT("control"),
		{},
		[](FHktStoryBuilder& B, const auto& Args) { B.Fail(); }
	});

	// ======================== Event Wait ========================

	Register({
		TEXT("WaitCollision"), TEXT("wait"),
		{ PReg(TEXT("entity"), TEXT("감시 대상 엔티티")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.WaitCollision(ARG_REG("entity")); }
	});

	Register({
		TEXT("WaitAnimEnd"), TEXT("wait"),
		{ PReg(TEXT("entity"), TEXT("대상 엔티티")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.WaitAnimEnd(ARG_REG("entity")); }
	});

	Register({
		TEXT("WaitMoveEnd"), TEXT("wait"),
		{ PReg(TEXT("entity"), TEXT("대상 엔티티")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.WaitMoveEnd(ARG_REG("entity")); }
	});

	// ======================== Data Operations ========================

	Register({
		TEXT("LoadConst"), TEXT("data"),
		{ PReg(TEXT("dst"), TEXT("대상 레지스터")), PInt(TEXT("value"), TEXT("상수값")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.LoadConst(ARG_REG("dst"), ARG_INT("value")); }
	});

	Register({
		TEXT("LoadStore"), TEXT("data"),
		{ PReg(TEXT("dst"), TEXT("대상 레지스터")), PProp(TEXT("property"), TEXT("PropertyId 이름")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.LoadStore(ARG_REG("dst"), ARG_PROP("property")); }
	});

	Register({
		TEXT("LoadEntityProperty"), TEXT("data"),
		{ PReg(TEXT("dst"), TEXT("대상 레지스터")), PReg(TEXT("entity"), TEXT("엔티티")), PProp(TEXT("property"), TEXT("PropertyId 이름")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.LoadEntityProperty(ARG_REG("dst"), ARG_REG("entity"), ARG_PROP("property")); }
	});

	Register({
		TEXT("SaveStore"), TEXT("data"),
		{ PProp(TEXT("property"), TEXT("PropertyId 이름")), PReg(TEXT("src"), TEXT("소스 레지스터")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SaveStore(ARG_PROP("property"), ARG_REG("src")); }
	});

	Register({
		TEXT("SaveEntityProperty"), TEXT("data"),
		{ PReg(TEXT("entity"), TEXT("엔티티")), PProp(TEXT("property"), TEXT("PropertyId 이름")), PReg(TEXT("src"), TEXT("소스 레지스터")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SaveEntityProperty(ARG_REG("entity"), ARG_PROP("property"), ARG_REG("src")); }
	});

	Register({
		TEXT("SaveConst"), TEXT("data"),
		{ PProp(TEXT("property"), TEXT("PropertyId 이름")), PInt(TEXT("value"), TEXT("상수값")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SaveConst(ARG_PROP("property"), ARG_INT("value")); }
	});

	Register({
		TEXT("SaveConstEntity"), TEXT("data"),
		{ PReg(TEXT("entity"), TEXT("엔티티")), PProp(TEXT("property"), TEXT("PropertyId 이름")), PInt(TEXT("value"), TEXT("상수값")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SaveConstEntity(ARG_REG("entity"), ARG_PROP("property"), ARG_INT("value")); }
	});

	Register({
		TEXT("Move"), TEXT("data"),
		{ PReg(TEXT("dst"), TEXT("대상 레지스터")), PReg(TEXT("src"), TEXT("소스 레지스터")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Move(ARG_REG("dst"), ARG_REG("src")); }
	});

	// ======================== Arithmetic ========================

	Register({
		TEXT("Add"), TEXT("arithmetic"),
		{ PReg(TEXT("dst")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Add(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("Sub"), TEXT("arithmetic"),
		{ PReg(TEXT("dst")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Sub(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("Mul"), TEXT("arithmetic"),
		{ PReg(TEXT("dst")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Mul(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("Div"), TEXT("arithmetic"),
		{ PReg(TEXT("dst")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Div(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("AddImm"), TEXT("arithmetic"),
		{ PReg(TEXT("dst")), PReg(TEXT("src")), PInt(TEXT("imm"), TEXT("즉시값")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.AddImm(ARG_REG("dst"), ARG_REG("src"), ARG_INT("imm")); }
	});

	// ======================== Comparison ========================

	Register({
		TEXT("CmpEq"), TEXT("comparison"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CmpEq(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("CmpNe"), TEXT("comparison"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CmpNe(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("CmpLt"), TEXT("comparison"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CmpLt(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("CmpLe"), TEXT("comparison"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CmpLe(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("CmpGt"), TEXT("comparison"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CmpGt(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	Register({
		TEXT("CmpGe"), TEXT("comparison"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("src1")), PReg(TEXT("src2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CmpGe(ARG_REG("dst"), ARG_REG("src1"), ARG_REG("src2")); }
	});

	// ======================== Entity ========================

	Register({
		TEXT("SpawnEntity"), TEXT("entity"),
		{ PTag(TEXT("classTag"), TEXT("Entity 클래스 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SpawnEntity(ARG_TAG("classTag")); }
	});

	Register({
		TEXT("DestroyEntity"), TEXT("entity"),
		{ PReg(TEXT("entity"), TEXT("제거 대상")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.DestroyEntity(ARG_REG("entity")); }
	});

	// ======================== Position & Movement ========================

	Register({
		TEXT("GetPosition"), TEXT("position"),
		{ PReg(TEXT("dst"), TEXT("3연속 레지스터 (X,Y,Z)")), PReg(TEXT("entity")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.GetPosition(ARG_REG("dst"), ARG_REG("entity")); }
	});

	Register({
		TEXT("SetPosition"), TEXT("position"),
		{ PReg(TEXT("entity")), PReg(TEXT("src"), TEXT("3연속 레지스터 (X,Y,Z)")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SetPosition(ARG_REG("entity"), ARG_REG("src")); }
	});

	Register({
		TEXT("MoveToward"), TEXT("position"),
		{ PReg(TEXT("entity")), PReg(TEXT("targetPos"), TEXT("3연속 레지스터")), PInt(TEXT("force"), TEXT("이동 힘")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.MoveToward(ARG_REG("entity"), ARG_REG("targetPos"), ARG_INT("force")); }
	});

	Register({
		TEXT("MoveForward"), TEXT("position"),
		{ PReg(TEXT("entity")), PInt(TEXT("force"), TEXT("이동 힘 (cm/s)")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.MoveForward(ARG_REG("entity"), ARG_INT("force")); }
	});

	Register({
		TEXT("StopMovement"), TEXT("position"),
		{ PReg(TEXT("entity")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.StopMovement(ARG_REG("entity")); }
	});

	Register({
		TEXT("GetDistance"), TEXT("position"),
		{ PReg(TEXT("dst"), TEXT("거리 결과 (cm)")), PReg(TEXT("entity1")), PReg(TEXT("entity2")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.GetDistance(ARG_REG("dst"), ARG_REG("entity1"), ARG_REG("entity2")); }
	});

	// ======================== Spatial Query ========================

	Register({
		TEXT("FindInRadius"), TEXT("spatial"),
		{ PReg(TEXT("center"), TEXT("중심 엔티티")), PInt(TEXT("radius"), TEXT("반경 (cm)")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.FindInRadius(ARG_REG("center"), ARG_INT("radius")); }
	});

	Register({
		TEXT("NextFound"), TEXT("spatial"),
		{},
		[](FHktStoryBuilder& B, const auto& Args) { B.NextFound(); }
	});

	Register({
		TEXT("ForEachInRadius"), TEXT("spatial"),
		{ PReg(TEXT("center"), TEXT("중심 엔티티")), PInt(TEXT("radius"), TEXT("반경 (cm)")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.ForEachInRadius(ARG_REG("center"), ARG_INT("radius")); }
	});

	Register({
		TEXT("EndForEach"), TEXT("spatial"),
		{},
		[](FHktStoryBuilder& B, const auto& Args) { B.EndForEach(); }
	});

	// ======================== Combat ========================

	Register({
		TEXT("ApplyDamage"), TEXT("combat"),
		{ PReg(TEXT("target")), PReg(TEXT("amount"), TEXT("피해량 레지스터")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.ApplyDamage(ARG_REG("target"), ARG_REG("amount")); }
	});

	Register({
		TEXT("ApplyDamageConst"), TEXT("combat"),
		{ PReg(TEXT("target")), PInt(TEXT("amount"), TEXT("피해량 상수")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.ApplyDamageConst(ARG_REG("target"), ARG_INT("amount")); }
	});

	Register({
		TEXT("ApplyEffect"), TEXT("combat"),
		{ PReg(TEXT("target")), PTag(TEXT("effectTag"), TEXT("이펙트 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.ApplyEffect(ARG_REG("target"), ARG_TAG("effectTag")); }
	});

	Register({
		TEXT("RemoveEffect"), TEXT("combat"),
		{ PReg(TEXT("target")), PTag(TEXT("effectTag"), TEXT("이펙트 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.RemoveEffect(ARG_REG("target"), ARG_TAG("effectTag")); }
	});

	// ======================== VFX ========================

	Register({
		TEXT("PlayVFX"), TEXT("vfx"),
		{ PReg(TEXT("pos"), TEXT("3연속 레지스터 (X,Y,Z)")), PTag(TEXT("tag"), TEXT("VFX 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.PlayVFX(ARG_REG("pos"), ARG_TAG("tag")); }
	});

	Register({
		TEXT("PlayVFXAttached"), TEXT("vfx"),
		{ PReg(TEXT("entity")), PTag(TEXT("tag"), TEXT("VFX 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.PlayVFXAttached(ARG_REG("entity"), ARG_TAG("tag")); }
	});

	// ======================== Audio ========================

	Register({
		TEXT("PlaySound"), TEXT("audio"),
		{ PTag(TEXT("tag"), TEXT("사운드 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.PlaySound(ARG_TAG("tag")); }
	});

	Register({
		TEXT("PlaySoundAtLocation"), TEXT("audio"),
		{ PReg(TEXT("pos"), TEXT("3연속 레지스터")), PTag(TEXT("tag"), TEXT("사운드 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.PlaySoundAtLocation(ARG_REG("pos"), ARG_TAG("tag")); }
	});

	// ======================== Tags ========================

	Register({
		TEXT("AddTag"), TEXT("tags"),
		{ PReg(TEXT("entity")), PTag(TEXT("tag")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.AddTag(ARG_REG("entity"), ARG_TAG("tag")); }
	});

	Register({
		TEXT("RemoveTag"), TEXT("tags"),
		{ PReg(TEXT("entity")), PTag(TEXT("tag")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.RemoveTag(ARG_REG("entity"), ARG_TAG("tag")); }
	});

	Register({
		TEXT("HasTag"), TEXT("tags"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")), PReg(TEXT("entity")), PTag(TEXT("tag")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.HasTag(ARG_REG("dst"), ARG_REG("entity"), ARG_TAG("tag")); }
	});

	Register({
		TEXT("CountByTag"), TEXT("tags"),
		{ PReg(TEXT("dst"), TEXT("카운트 결과")), PTag(TEXT("tag")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CountByTag(ARG_REG("dst"), ARG_TAG("tag")); }
	});

	// ======================== World Query ========================

	Register({
		TEXT("GetWorldTime"), TEXT("query"),
		{ PReg(TEXT("dst"), TEXT("현재 프레임 번호")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.GetWorldTime(ARG_REG("dst")); }
	});

	Register({
		TEXT("RandomInt"), TEXT("query"),
		{ PReg(TEXT("dst")), PReg(TEXT("modulus"), TEXT("범위 레지스터")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.RandomInt(ARG_REG("dst"), ARG_REG("modulus")); }
	});

	Register({
		TEXT("HasPlayerInGroup"), TEXT("query"),
		{ PReg(TEXT("dst"), TEXT("결과 (1/0)")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.HasPlayerInGroup(ARG_REG("dst")); }
	});

	// ======================== Item System ========================

	Register({
		TEXT("CountByOwner"), TEXT("item"),
		{ PReg(TEXT("dst")), PReg(TEXT("owner"), TEXT("소유자 엔티티")), PTag(TEXT("tag")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.CountByOwner(ARG_REG("dst"), ARG_REG("owner"), ARG_TAG("tag")); }
	});

	Register({
		TEXT("FindByOwner"), TEXT("item"),
		{ PReg(TEXT("owner"), TEXT("소유자 엔티티")), PTag(TEXT("tag")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.FindByOwner(ARG_REG("owner"), ARG_TAG("tag")); }
	});

	Register({
		TEXT("SetOwnerUid"), TEXT("item"),
		{ PReg(TEXT("entity")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SetOwnerUid(ARG_REG("entity")); }
	});

	Register({
		TEXT("ClearOwnerUid"), TEXT("item"),
		{ PReg(TEXT("entity")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.ClearOwnerUid(ARG_REG("entity")); }
	});

	// ======================== Stance ========================

	Register({
		TEXT("SetStance"), TEXT("stance"),
		{ PReg(TEXT("entity")), PTag(TEXT("stanceTag"), TEXT("스탠스 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SetStance(ARG_REG("entity"), ARG_TAG("stanceTag")); }
	});

	Register({
		TEXT("SetItemSkillTag"), TEXT("stance"),
		{ PReg(TEXT("entity")), PTag(TEXT("skillTag"), TEXT("스킬 태그")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.SetItemSkillTag(ARG_REG("entity"), ARG_TAG("skillTag")); }
	});

	// ======================== Utility ========================

	Register({
		TEXT("Log"), TEXT("utility"),
		{ PStr(TEXT("message"), TEXT("로그 메시지")) },
		[](FHktStoryBuilder& B, const auto& Args) { B.Log(ARG_STR("message")); }
	});
}

#undef ARG_REG
#undef ARG_INT
#undef ARG_FLOAT
#undef ARG_STR
#undef ARG_TAG
#undef ARG_PROP

// ============================================================================
// 스키마 JSON 생성
// ============================================================================

static const TCHAR* ParamTypeToString(EHktStoryParamType Type)
{
	switch (Type)
	{
	case EHktStoryParamType::Register:   return TEXT("register");
	case EHktStoryParamType::Int:        return TEXT("int");
	case EHktStoryParamType::Float:      return TEXT("float");
	case EHktStoryParamType::String:     return TEXT("string");
	case EHktStoryParamType::Tag:        return TEXT("tag");
	case EHktStoryParamType::PropertyId: return TEXT("propertyId");
	default: return TEXT("unknown");
	}
}

FString FHktStoryOpRegistry::GenerateSchemaJson() const
{
	FString Result;
	Result += TEXT("{\n");
	Result += TEXT("  \"description\": \"HktStory JSON Format — AI Agent가 게임 Story/Flow를 정의하는 형식 (FHktStoryOpRegistry에서 자동 생성)\",\n");

	// Format
	Result += TEXT("  \"format\": {\n");
	Result += TEXT("    \"storyTag\": \"GameplayTag (이벤트 이름, e.g. 'Ability.Skill.IceBlast')\",\n");
	Result += TEXT("    \"description\": \"Story 설명 (선택)\",\n");
	Result += TEXT("    \"cancelOnDuplicate\": \"bool — 같은 엔티티에 동일 이벤트 중복 시 기존 취소 (선택, 기본 false)\",\n");
	Result += TEXT("    \"tags\": { \"alias\": \"Full.GameplayTag.Name (단축 이름 → 전체 태그 매핑)\" },\n");
	Result += TEXT("    \"steps\": [ { \"op\": \"OperationName\", \"...params\": \"...\" } ]\n");
	Result += TEXT("  },\n");

	// Registers
	Result += TEXT("  \"registers\": {\n");
	Result += TEXT("    \"R0-R9\": \"범용 레지스터 (R9=Temp)\",\n");
	Result += TEXT("    \"Self\": \"이벤트 소스 엔티티 (R10)\",\n");
	Result += TEXT("    \"Target\": \"이벤트 타겟 엔티티 (R11)\",\n");
	Result += TEXT("    \"Spawned\": \"마지막 스폰된 엔티티 (R12)\",\n");
	Result += TEXT("    \"Hit\": \"충돌 대상 (R13)\",\n");
	Result += TEXT("    \"Iter\": \"ForEach 순회 대상 (R14)\",\n");
	Result += TEXT("    \"Flag\": \"조건 결과 / Count (R15)\"\n");
	Result += TEXT("  },\n");

	// Operations — 카테고리별 그룹화
	TMap<FString, TArray<const FHktStoryOpDef*>> ByCategory;
	for (const auto& Pair : Ops)
	{
		ByCategory.FindOrAdd(Pair.Value.Category).Add(&Pair.Value);
	}

	Result += TEXT("  \"operations\": {\n");
	int32 CatIdx = 0;
	for (const auto& Cat : ByCategory)
	{
		Result += FString::Printf(TEXT("    \"%s\": {\n"), *Cat.Key);

		for (int32 i = 0; i < Cat.Value.Num(); ++i)
		{
			const FHktStoryOpDef* Op = Cat.Value[i];
			Result += FString::Printf(TEXT("      \"%s\": { "), *Op->Name);

			for (int32 j = 0; j < Op->Params.Num(); ++j)
			{
				const FHktStoryParamDef& P = Op->Params[j];
				FString Desc = P.Description.IsEmpty()
					? FString(ParamTypeToString(P.Type))
					: FString::Printf(TEXT("%s — %s"), ParamTypeToString(P.Type), *P.Description);
				if (P.bOptional)
				{
					Desc += FString::Printf(TEXT(" (선택, 기본=%d)"), P.DefaultInt);
				}
				Result += FString::Printf(TEXT("\"%s\": \"%s\""), *P.Name, *Desc);
				if (j + 1 < Op->Params.Num()) Result += TEXT(", ");
			}

			Result += TEXT(" }");
			if (i + 1 < Cat.Value.Num()) Result += TEXT(",");
			Result += TEXT("\n");
		}

		Result += TEXT("    }");
		if (++CatIdx < ByCategory.Num()) Result += TEXT(",");
		Result += TEXT("\n");
	}
	Result += TEXT("  },\n");

	// PropertyIds
	Result += TEXT("  \"propertyIds\": [\n    ");
	{
		TArray<FString> Props;
		#define HKT_PROP_ADD(Name) Props.Add(TEXT("\"" #Name "\""));
		HKT_PROPERTY_LIST(HKT_PROP_ADD)
		#undef HKT_PROP_ADD
		Result += FString::Join(Props, TEXT(", "));
	}
	Result += TEXT("\n  ],\n");

	// Tag prefixes
	Result += TEXT("  \"tagPrefixes\": {\n");
	Result += TEXT("    \"VFX.*\": \"VFXGenerator에서 자동 생성 가능\",\n");
	Result += TEXT("    \"Entity.*\": \"MeshGenerator에서 자동 생성 가능\",\n");
	Result += TEXT("    \"Anim.*\": \"AnimGenerator에서 자동 생성 가능\",\n");
	Result += TEXT("    \"Entity.Item.*\": \"ItemGenerator에서 자동 생성 가능\",\n");
	Result += TEXT("    \"Sound.*\": \"사운드 에셋 (수동 등록 필요)\",\n");
	Result += TEXT("    \"Effect.*\": \"게임플레이 이펙트 (수동 등록 필요)\"\n");
	Result += TEXT("  }\n");

	Result += TEXT("}");
	return Result;
}
