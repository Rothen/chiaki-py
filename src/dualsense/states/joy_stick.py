from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class JoyStick:
    x: float = 0
    y: float = 0
