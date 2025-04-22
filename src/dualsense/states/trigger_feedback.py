from __future__ import annotations
from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class TriggerFeedback:
    active: bool = False
    value: int = 0
