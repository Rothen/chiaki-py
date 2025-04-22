from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class Gyroscope:
    x: int = 0
    y: int = 0
    z: int = 0
