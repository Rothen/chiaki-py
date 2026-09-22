from pathlib import Path
from typing import TypeVar, Callable

from pydantic import BaseModel

T = TypeVar("T", bound=BaseModel)


class Serializer:
    """Loads and saves any pydantic `BaseModel` (e.g. `HostRegistration`) as indented JSON,
    so registrations and other settings can be kept in a file between runs."""

    @classmethod
    def load(cls, dst_cls: type[T], file: Path) -> T:
        """Read and validate `file` as JSON into a `dst_cls` instance. Raises FileNotFoundError
        if it doesn't exist, or a pydantic ValidationError if its contents don't match."""
        with open(file, encoding="utf-8") as f:
            return dst_cls.model_validate_json(f.read())

    @classmethod
    def load_or(cls, dst_cls: type[T], file: Path, or_fn: Callable[[], T], save: bool = True) -> T:
        """Like `load()`, but if `file` doesn't exist yet, build the object with `or_fn()` instead
        (e.g. running interactive registration) and save it to `file` for next time."""
        try:
            with open(file, encoding="utf-8") as f:
                return dst_cls.model_validate_json(f.read())
        except FileNotFoundError:
            obj: T = or_fn()
            if save:
                cls.save(obj, file)
            return obj

    @classmethod
    def save(cls, src: BaseModel, file: Path) -> None:
        """Write `src` to `file` as indented JSON, overwriting it if it already exists."""
        with open(file, "w", encoding="utf-8") as f:
            f.write(src.model_dump_json(indent=2))
