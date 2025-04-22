from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class TouchFinger:
    active: bool = False
    id: int = 0
    x: int = 0
    y: int = 0
