#!/usr/bin/env python3
"""
HktVFXGenerator - Niagara VFX JSON Pipeline Generator

Monolith MCP의 niagara_query 호출 시퀀스를 JSON으로 생성합니다.
생성된 JSON은 MCP 호출 자동화 또는 향후 HktVFXEditor 파이프라인에서 사용됩니다.

Usage:
    python generate_vfx_json.py                          # FireTornado 기본 생성
    python generate_vfx_json.py --output custom.json     # 출력 경로 지정
"""

import json
import argparse
from dataclasses import dataclass, field, asdict
from typing import Optional
from pathlib import Path


# =============================================================================
# Data Types
# =============================================================================

@dataclass
class FHktRange:
    min: float
    max: float


@dataclass
class FHktEmitterSource:
    """사용 가능한 이미터 소스 에셋 경로"""
    NE_Smoke: str = "/Game/NiagaraExamples/FX_Smoke/Emitters/NE_Smoke"
    NE_Sparks: str = "/Game/NiagaraExamples/FX_Sparks/Emitters/NE_Sparks"
    NE_Core: str = "/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Core"


@dataclass
class FHktMaterialPath:
    """사용 가능한 머티리얼 에셋 경로"""
    MI_Flames: str = "/Game/NiagaraExamples/Materials/MI_Flames"
    MI_SmokePuff: str = "/Game/NiagaraExamples/Materials/MI_SmokePuffLight_8x8"
    MI_SmokePuffEmissive: str = "/Game/NiagaraExamples/Materials/MI_SmokePuffLight_8x8_Emissive"
    MI_Sparks: str = "/Game/NiagaraExamples/Materials/MI_Sparks"


@dataclass
class FHktModulePath:
    """사용 가능한 모듈 스크립트 경로"""
    VortexForce: str = "/Niagara/Modules/Update/Forces/VortexForce.VortexForce"
    LinearForce: str = "/Niagara/Modules/Update/Forces/LinearForce.LinearForce"
    AddVelocity: str = "/Niagara/Modules/Update/Velocity/AddVelocity.AddVelocity"


@dataclass
class FHktVFXEmitterConfig:
    """이미터 설정"""
    name: str
    source_asset: str
    material: Optional[str] = None
    renderer_index: int = 0
    modules_to_add: list = field(default_factory=list)
    modules_to_enable: list = field(default_factory=list)
    modules_to_disable: list = field(default_factory=list)
    overrides: dict = field(default_factory=dict)


@dataclass
class FHktVFXIntent:
    """FHktVFXIntent 미러 (Python 측)"""
    event_type: str = "AreaEffect"
    element: str = "Fire"
    intensity: float = 0.5
    radius: float = 200.0
    duration: float = 0.0
    style_keywords: list = field(default_factory=list)


@dataclass
class FHktVFXSystemDef:
    """전체 VFX 시스템 정의"""
    asset_name: str
    save_path: str
    description: str
    intent: FHktVFXIntent
    emitters: list = field(default_factory=list)


# =============================================================================
# Pipeline Builder
# =============================================================================

class FHktVFXPipelineBuilder:
    """Monolith MCP 호출 시퀀스를 JSON으로 변환"""

    def __init__(self, system_def: FHktVFXSystemDef):
        self.system_def = system_def
        self.steps = []
        self.step_counter = 0

    def _add_step(self, action: str, params: dict) -> dict:
        self.step_counter += 1
        step = {"step": self.step_counter, "action": action, "params": params}
        self.steps.append(step)
        return step

    def build(self) -> dict:
        """전체 파이프라인을 JSON dict로 빌드"""
        sd = self.system_def
        asset_path = f"{sd.save_path}/{sd.asset_name}" if not sd.save_path.endswith(sd.asset_name) else sd.save_path

        # Step 1: Create system
        self._add_step("create_system", {
            "save_path": sd.save_path,
            "asset_name": sd.asset_name,
        })

        # Steps: Add emitters
        for em in sd.emitters:
            self._add_step("add_emitter", {
                "emitter_asset": em.source_asset,
                "name": em.name,
            })

        # Steps: Set materials
        for em in sd.emitters:
            if em.material:
                self._add_step("set_renderer_material", {
                    "emitter": em.name,
                    "renderer_index": em.renderer_index,
                    "material": em.material,
                })

        # Steps: Add modules
        for em in sd.emitters:
            for mod in em.modules_to_add:
                self._add_step("add_module", {
                    "emitter": em.name,
                    "usage": mod.get("usage", "Update"),
                    "module_script": mod["script"],
                    "index": mod.get("index", -1),
                })

        # Steps: Enable/disable modules
        for em in sd.emitters:
            for mod_name in em.modules_to_enable:
                self._add_step("set_module_enabled", {
                    "emitter": em.name,
                    "module_node": mod_name,
                    "enabled": True,
                })
            for mod_name in em.modules_to_disable:
                self._add_step("set_module_enabled", {
                    "emitter": em.name,
                    "module_node": mod_name,
                    "enabled": False,
                })

        # Final: Compile
        self._add_step("request_compile", {})

        # Build manual_overrides section
        manual_overrides = {}
        for em in sd.emitters:
            if em.overrides:
                manual_overrides[em.name] = em.overrides

        return {
            "meta": {
                "name": sd.asset_name,
                "save_path": sd.save_path,
                "description": sd.description,
                "intent": asdict(sd.intent),
            },
            "pipeline": self.steps,
            "manual_overrides": manual_overrides,
        }


# =============================================================================
# Preset: FireTornado
# =============================================================================

def create_fire_tornado() -> FHktVFXSystemDef:
    """FireTornado VFX 시스템 정의 생성"""
    modules = FHktModulePath()
    sources = FHktEmitterSource()
    materials = FHktMaterialPath()

    return FHktVFXSystemDef(
        asset_name="NS_FireTornado",
        save_path="/Game/HktGameplay/Generated/FireTornado",
        description="Fire tornado VFX - 4 emitters with vortex forces",
        intent=FHktVFXIntent(
            event_type="AreaEffect",
            element="Fire",
            intensity=0.8,
            radius=500.0,
            duration=0.0,
            style_keywords=["tornado", "vortex", "dramatic"],
        ),
        emitters=[
            FHktVFXEmitterConfig(
                name="FireCore",
                source_asset=sources.NE_Smoke,
                material=materials.MI_Flames,
                modules_to_add=[
                    {"script": modules.VortexForce, "usage": "Update", "index": 4},
                ],
                overrides={
                    "SpawnRate": 60,
                    "InitializeParticle": {
                        "Color": [1.0, 0.4, 0.05, 1.0],
                        "Lifetime": {"min": 1.0, "max": 2.5},
                        "SpriteSize": {"min": 30, "max": 80},
                    },
                    "LinearForce": {"Force": [0, 0, 400]},
                    "VortexForce": {
                        "Amount": 800,
                        "Axis": [0, 0, 1],
                        "OriginPull": 200,
                        "FalloffRadius": 500,
                    },
                    "ShapeLocation": {
                        "Shape": "Cylinder",
                        "Radius": 60,
                        "Height": 20,
                    },
                },
            ),
            FHktVFXEmitterConfig(
                name="Smoke",
                source_asset=sources.NE_Smoke,
                modules_to_add=[
                    {"script": modules.VortexForce, "usage": "Update", "index": 4},
                ],
                overrides={
                    "SpawnRate": 25,
                    "InitializeParticle": {
                        "Color": [0.15, 0.08, 0.03, 0.6],
                        "Lifetime": {"min": 2.0, "max": 4.0},
                        "SpriteSize": {"min": 60, "max": 120},
                    },
                    "LinearForce": {"Force": [0, 0, 200]},
                    "VortexForce": {
                        "Amount": 400,
                        "OriginPull": 100,
                    },
                },
            ),
            FHktVFXEmitterConfig(
                name="Embers",
                source_asset=sources.NE_Sparks,
                modules_to_add=[
                    {"script": modules.VortexForce, "usage": "Update", "index": 1},
                ],
                modules_to_enable=["SpawnRate"],
                modules_to_disable=["SpawnBurst_Instantaneous"],
                overrides={
                    "SpawnRate": 40,
                    "InitializeParticle": {
                        "Color": [5.0, 2.0, 0.2, 1.0],
                        "ColorHDR": True,
                        "Lifetime": {"min": 1.0, "max": 3.0},
                        "SpriteSize": {"min": 2, "max": 8},
                    },
                    "AddVelocity": {"Velocity": [0, 0, 400]},
                    "VortexForce": {
                        "Amount": 600,
                        "OriginPull": 150,
                    },
                },
            ),
            FHktVFXEmitterConfig(
                name="InnerGlow",
                source_asset=sources.NE_Core,
                material=materials.MI_SmokePuffEmissive,
                overrides={
                    "SpawnRate": 15,
                    "InitializeParticle": {
                        "Color": [8.0, 4.0, 0.5, 0.8],
                        "ColorHDR": True,
                        "Lifetime": {"min": 0.5, "max": 1.0},
                        "SpriteSize": {"min": 100, "max": 200},
                    },
                },
            ),
        ],
    )


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="HktVFXGenerator - Niagara VFX JSON Pipeline Generator")
    parser.add_argument("--output", "-o", type=str, default=None,
                        help="Output JSON file path (default: <asset_name>.json in current dir)")
    parser.add_argument("--preset", "-p", type=str, default="fire_tornado",
                        choices=["fire_tornado"],
                        help="VFX preset to generate")
    parser.add_argument("--pretty", action="store_true", default=True,
                        help="Pretty-print JSON output")
    args = parser.parse_args()

    presets = {
        "fire_tornado": create_fire_tornado,
    }

    system_def = presets[args.preset]()
    builder = FHktVFXPipelineBuilder(system_def)
    result = builder.build()

    output_path = args.output or f"{system_def.asset_name}.json"
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"Generated: {output_path}")
    print(f"  System: {system_def.asset_name}")
    print(f"  Emitters: {len(system_def.emitters)}")
    print(f"  Pipeline steps: {len(result['pipeline'])}")
    print(f"  Manual overrides: {len(result['manual_overrides'])} emitters")


if __name__ == "__main__":
    main()
