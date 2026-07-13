from __future__ import annotations

from enum import StrEnum
from pathlib import Path
from typing import Any

from pydantic import BaseModel, Field, field_validator


class TaskStatus(StrEnum):
    CREATED = "created"
    RUNNING = "running"
    CODEX_DONE = "codex_done"
    REVIEWED = "reviewed"
    FAILED = "failed"


class TaskCreate(BaseModel):
    repo_path: str = Field(..., min_length=1)
    title: str = Field(..., min_length=1, max_length=200)
    prompt: str = Field(..., min_length=1)

    @field_validator("repo_path")
    @classmethod
    def repo_path_must_exist(cls, value: str) -> str:
        path = Path(value).expanduser()
        if not path.exists():
            raise ValueError("repo_path does not exist")
        if not (path / ".git").exists():
            raise ValueError("repo_path must point to a git repository root")
        return str(path.resolve())


class TaskRead(BaseModel):
    id: int
    repo_path: str
    title: str
    prompt: str
    status: TaskStatus
    branch_name: str | None = None
    worktree_path: str | None = None
    log_dir: str | None = None
    diff_path: str | None = None
    review_path: str | None = None
    error: str | None = None
    created_at: str
    updated_at: str

    @classmethod
    def from_row(cls, row: Any) -> "TaskRead":
        return cls(**dict(row))


class TaskCreated(BaseModel):
    task_id: int
    status: TaskStatus


class RunResult(BaseModel):
    task_id: int
    status: TaskStatus
    branch_name: str | None = None
    worktree_path: str | None = None
    diff_path: str | None = None
    review_path: str | None = None
    error: str | None = None
