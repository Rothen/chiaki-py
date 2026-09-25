from types import SimpleNamespace

import pytest

import chiaki_py.registration as registration_module
from chiaki_py import HostRegistration, register_host
from chiaki_py.lib import Settings, Target


def make(**overrides) -> HostRegistration:
    fields = dict(host="10.0.0.2", target=Target.PS5_1, regist_key="abcd", nickname="PS5", morning=b"\x00\x01\xfe\xff")
    fields.update(overrides)
    return HostRegistration(**fields)


class TestHostRegistration:
    def test_morning_is_serialized_as_hex(self):
        assert make().model_dump(mode="json")["morning"] == "0001feff"

    def test_morning_accepts_hex_or_bytes(self):
        assert make(morning="0001feff").morning == b"\x00\x01\xfe\xff"
        assert make(morning=b"\x10").morning == b"\x10"

    def test_target_is_serialized_by_name(self):
        assert make(target=Target.PS4_10).model_dump(mode="json")["target"] == "PS4_10"

    @pytest.mark.parametrize("value", [Target.PS4_10, "PS4_10", int(Target.PS4_10)])
    def test_target_accepts_enum_name_or_value(self, value):
        assert make(target=value).target == Target.PS4_10

    def test_unknown_target_name_is_rejected(self):
        with pytest.raises(Exception):
            make(target="PS6")

    def test_json_round_trip(self):
        original = make(duid="d", auto_regist=True, discover_timeout=5.0)
        assert HostRegistration.model_validate_json(original.model_dump_json()) == original

    def test_defaults(self):
        registration = make()
        assert registration.initial_login_pin == ""
        assert registration.duid == ""
        assert registration.auto_regist is False
        assert registration.ps5 is True


class FakeBackend:
    calls: list[dict] = []

    def __init__(self, settings):
        self.settings = settings

    def register_host(self, **kwargs):
        FakeBackend.calls.append(kwargs)
        return SimpleNamespace(rp_regist_key="key123", server_nickname="PS4-1", rp_key=b"\xaa\xbb")


@pytest.fixture
def fake_backend(monkeypatch):
    FakeBackend.calls = []
    monkeypatch.setattr(registration_module, "Backend", FakeBackend)
    return FakeBackend


def test_register_host_passes_arguments_through(fake_backend):
    register_host(Settings(), host="10.0.0.3", psn_id="id", pin="12345678", console_pin="1111",
                  target=Target.PS4_10, broadcast=True)

    assert fake_backend.calls == [dict(host="10.0.0.3", psn_id="id", pin="12345678", cpin="1111",
                                       broadcast=True, target=Target.PS4_10)]


def test_register_host_builds_registration_from_result(fake_backend):
    result = register_host(Settings(), host="10.0.0.3", psn_id="id", pin="12345678")

    assert result.host == "10.0.0.3"
    assert result.regist_key == "key123"
    assert result.nickname == "PS4-1"
    assert result.morning == b"\xaa\xbb"


def test_register_host_keeps_the_target(fake_backend):
    result = register_host(Settings(), host="10.0.0.3", psn_id="id", pin="12345678", target=Target.PS4_10)

    assert result.target == Target.PS4_10
