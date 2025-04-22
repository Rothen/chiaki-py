from dataclasses import dataclass

@dataclass(frozen=True, slots=True)
class Orientation:
    pitch: float = 0
    roll: float = 0
    yaw: float = 0
