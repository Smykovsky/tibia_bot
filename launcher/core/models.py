from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional

@dataclass
class Position:
    x: int = 0
    y: int = 0
    z: int = 7

@dataclass
class PlayerState:
    id: int = 0
    name: str = "Offline"
    position: Position = field(default_factory=Position)
    hp: int = 0
    max_hp: int = 0
    hp_percent: float = 0.0
    mp: int = 0
    max_mp: int = 0
    mp_percent: float = 0.0
    level: int = 1
    capacity: float = 0.0
    target_id: int = 0

@dataclass
class CreatureInfo:
    id: int = 0
    name: str = ""
    hp_percent: int = 100
    distance: int = 0
    position: Position = field(default_factory=Position)

@dataclass
class ModuleInfo:
    name: str
    description: str
    enabled: bool

@dataclass
class HealRule:
    name: str = "Heal"
    enabled: bool = True
    trigger: str = "hp_below" # "hp_below", "mp_below", "poison"
    threshold_percent: float = 70.0
    action: str = "spell" # "spell", "item"
    spell_words: str = "exura"
    item_id: int = 266
    min_mana: int = 20
    cooldown_ms: int = 1000
    priority: int = 1

@dataclass
class Waypoint:
    type: str = "walk" # "walk", "node", "stand", "rope", "ladder", "shovel", "action"
    x: int = 0
    y: int = 0
    z: int = 7
    label: str = ""
    script: str = ""
    tolerance: int = 1

@dataclass
class MonsterTarget:
    name: str = "*"
    priority: int = 1
    stance: str = "chase" # "chase", "keep_distance", "stand"
    desired_distance: int = 3
    spells: List[str] = field(default_factory=list)

@dataclass
class LootItem:
    id: int = 0
    name: str = ""
    dest_container: int = 0

@dataclass
class AoeRule:
    spell: str = "exori"
    min_monsters: int = 3
    max_distance: int = 1
    min_mana: int = 115
    cooldown_ms: int = 2000
