# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Presentation saves shared by the native filesystem and browser IDBFS adapters."""

FIXTURE_NAMES = ("fresh", "max_roster", "rewards", "rescue")


def capture_profile(name: str) -> dict:
    if name == "fresh":
        return {}
    if name == "max_roster":
        return {"total_score": 123456789,
                "levels_completed": [f"level_{i:02}" for i in range(1, 6)],
                "owned_upgrade_counts": {"sharpshooter_contract": 1,
                                         "demolition_permit": 1, "command_uplink": 1}}
    if name == "rewards":
        return {"total_score": 123456789, "levels_completed": [],
                "owned_upgrade_counts": {"battery_pack": 10, "reinforced_plating": 100,
                                         "sharpshooter_contract": 1, "precision_ammo": 1000}}
    if name == "rescue":
        return {"total_score": 620, "levels_completed": ["level_01"],
                "owned_upgrade_counts": {"sharpshooter_contract": 1}}
    raise ValueError(f"Unknown capture fixture: {name}")
