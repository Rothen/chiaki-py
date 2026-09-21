from pathlib import Path
from typing import TypeVar, Callable

from pydantic import BaseModel

T = TypeVar("T", bound=BaseModel)


class Serializer:
    @classmethod
    def load(cls, dst_cls: type[T], file: Path) -> T:
        with open(file, encoding="utf-8") as f:
            return dst_cls.model_validate_json(f.read())

    @classmethod
    def load_or(cls, dst_cls: type[T], file: Path, or_fn: Callable[[], T], save: bool = True) -> T:
        try:
            with open(file, encoding="utf-8") as f:
                return dst_cls.model_validate_json(f.read())
        except FileNotFoundError:
            obj: T = or_fn()
            cls.save(obj, file)
            return or_fn()

    @classmethod
    def save(cls, src: BaseModel, file: Path) -> None:
        with open(file, "w", encoding="utf-8") as f:
            f.write(src.model_dump_json(indent=2))
