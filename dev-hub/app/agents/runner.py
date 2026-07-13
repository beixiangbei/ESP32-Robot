from __future__ import annotations

import subprocess
from pathlib import Path


class AgentCommandError(RuntimeError):
    def __init__(self, message: str, returncode: int | None = None) -> None:
        super().__init__(message)
        self.returncode = returncode


def render_command(template: list[str], context: dict[str, object]) -> list[str]:
    rendered: list[str] = []
    str_context = {key: str(value) for key, value in context.items()}
    for part in template:
        rendered.append(part.format(**str_context))
    return rendered


def run_agent_command(
    command_template: list[str],
    context: dict[str, object],
    cwd: Path,
    log_path: Path,
    timeout_seconds: int,
) -> None:
    command = render_command(command_template, context)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        log.write(f"$ {' '.join(command)}\n\n")
        log.flush()
        try:
            proc = subprocess.run(
                command,
                cwd=str(cwd),
                text=True,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=timeout_seconds,
            )
        except subprocess.TimeoutExpired as exc:
            raise AgentCommandError(f"agent command timed out after {timeout_seconds}s") from exc
        except FileNotFoundError as exc:
            raise AgentCommandError(f"agent command not found: {command[0]}") from exc
    if proc.returncode != 0:
        raise AgentCommandError(f"agent command failed with exit code {proc.returncode}", proc.returncode)
