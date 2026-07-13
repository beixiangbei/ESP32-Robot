from __future__ import annotations

import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]


def _default_data_dir() -> Path:
    configured = os.getenv("DEVHUB_DATA_DIR")
    if configured:
        return Path(configured).expanduser()
    local_app_data = os.getenv("LOCALAPPDATA")
    if local_app_data:
        return Path(local_app_data) / "dev-hub"
    return Path.home() / ".local" / "share" / "dev-hub"


def _command_from_env(name: str, default: list[str]) -> list[str]:
    raw = os.getenv(name)
    if not raw:
        return default
    try:
        value = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ValueError(f"{name} must be a JSON array of strings") from exc
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ValueError(f"{name} must be a JSON array of strings")
    return value


@dataclass(frozen=True)
class Settings:
    root_dir: Path = ROOT_DIR
    data_dir: Path = _default_data_dir()
    logs_dir: Path = ROOT_DIR / "logs"
    workspaces_dir: Path = ROOT_DIR / "workspaces"
    database_path: Path = _default_data_dir() / "devhub.sqlite"
    command_timeout_seconds: int = int(os.getenv("DEVHUB_COMMAND_TIMEOUT_SECONDS", "3600"))
    codex_command: list[str] = None  # type: ignore[assignment]
    claude_command: list[str] = None  # type: ignore[assignment]

    def __post_init__(self) -> None:
        object.__setattr__(
            self,
            "codex_command",
            _command_from_env("DEVHUB_CODEX_COMMAND_JSON", ["codex", "exec", "{prompt}"]),
        )
        object.__setattr__(
            self,
            "claude_command",
            _command_from_env("DEVHUB_CLAUDE_COMMAND_JSON", ["claude", "-p", "{prompt}"]),
        )
        data_dir = self.data_dir
        database_path = self.database_path
        try:
            data_dir.mkdir(parents=True, exist_ok=True)
        except PermissionError:
            data_dir = Path(tempfile.gettempdir()) / "dev-hub"
            database_path = data_dir / "devhub.sqlite"
            data_dir.mkdir(parents=True, exist_ok=True)
            object.__setattr__(self, "data_dir", data_dir)
            object.__setattr__(self, "database_path", database_path)
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.workspaces_dir.mkdir(parents=True, exist_ok=True)


settings = Settings()
