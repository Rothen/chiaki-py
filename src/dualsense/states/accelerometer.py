from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class Accelerometer:
    x: int = 0
    y: int = 0
    z: int = 0
