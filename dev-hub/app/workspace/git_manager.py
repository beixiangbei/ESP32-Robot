from __future__ import annotations

import re
import subprocess
from pathlib import Path


class GitError(RuntimeError):
    pass


def _run_git(args: list[str], cwd: Path, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        ["git", *args],
        cwd=str(cwd),
        text=True,
        capture_output=True,
        timeout=timeout,
    )
    if proc.returncode != 0:
        raise GitError(proc.stderr.strip() or proc.stdout.strip() or "git command failed")
    return proc


def slugify(value: str) -> str:
    slug = re.sub(r"[^a-zA-Z0-9._-]+", "-", value.strip()).strip("-").lower()
    return slug[:60] or "task"


def repo_name(repo_path: Path) -> str:
    return slugify(repo_path.name)


def ensure_clean_enough(repo_path: Path) -> None:
    _run_git(["rev-parse", "--show-toplevel"], repo_path)


def create_worktree(repo_path: Path, workspaces_dir: Path, task_id: int, title: str) -> tuple[str, Path]:
    ensure_clean_enough(repo_path)
    branch_name = f"devhub/task-{task_id}-{slugify(title)}"
    target = workspaces_dir / f"{repo_name(repo_path)}-task-{task_id}"
    if target.exists():
        raise GitError(f"worktree already exists: {target}")
    _run_git(["worktree", "add", str(target), "-b", branch_name], repo_path, timeout=300)
    return branch_name, target.resolve()


def get_diff(worktree_path: Path) -> str:
    proc = _run_git(["diff", "--binary", "HEAD"], worktree_path, timeout=120)
    return proc.stdout


def get_status(worktree_path: Path) -> str:
    proc = _run_git(["status", "--short"], worktree_path, timeout=120)
    return proc.stdout
