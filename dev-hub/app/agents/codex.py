from __future__ import annotations

from pathlib import Path

from app.agents.runner import run_agent_command
from app.config import settings
from app.models import TaskRead


def run_codex(task: TaskRead, worktree_path: Path, log_path: Path) -> None:
    context = {
        "prompt": task.prompt,
        "task_id": task.id,
        "title": task.title,
        "repo_path": task.repo_path,
        "worktree_path": worktree_path,
        "diff_path": "",
    }
    run_agent_command(
        settings.codex_command,
        context,
        cwd=worktree_path,
        log_path=log_path,
        timeout_seconds=settings.command_timeout_seconds,
    )
