from __future__ import annotations

from pathlib import Path

from app.agents.runner import run_agent_command
from app.config import settings
from app.models import TaskRead


REVIEW_PROMPT = """Review the changes for this task.

Task title:
{title}

Task prompt:
{prompt}

Focus on:
1. Correctness and behavioral regressions.
2. Concurrency, filesystem, network, and state-management risks.
3. Whether the change actually satisfies the task.
4. Missing tests or verification gaps.
5. Simpler alternatives if the implementation is unnecessarily complex.

Diff:
{diff}
"""


def run_claude_review(task: TaskRead, worktree_path: Path, diff_path: Path, log_path: Path) -> None:
    diff = diff_path.read_text(encoding="utf-8") if diff_path.exists() else ""
    prompt = REVIEW_PROMPT.format(title=task.title, prompt=task.prompt, diff=diff or "(empty diff)")
    context = {
        "prompt": prompt,
        "task_id": task.id,
        "title": task.title,
        "repo_path": task.repo_path,
        "worktree_path": worktree_path,
        "diff_path": diff_path,
    }
    run_agent_command(
        settings.claude_command,
        context,
        cwd=worktree_path,
        log_path=log_path,
        timeout_seconds=settings.command_timeout_seconds,
    )
