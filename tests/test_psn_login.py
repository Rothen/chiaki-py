import base64
import hashlib
from types import SimpleNamespace

import pytest

import chiaki_py.psn.login as login_module
from chiaki_py.psn import LoginError, PSNLoginParser, PSNLoginTerminal
from chiaki_py.psn.login import REDIRECT_URL, TOKEN_URL

USER_ID = "1234567890123456789"
ACCOUNT_INFO = {
    "scopes": "psn:clientapp",
    "expiration": "2026-10-01T00:00:00.000Z",
    "client_id": "client",
    "dcim_id": "dcim",
    "grant_type": "authorization_code",
    "user_id": USER_ID,
    "user_uuid": "uuid",
    "online_id": "player_one",
    "country_code": "CH",
    "language_code": "de",
    "community_domain": "a1",
    "is_sub_account": False,
}


class FakeRequests:
    def __init__(self, token_status=200, account_status=200, token="tok"):
        self.token_status = token_status
        self.account_status = account_status
        self.token = token
        self.posts = []
        self.gets = []

    def post(self, url, **kwargs):
        self.posts.append((url, kwargs))
        return SimpleNamespace(status_code=self.token_status, json=lambda: {"access_token": self.token})

    def get(self, url, **kwargs):
        self.gets.append((url, kwargs))
        return SimpleNamespace(status_code=self.account_status, json=lambda: dict(ACCOUNT_INFO))


@pytest.fixture
def fake_requests(monkeypatch):
    fake = FakeRequests()
    monkeypatch.setattr(login_module, "requests", fake)
    return fake


def redirect(code="abc123"):
    return f"{REDIRECT_URL}?code={code}&cid=x"


def test_parses_the_account(fake_requests):
    account = PSNLoginParser.parse_psn_account(redirect())

    assert account.online_id == "player_one"
    assert account.user_id == USER_ID
    assert account.is_sub_account is False


def test_user_rpid_is_the_little_endian_user_id_in_base64(fake_requests):
    account = PSNLoginParser.parse_psn_account(redirect())

    assert base64.b64decode(account.user_rpid) == int(USER_ID).to_bytes(8, "little")


def test_credentials_are_the_sha256_of_the_user_id(fake_requests):
    account = PSNLoginParser.parse_psn_account(redirect())

    assert account.credentials == hashlib.sha256(USER_ID.encode()).hexdigest()


def test_exchanges_the_code_then_fetches_the_account_with_the_token(fake_requests):
    PSNLoginParser.parse_psn_account(redirect("the-code"))

    (post_url, post_kwargs), = fake_requests.posts
    assert post_url == TOKEN_URL
    assert b"code=the-code" in post_kwargs["data"]
    (get_url, _), = fake_requests.gets
    assert get_url == f"{TOKEN_URL}/tok"


@pytest.mark.parametrize("url", [
    "https://example.com/?code=abc",       # not the redirect page
    f"{REDIRECT_URL}?cid=x",                # no code
    f"{REDIRECT_URL}?code=a",               # too short to be a code
])
def test_rejects_urls_without_a_usable_code(fake_requests, url):
    with pytest.raises(ValueError):
        PSNLoginParser.parse_psn_account(url)
    assert fake_requests.posts == []


def test_token_http_error(fake_requests):
    fake_requests.token_status = 400

    with pytest.raises(ValueError, match="400"):
        PSNLoginParser.parse_psn_account(redirect())


def test_missing_token(fake_requests):
    fake_requests.token = None

    with pytest.raises(ValueError, match="Token"):
        PSNLoginParser.parse_psn_account(redirect())


def test_account_http_error(fake_requests):
    fake_requests.account_status = 500

    with pytest.raises(ValueError, match="500"):
        PSNLoginParser.parse_psn_account(redirect())


class TestTerminalLogin:
    def test_reads_the_redirect_url(self, fake_requests, monkeypatch, capsys):
        monkeypatch.setattr("builtins.input", lambda prompt: "  " + redirect() + "\n")

        assert PSNLoginTerminal.login().online_id == "player_one"
        assert login_module.LOGIN_URL in capsys.readouterr().out

    def test_bad_url_raises_login_error(self, fake_requests, monkeypatch):
        monkeypatch.setattr("builtins.input", lambda prompt: "https://example.com")

        with pytest.raises(LoginError):
            PSNLoginTerminal.login()

    def test_no_input_raises_login_error(self, fake_requests, monkeypatch):
        def eof(prompt):
            raise EOFError

        monkeypatch.setattr("builtins.input", eof)

        with pytest.raises(LoginError, match="No redirect URL"):
            PSNLoginTerminal.login()
