# Hkt VFX Generator – Unreal Engine 5 Editor Plugin

프로그래머가 **VFX Intent(의미)**만 정의하면, AI가 Niagara 파티클 시스템과 텍스처를 자동 생성하는 에디터 플러그인.

---

## 프로그래머가 하는 것

**FVFXIntent만 채우면 됩니다. 비주얼 작업 제로.**

```cpp
// 이게 전부
Intent.EventType = Explosion;
Intent.Element = Fire;
Intent.Intensity = Damage / MaxDamage;  // 시뮬레이션 결과값
Gen->GenerateVFX(Request);
```

---

## Architecture (개요)

```
[프로그래머] FVFXIntent 정의
      ↓
[UVFXGeneratorSubsystem] 에디터 서브시스템
      ↓
[FVFXLLMClient] → Claude/GPT API → Niagara JSON Config
      ↓
[FVFXTextureGenerator] → Stable Diffusion/ComfyUI API → Texture Assets
      ↓
[FVFXNiagaraBuilder] → JSON + Textures → UNiagaraSystem .uasset
      ↓
[UVFXAssetBank] (HktVFX) → 생성된 에셋 관리 + 런타임 Resolve
```

---

## 파이프라인 흐름 (전부 자동)

| 단계 | 설명 |
|------|------|
| **1. 자연어 변환** | `FVFXIntent` → `ToNaturalLanguage()`로 자연어 변환 |
| **2. LLM** | `VFXLLMClient` → Claude/GPT API에 **Niagara 전문 시스템 프롬프트** + 자연어 전송 → JSON으로 **에미터 구성** 수신 (스폰, 초기화, 업데이트, 렌더러 전부) |
| **3. 텍스처** | `VFXTextureGenerator` → LLM이 지정한 텍스처 프롬프트를 로컬 **Stable Diffusion (AUTOMATIC1111)** 에 전송 → PNG → `UTexture2D` 에셋으로 임포트 |
| **4. Niagara 빌드** | `VFXNiagaraBuilder` → JSON Config + 텍스처 → `UNiagaraSystem` 에셋 자동 빌드 & 디스크 저장 |
| **5. 런타임 Resolve** | `VFXRuntimeResolver` (HktVFX) → 런타임에 시뮬레이션 결과 → **가장 가까운 에셋 매칭** + **파라미터 오버라이드** |

---

## Modules

| 모듈 | 용도 |
|------|------|
| **HktVFX** (Runtime) | Intent 정의, AssetBank, 런타임 Resolver |
| **HktVFXEditor** (Editor) | LLM Client, Texture Generator, Niagara Builder, Editor UI |

## HktVFXEditor 파일 구조

```
HktVFXEditor/
├── HktVFXEditor.Build.cs
├── Public/
│   ├── IHktVFXEditorModule.h
│   ├── VFXGeneratorSubsystem.h    # UVFXGeneratorSubsystem (에디터 서브시스템)
│   ├── VFXLLMClient.h             # FVFXLLMClient → LLM API → Niagara JSON
│   ├── VFXTextureGenerator.h     # FVFXTextureGenerator → SD/ComfyUI → Textures
│   └── VFXNiagaraBuilder.h        # FVFXNiagaraBuilder → JSON + Textures → UNiagaraSystem
├── Private/
│   ├── HktVFXEditorModule.cpp
│   ├── VFXGeneratorSubsystem.cpp
│   ├── VFXLLMClient.cpp
│   ├── VFXTextureGenerator.cpp
│   └── VFXNiagaraBuilder.cpp
└── README.md
```

## 의존성

- **HktVFX**, HktCore, HktRuntime, HktAsset
- UnrealEd, EditorStyle, Niagara, DeveloperSettings, InputCore

---

## 프로덕션 적용 시 보완 포인트

- **마스터 머티리얼**  
  `M_VFX_Additive` / `Translucent` 를 플러그인 Content에 미리 만들어둬야 합니다.

- **NiagaraBuilder**  
  UE 5.6 Niagara 에디터 API에서 `RapidIterationParameters` 접근 방식이 변경됐을 수 있으므로 **버전 확인** 필요.

- **ComfyUI 폴링**  
  현재는 **SD WebUI** 가 더 간단하므로 그쪽 추천. ComfyUI를 쓰려면 WebSocket 폴링 추가가 필요합니다.
