import json

import pytest
from pydantic import ValidationError

from chiaki_py import HostRegistration, Serializer
from chiaki_py.lib import Target


def test_save_then_load_round_trips(tmp_path, registration):
    path = tmp_path / "host.json"
    Serializer.save(registration, path)

    assert Serializer.load(HostRegistration, path) == registration


def test_save_writes_indented_json(tmp_path, registration):
    path = tmp_path / "host.json"
    Serializer.save(registration, path)

    text = path.read_text(encoding="utf-8")
    assert text.startswith("{\n  ")
    assert json.loads(text)["nickname"] == "PS5-083"


def test_save_overwrites(tmp_path, registration):
    path = tmp_path / "host.json"
    Serializer.save(registration, path)
    Serializer.save(registration.model_copy(update={"nickname": "other"}), path)

    assert Serializer.load(HostRegistration, path).nickname == "other"


def test_load_missing_file_raises(tmp_path):
    with pytest.raises(FileNotFoundError):
        Serializer.load(HostRegistration, tmp_path / "missing.json")


def test_load_invalid_contents_raises(tmp_path):
    path = tmp_path / "host.json"
    path.write_text('{"host": "1.2.3.4"}', encoding="utf-8")

    with pytest.raises(ValidationError):
        Serializer.load(HostRegistration, path)


def test_load_or_loads_existing_file_without_calling_fallback(tmp_path, registration):
    path = tmp_path / "host.json"
    Serializer.save(registration, path)

    def fallback():
        raise AssertionError("fallback must not run when the file exists")

    assert Serializer.load_or(HostRegistration, path, fallback) == registration


def test_load_or_builds_and_saves_missing_file(tmp_path, registration):
    path = tmp_path / "host.json"

    assert Serializer.load_or(HostRegistration, path, lambda: registration) == registration
    assert Serializer.load(HostRegistration, path) == registration


def test_load_or_without_save_leaves_no_file(tmp_path, registration):
    path = tmp_path / "host.json"

    Serializer.load_or(HostRegistration, path, lambda: registration, save=False)

    assert not path.exists()


def test_load_or_does_not_swallow_invalid_contents(tmp_path, registration):
    path = tmp_path / "host.json"
    path.write_text("not json", encoding="utf-8")

    with pytest.raises(ValidationError):
        Serializer.load_or(HostRegistration, path, lambda: registration)


def test_target_is_kept_through_a_file(tmp_path, registration):
    path = tmp_path / "host.json"
    Serializer.save(registration.model_copy(update={"target": Target.PS4_10}), path)

    assert Serializer.load(HostRegistration, path).target == Target.PS4_10
