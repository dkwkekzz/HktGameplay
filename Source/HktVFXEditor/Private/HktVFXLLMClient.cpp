// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXLLMClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FHktVFXLLMClient::FHktVFXLLMClient()
{
}

void FHktVFXLLMClient::SetSettings(const FHktLLMSettings& InSettings)
{
    Settings = InSettings;
}

// ============================================================================
// 시스템 프롬프트 - LLM에게 Niagara 전문가 역할 부여
// ============================================================================
FString FHktVFXLLMClient::BuildSystemPrompt() const
{
    return TEXT(R"(
You are an expert VFX artist specializing in Unreal Engine 5 Niagara particle systems.
Given a description of a game event, you design a complete Niagara particle system as JSON.

DESIGN PRINCIPLES:
- Layer 2-5 emitters for depth: core effect + secondary detail + ambient atmosphere + debris/sparks
- Use complementary colors within the element's palette
- Consider particle lifecycle: birth (expand/bright) → sustain → death (fade/shrink)
- Higher intensity = more particles, larger sizes, brighter colors, more sub-effects
- Low intensity = fewer, subtler, simpler

RENDERER TYPES:
- "sprite": Standard billboard particles. Best for most effects.
- "ribbon": Connected trail. Best for trails, lightning arcs, energy beams.
- "mesh": 3D mesh particles. Best for debris, chunks, leaves.
- "light": Dynamic light emission. Use sparingly (1 per system max, performance).

BLEND MODES:
- "additive": Glowing, bright effects (fire, energy, magic). Most VFX use this.
- "translucent": Soft, semi-transparent (smoke, dust, fog). Use alpha.
- "opaque": Solid particles (debris, rocks, leaves).

TEXTURE STYLES (for texture generation):
- "soft_circle": Radial gradient, good for glows and base particles
- "smoke": Cloudy, wispy shapes for smoke and fog
- "spark": Small bright streaks for sparks and fireflies
- "ring": Ring/circle outlines for shockwaves and auras
- "debris": Irregular solid shapes for physical fragments
- "rune": Magical symbols/glyphs for arcane effects
- "streak": Motion-blurred streaks for speed lines
- "glow": Intense center bloom for light sources
- "noise": Perlin/cellular noise for distortion and dissolve
- "flipbook_fire": Animated fire sequence (use FlipbookGridSize: 4 for 4x4)
- "flipbook_smoke": Animated smoke sequence (use FlipbookGridSize: 4 for 4x4)

RESPONSE FORMAT - Output ONLY valid JSON matching this exact structure:
{
  "system_name": "string",
  "design_notes": "brief explanation of your design choices",
  "emitters": [
    {
      "name": "PascalCaseEmitterName",
      "purpose": "what this emitter layer does",
      "spawn": {
        "mode": "burst|rate",
        "rate": float,
        "burst_count": int,
        "burst_delay": float
      },
      "init": {
        "lifetime_min": float, "lifetime_max": float,
        "size_min": float, "size_max": float,
        "velocity_min": [x, y, z], "velocity_max": [x, y, z],
        "color": [r, g, b, a],
        "color_variation": [r, g, b, a]
      },
      "update": {
        "gravity": [x, y, z],
        "drag": float,
        "size_curve": [[time, scale], ...],
        "color_curve": [[time, [r,g,b,a]], ...],
        "opacity_curve": [[time, alpha], ...],
        "rotation_rate_min": float, "rotation_rate_max": float,
        "orbit_center": bool, "orbit_radius": float, "orbit_speed": float,
        "attract_to_center": bool, "attraction_strength": float,
        "noise_strength": float, "noise_frequency": float
      },
      "render": {
        "renderer_type": "sprite|ribbon|mesh|light",
        "blend_mode": "additive|translucent|opaque",
        "texture_style": "string",
        "sort_order": int,
        "alignment": "camera_facing|velocity_aligned",
        "sub_uv_frames": int,
        "ribbon_width": float,
        "light_radius": float, "light_intensity": float
      }
    }
  ],
  "texture_requests": [
    {
      "emitter_name": "matching emitter name",
      "prompt": "stable diffusion prompt for this texture",
      "negative_prompt": "what to avoid",
      "texture_type": "diffuse|normal|mask|flipbook",
      "flipbook_grid_size": int
    }
  ],
  "exposed_parameters": ["RadiusScale", "IntensityMult", "DurationScale", "ElementTint"]
}

COORDINATE SYSTEM: Unreal = X-forward, Y-right, Z-up. Units in cm.
Size values are in Unreal units (cm). A human is ~180 units tall.

IMPORTANT:
- Colors use linear color space [0-1] per channel. For bright emissive, use values > 1 (e.g., [5, 2, 0, 1] for bright orange).
- All velocity/gravity values in cm/s.
- Output ONLY the JSON object, no markdown fences, no explanation outside JSON.
)");
}

// ============================================================================
// 유저 프롬프트 - Intent를 자연어로 변환
// ============================================================================
FString FHktVFXLLMClient::BuildUserPrompt(const FHktVFXIntent& Intent) const
{
    return FString::Printf(TEXT("Design a Niagara particle system for:\n\n%s"),
        *Intent.ToNaturalLanguage());
}

// ============================================================================
// 메인 요청 함수
// ============================================================================
void FHktVFXLLMClient::RequestNiagaraConfig(const FHktVFXIntent& Intent, FOnHktLLMResponse OnComplete)
{
    FString SystemPrompt = BuildSystemPrompt();
    FString UserPrompt = BuildUserPrompt(Intent);

    UE_LOG(LogTemp, Log, TEXT("[VFXLLMClient] Requesting config for: %s"), *Intent.GetAssetKey());
    UE_LOG(LogTemp, Verbose, TEXT("[VFXLLMClient] Prompt:\n%s"), *UserPrompt);

    auto OnResponse = [this, OnComplete](bool bSuccess, const FString& Response)
    {
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Error, TEXT("[VFXLLMClient] API call failed"));
            FHktVFXNiagaraConfig EmptyConfig;
            OnComplete.ExecuteIfBound(false, EmptyConfig);
            return;
        }

        FHktVFXNiagaraConfig Config;
        if (ParseLLMResponse(Response, Config))
        {
            UE_LOG(LogTemp, Log, TEXT("[VFXLLMClient] Successfully parsed config: %s with %d emitters"),
                *Config.SystemName, Config.Emitters.Num());
            OnComplete.ExecuteIfBound(true, Config);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[VFXLLMClient] Failed to parse LLM response"));
            OnComplete.ExecuteIfBound(false, Config);
        }
    };

    switch (Settings.Provider)
    {
    case EHktLLMProvider::Anthropic:
        CallAnthropicAPI(SystemPrompt, UserPrompt, OnResponse);
        break;
    case EHktLLMProvider::OpenAI:
        CallOpenAIAPI(SystemPrompt, UserPrompt, OnResponse);
        break;
    }
}

// ============================================================================
// Anthropic (Claude) API 호출
// ============================================================================
void FHktVFXLLMClient::CallAnthropicAPI(const FString& SystemPrompt, const FString& UserPrompt,
    TFunction<void(bool, const FString&)> OnResponse)
{
    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-api-key"), Settings.APIKey);
    Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));

    // JSON 바디 구성
    TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
    Body->SetStringField(TEXT("model"), Settings.Model);
    Body->SetNumberField(TEXT("max_tokens"), Settings.MaxTokens);
    Body->SetNumberField(TEXT("temperature"), Settings.Temperature);

    // System prompt
    Body->SetStringField(TEXT("system"), SystemPrompt);

    // Messages
    TArray<TSharedPtr<FJsonValue>> Messages;
    TSharedPtr<FJsonObject> UserMsg = MakeShareable(new FJsonObject);
    UserMsg->SetStringField(TEXT("role"), TEXT("user"));
    UserMsg->SetStringField(TEXT("content"), UserPrompt);
    Messages.Add(MakeShareable(new FJsonValueObject(UserMsg)));
    Body->SetArrayField(TEXT("messages"), Messages);

    FString BodyString;
    auto Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(BodyString);

    Request->OnProcessRequestComplete().BindLambda(
        [OnResponse](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
        {
            if (!bConnected || !Resp.IsValid())
            {
                OnResponse(false, TEXT("Connection failed"));
                return;
            }

            int32 Code = Resp->GetResponseCode();
            FString ResponseStr = Resp->GetContentAsString();

            if (Code != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("[VFXLLMClient] Anthropic API error %d: %s"),
                    Code, *ResponseStr);
                OnResponse(false, ResponseStr);
                return;
            }

            // content[0].text 추출
            TSharedPtr<FJsonObject> ResponseObj;
            auto Reader = TJsonReaderFactory<>::Create(ResponseStr);
            if (FJsonSerializer::Deserialize(Reader, ResponseObj))
            {
                auto Content = ResponseObj->GetArrayField(TEXT("content"));
                if (Content.Num() > 0)
                {
                    FString Text = Content[0]->AsObject()->GetStringField(TEXT("text"));
                    OnResponse(true, Text);
                    return;
                }
            }
            OnResponse(false, TEXT("Failed to parse Anthropic response"));
        });

    Request->ProcessRequest();
}

// ============================================================================
// OpenAI (GPT) API 호출
// ============================================================================
void FHktVFXLLMClient::CallOpenAIAPI(const FString& SystemPrompt, const FString& UserPrompt,
    TFunction<void(bool, const FString&)> OnResponse)
{
    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://api.openai.com/v1/chat/completions"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Settings.APIKey));

    TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
    Body->SetStringField(TEXT("model"), Settings.Model);
    Body->SetNumberField(TEXT("max_tokens"), Settings.MaxTokens);
    Body->SetNumberField(TEXT("temperature"), Settings.Temperature);

    // response_format으로 JSON 강제
    TSharedPtr<FJsonObject> ResponseFormat = MakeShareable(new FJsonObject);
    ResponseFormat->SetStringField(TEXT("type"), TEXT("json_object"));
    Body->SetObjectField(TEXT("response_format"), ResponseFormat);

    TArray<TSharedPtr<FJsonValue>> Messages;

    TSharedPtr<FJsonObject> SysMsg = MakeShareable(new FJsonObject);
    SysMsg->SetStringField(TEXT("role"), TEXT("system"));
    SysMsg->SetStringField(TEXT("content"), SystemPrompt);
    Messages.Add(MakeShareable(new FJsonValueObject(SysMsg)));

    TSharedPtr<FJsonObject> UserMsg = MakeShareable(new FJsonObject);
    UserMsg->SetStringField(TEXT("role"), TEXT("user"));
    UserMsg->SetStringField(TEXT("content"), UserPrompt);
    Messages.Add(MakeShareable(new FJsonValueObject(UserMsg)));

    Body->SetArrayField(TEXT("messages"), Messages);

    FString BodyString;
    auto Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(BodyString);

    Request->OnProcessRequestComplete().BindLambda(
        [OnResponse](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
        {
            if (!bConnected || !Resp.IsValid())
            {
                OnResponse(false, TEXT("Connection failed"));
                return;
            }

            int32 Code = Resp->GetResponseCode();
            FString ResponseStr = Resp->GetContentAsString();

            if (Code != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("[VFXLLMClient] OpenAI API error %d: %s"),
                    Code, *ResponseStr);
                OnResponse(false, ResponseStr);
                return;
            }

            // choices[0].message.content 추출
            TSharedPtr<FJsonObject> ResponseObj;
            auto Reader = TJsonReaderFactory<>::Create(ResponseStr);
            if (FJsonSerializer::Deserialize(Reader, ResponseObj))
            {
                auto Choices = ResponseObj->GetArrayField(TEXT("choices"));
                if (Choices.Num() > 0)
                {
                    FString Text = Choices[0]->AsObject()
                        ->GetObjectField(TEXT("message"))
                        ->GetStringField(TEXT("content"));
                    OnResponse(true, Text);
                    return;
                }
            }
            OnResponse(false, TEXT("Failed to parse OpenAI response"));
        });

    Request->ProcessRequest();
}

// ============================================================================
// JSON 추출 - LLM 응답에서 순수 JSON 부분만 추출
// ============================================================================
bool FHktVFXLLMClient::ExtractJSONFromResponse(const FString& RawResponse, FString& OutJSON)
{
    OutJSON = RawResponse.TrimStartAndEnd();

    // 마크다운 코드 블럭 제거
    if (OutJSON.StartsWith(TEXT("```")))
    {
        int32 FirstNewline = OutJSON.Find(TEXT("\n"));
        if (FirstNewline != INDEX_NONE)
        {
            OutJSON = OutJSON.Mid(FirstNewline + 1);
        }
        if (OutJSON.EndsWith(TEXT("```")))
        {
            OutJSON = OutJSON.LeftChop(3);
        }
        OutJSON = OutJSON.TrimStartAndEnd();
    }

    // JSON 시작 '{' 찾기
    int32 BraceStart = OutJSON.Find(TEXT("{"));
    if (BraceStart == INDEX_NONE) return false;

    // 매칭되는 마지막 '}' 찾기
    int32 Depth = 0;
    int32 BraceEnd = INDEX_NONE;
    for (int32 i = BraceStart; i < OutJSON.Len(); ++i)
    {
        if (OutJSON[i] == '{') Depth++;
        else if (OutJSON[i] == '}')
        {
            Depth--;
            if (Depth == 0)
            {
                BraceEnd = i;
                break;
            }
        }
    }

    if (BraceEnd == INDEX_NONE) return false;

    OutJSON = OutJSON.Mid(BraceStart, BraceEnd - BraceStart + 1);
    return true;
}


// ============================================================================
// LLM JSON 응답 → FHktVFXNiagaraConfig 파싱
// ============================================================================
bool FHktVFXLLMClient::ParseLLMResponse(const FString& ResponseText, FHktVFXNiagaraConfig& OutConfig)
{
    FString CleanJSON;
    if (!ExtractJSONFromResponse(ResponseText, CleanJSON))
    {
        UE_LOG(LogTemp, Error, TEXT("[VFXLLMClient] Could not extract JSON from response"));
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    auto Reader = TJsonReaderFactory<>::Create(CleanJSON);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[VFXLLMClient] JSON parse failed"));
        return false;
    }

    // 시스템 레벨
    OutConfig.SystemName = Root->GetStringField(TEXT("system_name"));
    OutConfig.DesignNotes = Root->GetStringField(TEXT("design_notes"));

    // Exposed parameters
    if (Root->HasField(TEXT("exposed_parameters")))
    {
        auto Params = Root->GetArrayField(TEXT("exposed_parameters"));
        for (auto& P : Params)
        {
            OutConfig.ExposedParameters.Add(P->AsString());
        }
    }

    // --- 에미터 파싱 ---
    auto EmittersArr = Root->GetArrayField(TEXT("emitters"));
    for (auto& EmitterVal : EmittersArr)
    {
        auto E = EmitterVal->AsObject();
        FHktVFXEmitterConfig EmitterConfig;

        EmitterConfig.Name = E->GetStringField(TEXT("name"));
        EmitterConfig.Purpose = E->GetStringField(TEXT("purpose"));

        // Spawn
        if (E->HasField(TEXT("spawn")))
        {
            auto S = E->GetObjectField(TEXT("spawn"));
            EmitterConfig.Spawn.Mode = S->GetStringField(TEXT("mode"));
            EmitterConfig.Spawn.Rate = S->GetNumberField(TEXT("rate"));
            EmitterConfig.Spawn.BurstCount = S->GetIntegerField(TEXT("burst_count"));
            EmitterConfig.Spawn.BurstDelay = S->GetNumberField(TEXT("burst_delay"));
        }

        // Init
        if (E->HasField(TEXT("init")))
        {
            auto I = E->GetObjectField(TEXT("init"));
            auto& Init = EmitterConfig.Init;
            Init.LifetimeMin = I->GetNumberField(TEXT("lifetime_min"));
            Init.LifetimeMax = I->GetNumberField(TEXT("lifetime_max"));
            Init.SizeMin = I->GetNumberField(TEXT("size_min"));
            Init.SizeMax = I->GetNumberField(TEXT("size_max"));

            // Velocity (배열 [x,y,z])
            auto ParseVec = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key) -> FVector
            {
                if (!Obj->HasField(Key)) return FVector::ZeroVector;
                auto Arr = Obj->GetArrayField(Key);
                if (Arr.Num() >= 3)
                    return FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
                return FVector::ZeroVector;
            };
            Init.VelocityMin = ParseVec(I, TEXT("velocity_min"));
            Init.VelocityMax = ParseVec(I, TEXT("velocity_max"));

            // Color
            auto ParseColor = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key) -> FLinearColor
            {
                if (!Obj->HasField(Key)) return FLinearColor::White;
                auto Arr = Obj->GetArrayField(Key);
                if (Arr.Num() >= 4)
                    return FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                        Arr[2]->AsNumber(), Arr[3]->AsNumber());
                if (Arr.Num() >= 3)
                    return FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                        Arr[2]->AsNumber(), 1.f);
                return FLinearColor::White;
            };
            Init.Color = ParseColor(I, TEXT("color"));
            Init.ColorVariation = ParseColor(I, TEXT("color_variation"));
        }

        // Update
        if (E->HasField(TEXT("update")))
        {
            auto U = E->GetObjectField(TEXT("update"));
            auto& Upd = EmitterConfig.Update;

            // Gravity
            if (U->HasField(TEXT("gravity")))
            {
                auto G = U->GetArrayField(TEXT("gravity"));
                if (G.Num() >= 3)
                    Upd.Gravity = FVector(G[0]->AsNumber(), G[1]->AsNumber(), G[2]->AsNumber());
            }

            Upd.Drag = U->HasField(TEXT("drag")) ? U->GetNumberField(TEXT("drag")) : 0.f;
            Upd.RotationRateMin = U->HasField(TEXT("rotation_rate_min")) ? U->GetNumberField(TEXT("rotation_rate_min")) : 0.f;
            Upd.RotationRateMax = U->HasField(TEXT("rotation_rate_max")) ? U->GetNumberField(TEXT("rotation_rate_max")) : 0.f;

            // Size curve
            if (U->HasField(TEXT("size_curve")))
            {
                for (auto& Point : U->GetArrayField(TEXT("size_curve")))
                {
                    auto Arr = Point->AsArray();
                    if (Arr.Num() >= 2)
                    {
                        FHktVFXCurvePoint CP;
                        CP.Time = Arr[0]->AsNumber();
                        CP.Value = Arr[1]->AsNumber();
                        Upd.SizeCurve.Add(CP);
                    }
                }
            }

            // Color curve
            if (U->HasField(TEXT("color_curve")))
            {
                for (auto& Point : U->GetArrayField(TEXT("color_curve")))
                {
                    auto Arr = Point->AsArray();
                    if (Arr.Num() >= 2)
                    {
                        FHktVFXColorCurvePoint CP;
                        CP.Time = Arr[0]->AsNumber();
                        auto C = Arr[1]->AsArray();
                        if (C.Num() >= 4)
                            CP.Color = FLinearColor(C[0]->AsNumber(), C[1]->AsNumber(),
                                                    C[2]->AsNumber(), C[3]->AsNumber());
                        Upd.ColorCurve.Add(CP);
                    }
                }
            }

            // Opacity curve
            if (U->HasField(TEXT("opacity_curve")))
            {
                for (auto& Point : U->GetArrayField(TEXT("opacity_curve")))
                {
                    auto Arr = Point->AsArray();
                    if (Arr.Num() >= 2)
                    {
                        FHktVFXCurvePoint CP;
                        CP.Time = Arr[0]->AsNumber();
                        CP.Value = Arr[1]->AsNumber();
                        Upd.OpacityCurve.Add(CP);
                    }
                }
            }

            // 추가 행동
            Upd.bOrbitCenter = U->HasField(TEXT("orbit_center")) && U->GetBoolField(TEXT("orbit_center"));
            Upd.OrbitRadius = U->HasField(TEXT("orbit_radius")) ? U->GetNumberField(TEXT("orbit_radius")) : 0.f;
            Upd.OrbitSpeed = U->HasField(TEXT("orbit_speed")) ? U->GetNumberField(TEXT("orbit_speed")) : 0.f;
            Upd.bAttractToCenter = U->HasField(TEXT("attract_to_center")) && U->GetBoolField(TEXT("attract_to_center"));
            Upd.AttractionStrength = U->HasField(TEXT("attraction_strength")) ? U->GetNumberField(TEXT("attraction_strength")) : 0.f;
            Upd.NoiseStrength = U->HasField(TEXT("noise_strength")) ? U->GetNumberField(TEXT("noise_strength")) : 0.f;
            Upd.NoiseFrequency = U->HasField(TEXT("noise_frequency")) ? U->GetNumberField(TEXT("noise_frequency")) : 1.f;
        }

        // Render
        if (E->HasField(TEXT("render")))
        {
            auto R = E->GetObjectField(TEXT("render"));
            auto& Rend = EmitterConfig.Render;
            Rend.RendererType = R->GetStringField(TEXT("renderer_type"));
            Rend.BlendMode = R->GetStringField(TEXT("blend_mode"));
            Rend.TextureStyle = R->HasField(TEXT("texture_style")) ? R->GetStringField(TEXT("texture_style")) : TEXT("soft_circle");
            Rend.SortOrder = R->HasField(TEXT("sort_order")) ? R->GetIntegerField(TEXT("sort_order")) : 0;
            Rend.Alignment = R->HasField(TEXT("alignment")) ? R->GetStringField(TEXT("alignment")) : TEXT("camera_facing");
            Rend.SubUVFrames = R->HasField(TEXT("sub_uv_frames")) ? R->GetIntegerField(TEXT("sub_uv_frames")) : 1;
            Rend.RibbonWidth = R->HasField(TEXT("ribbon_width")) ? R->GetNumberField(TEXT("ribbon_width")) : 10.f;
            Rend.LightRadius = R->HasField(TEXT("light_radius")) ? R->GetNumberField(TEXT("light_radius")) : 200.f;
            Rend.LightIntensity = R->HasField(TEXT("light_intensity")) ? R->GetNumberField(TEXT("light_intensity")) : 5.f;
        }

        OutConfig.Emitters.Add(EmitterConfig);
    }

    // --- 텍스처 요청 파싱 ---
    if (Root->HasField(TEXT("texture_requests")))
    {
        for (auto& TexVal : Root->GetArrayField(TEXT("texture_requests")))
        {
            auto T = TexVal->AsObject();
            FHktVFXTextureRequest TexReq;
            TexReq.EmitterName = T->GetStringField(TEXT("emitter_name"));
            TexReq.Prompt = T->GetStringField(TEXT("prompt"));
            TexReq.NegativePrompt = T->HasField(TEXT("negative_prompt")) ? T->GetStringField(TEXT("negative_prompt")) : TEXT("");
            TexReq.TextureType = T->HasField(TEXT("texture_type")) ? T->GetStringField(TEXT("texture_type")) : TEXT("diffuse");
            TexReq.FlipbookGridSize = T->HasField(TEXT("flipbook_grid_size")) ? T->GetIntegerField(TEXT("flipbook_grid_size")) : 1;
            OutConfig.TextureRequests.Add(TexReq);
        }
    }

    return OutConfig.IsValid();
} 