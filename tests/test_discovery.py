import pytest

import chiaki_py.discovery as discovery_module
from chiaki_py import discover_hosts
from chiaki_py.lib import Settings


class FakeDiscoveryManager:
    instances: list["FakeDiscoveryManager"] = []

    def __init__(self):
        self.settings = None
        self.active_changes: list[bool] = []
        self.hosts = ["host-a", "host-b"]
        self.fail = False
        FakeDiscoveryManager.instances.append(self)

    def set_settings(self, settings):
        self.settings = settings

    def set_active(self, active):
        self.active_changes.append(active)

    def get_hosts(self):
        if self.fail:
            raise RuntimeError("boom")
        return self.hosts


@pytest.fixture
def manager_cls(monkeypatch):
    FakeDiscoveryManager.instances = []
    monkeypatch.setattr(discovery_module, "DiscoveryManager", FakeDiscoveryManager)
    monkeypatch.setattr(discovery_module.time, "sleep", lambda seconds: None)
    return FakeDiscoveryManager


def test_returns_hosts_and_stops_discovery(manager_cls):
    settings = Settings()

    assert discover_hosts(settings, timeout=0.0) == ["host-a", "host-b"]

    manager = manager_cls.instances[0]
    assert manager.settings is settings
    assert manager.active_changes == [True, False]


def test_waits_for_the_timeout(manager_cls, monkeypatch):
    slept = []
    monkeypatch.setattr(discovery_module.time, "sleep", slept.append)

    discover_hosts(Settings(), timeout=1.5)

    assert slept == [1.5]


def test_stops_discovery_even_if_collecting_fails(manager_cls, monkeypatch):
    original_init = FakeDiscoveryManager.__init__

    def failing_init(self):
        original_init(self)
        self.fail = True

    monkeypatch.setattr(FakeDiscoveryManager, "__init__", failing_init)

    with pytest.raises(RuntimeError):
        discover_hosts(Settings(), timeout=0.0)

    assert manager_cls.instances[0].active_changes == [True, False]
