from __future__ import annotations

from pathlib import Path

from app.agents.claude import run_claude_review
from app.agents.codex import run_codex
from app.config import settings
from app.models import RunResult, TaskStatus
from app.repository import get_task, update_task
from app.workspace.git_manager import create_worktree, get_diff


def run_task(task_id: int) -> RunResult:
    task = get_task(task_id)
    if task is None:
        raise KeyError(task_id)
    if task.status not in {TaskStatus.CREATED, TaskStatus.FAILED}:
        return RunResult(
            task_id=task.id,
            status=task.status,
            branch_name=task.branch_name,
            worktree_path=task.worktree_path,
            diff_path=task.diff_path,
            review_path=task.review_path,
            error=task.error,
        )

    log_dir = settings.logs_dir / f"task-{task.id}"
    log_dir.mkdir(parents=True, exist_ok=True)
    update_task(task.id, status=TaskStatus.RUNNING.value, log_dir=str(log_dir), error=None)

    try:
        branch_name, worktree_path = create_worktree(
            Path(task.repo_path),
            settings.workspaces_dir,
            task.id,
            task.title,
        )
        task = update_task(
            task.id,
            branch_name=branch_name,
            worktree_path=str(worktree_path),
        )

        run_codex(task, worktree_path, log_dir / "codex.log")
        task = update_task(task.id, status=TaskStatus.CODEX_DONE.value)

        diff_path = log_dir / "diff.patch"
        diff_path.write_text(get_diff(worktree_path), encoding="utf-8")
        task = update_task(task.id, diff_path=str(diff_path))

        review_path = log_dir / "claude-review.log"
        run_claude_review(task, worktree_path, diff_path, review_path)
        task = update_task(task.id, status=TaskStatus.REVIEWED.value, review_path=str(review_path))

        return RunResult(
            task_id=task.id,
            status=task.status,
            branch_name=task.branch_name,
            worktree_path=task.worktree_path,
            diff_path=task.diff_path,
            review_path=task.review_path,
        )
    except Exception as exc:
        task = update_task(task_id, status=TaskStatus.FAILED.value, error=str(exc))
        return RunResult(
            task_id=task.id,
            status=task.status,
            branch_name=task.branch_name,
            worktree_path=task.worktree_path,
            diff_path=task.diff_path,
            review_path=task.review_path,
            error=task.error,
        )
