// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktVFXIntent.h"
#include "HktVFXGeneratorConfig.h"
#include "HktVFXNiagaraConfig.h"

/**
 * Claude/GPT 등 LLM API 호출 → Niagara 파티클 설정용 JSON 수신 → FHktVFXNiagaraConfig 파싱.
 * 에디터 전용 (런타임 미로드).
 */
class HKTVFXEDITOR_API FHktVFXLLMClient
{
public:
	FHktVFXLLMClient();

	/** 설정 적용 (Config에서 ToLLMSettings()로 전달) */
	void SetSettings(const FHktLLMSettings& InSettings);

	/** Intent 기반으로 LLM에 요청, 완료 시 콜백으로 FHktVFXNiagaraConfig 전달 (비동기) */
	void RequestNiagaraConfig(const FHktVFXIntent& Intent, FOnHktLLMResponse OnComplete);

private:
	FString BuildSystemPrompt() const;
	FString BuildUserPrompt(const FHktVFXIntent& Intent) const;
	void CallAnthropicAPI(const FString& SystemPrompt, const FString& UserPrompt,
		TFunction<void(bool, const FString&)> OnResponse);
	void CallOpenAIAPI(const FString& SystemPrompt, const FString& UserPrompt,
		TFunction<void(bool, const FString&)> OnResponse);
	bool ExtractJSONFromResponse(const FString& RawResponse, FString& OutJSON);
	bool ParseLLMResponse(const FString& ResponseText, FHktVFXNiagaraConfig& OutConfig);

	FHktLLMSettings Settings;
};
