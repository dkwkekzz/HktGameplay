// Copyright Hkt Studios, Inc. All Rights Reserved.
// Story Operation 레지스트리 — FHktStoryBuilder 메서드를 이름/파라미터/핸들러로 등록

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class FHktStoryBuilder;

// ============================================================================
// 파라미터 타입 & 정의
// ============================================================================

/** Story Operation 파라미터 타입 */
enum class EHktStoryParamType : uint8
{
	Register,      // 레지스터 이름 ("Self", "R0", etc.)
	Int,           // 정수값
	Float,         // 실수값
	String,        // 문자열
	Tag,           // GameplayTag (alias 해결 대상)
	PropertyId,    // PropertyId 이름
};

/** Operation 파라미터 정의 */
struct FHktStoryParamDef
{
	FString Name;                    // JSON 필드명
	EHktStoryParamType Type;         // 파라미터 타입
	bool bOptional = false;          // 선택적 파라미터
	int32 DefaultInt = 0;
	float DefaultFloat = 0.f;
	FString Description;             // 스키마 설명
};

/** 파싱된 Operation 인자값 */
struct HKTCORE_API FHktStoryOpArg
{
	int32 RegIdx = -1;
	int32 IntVal = 0;
	float FloatVal = 0.f;
	FString StrVal;
	FGameplayTag TagVal;
	uint16 PropId = 0xFFFF;
};

// ============================================================================
// Operation 핸들러 & 정의
// ============================================================================

/** Operation 핸들러: Builder + 이름→인자 맵 → Builder 메서드 호출 */
using FHktStoryOpHandler = TFunction<void(FHktStoryBuilder& Builder, const TMap<FString, FHktStoryOpArg>& Args)>;

/** Story Operation 정의 */
struct HKTCORE_API FHktStoryOpDef
{
	FString Name;                         // Operation 이름 (e.g. "AddTag")
	FString Category;                     // 분류 (e.g. "tags", "combat")
	TArray<FHktStoryParamDef> Params;     // 파라미터 정의
	FHktStoryOpHandler Handler;           // Builder 호출 핸들러
};

// ============================================================================
// FHktStoryOpRegistry
// ============================================================================

/**
 * FHktStoryOpRegistry — Story Operation 자기 등록 레지스트리
 *
 * FHktStoryBuilder의 각 메서드를 이름, 파라미터 타입, 핸들러와 함께 등록.
 * HktStoryGenerator(JsonCompiler)는 이 레지스트리를 통해 JSON을 Builder 호출로 변환.
 *
 * === HktStoryGenerator 무변경 보장 ===
 * 새 FHktStoryBuilder 메서드 추가 시:
 * 1. FHktStoryBuilder에 메서드 추가
 * 2. RegisterBuiltinOps()에 등록 추가
 * 3. skill.md 업데이트 (Claude가 새 op을 알 수 있도록)
 * → HktStoryGenerator 코드 변경 불필요
 */
class HKTCORE_API FHktStoryOpRegistry
{
public:
	static FHktStoryOpRegistry& Get();

	/** Operation 등록 */
	void Register(FHktStoryOpDef&& Def);

	/** 이름으로 Operation 검색 */
	const FHktStoryOpDef* Find(const FString& OpName) const;

	/** 전체 Operation 맵 */
	const TMap<FString, FHktStoryOpDef>& GetAll() const { return Ops; }

	/** 유효한 Operation 이름 집합 (Validate용) */
	TSet<FString> GetValidOpNames() const;

	/** JSON 스키마 문자열 생성 (AI Agent 학습용) */
	FString GenerateSchemaJson() const;

	// ========== 유틸리티 (JSON 파싱 공용) ==========

	/** Register 이름 → RegisterIndex 변환. 실패 시 -1 */
	static int32 ParseRegister(const FString& RegStr);

	/** PropertyId 이름 → uint16 변환. 실패 시 0xFFFF */
	static uint16 ParsePropertyId(const FString& PropStr);

private:
	FHktStoryOpRegistry();

	/** 내장 Operation 일괄 등록 */
	void RegisterBuiltinOps();

	TMap<FString, FHktStoryOpDef> Ops;
};
