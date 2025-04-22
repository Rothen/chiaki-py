from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class Battery:
    level_percentage: float = 0.0
    full: bool = False
    charging: bool = False
