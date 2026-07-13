# Dev Hub

Dev Hub is a small local orchestrator for personal coding workflows. It creates an isolated git worktree for a task, runs Codex CLI to implement it, captures the diff, then asks Claude Code CLI to review the result.

## MVP Flow

1. Create a task with a repository path and prompt.
2. Dev Hub creates a branch and git worktree under `workspaces/`.
3. Dev Hub runs Codex in the worktree and writes `logs/task-<id>/codex.log`.
4. Dev Hub saves `logs/task-<id>/diff.patch`.
5. Dev Hub runs Claude review and writes `logs/task-<id>/claude-review.log`.

## Install

```powershell
cd D:\zgp\xiaozhi_moss\dev-hub
python -m venv .venv
.\.venv\Scripts\pip install -e .[dev]
```

## Run

```powershell
.\.venv\Scripts\uvicorn app.main:app --reload --host 127.0.0.1 --port 8765
```

By default, SQLite data is stored outside the repo:

- Windows: `%LOCALAPPDATA%\dev-hub\devhub.sqlite`
- Linux/macOS: `~/.local/share/dev-hub/devhub.sqlite`

Override it when needed:

```powershell
$env:DEVHUB_DATA_DIR="D:\devhub-data"
```

## Example

```powershell
$body = @{
  repo_path = "D:/zgp/xiaozhi_moss/ESP32-Robot"
  title = "Fix motor stop"
  prompt = "Analyze and fix motor stop logic so a stop command interrupts an active motor move."
} | ConvertTo-Json

Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8765/tasks -ContentType "application/json" -Body $body
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8765/tasks/1/run
Invoke-RestMethod -Uri http://127.0.0.1:8765/tasks/1
```

## CLI Command Templates

The default command templates are intentionally simple:

- Codex: `["codex", "exec", "{prompt}"]`
- Claude: `["claude", "-p", "{prompt}"]`

Override them if your local CLI uses different flags:

```powershell
$env:DEVHUB_CODEX_COMMAND_JSON='["codex","exec","--skip-git-repo-check","{prompt}"]'
$env:DEVHUB_CLAUDE_COMMAND_JSON='["claude","-p","{prompt}"]'
```

Available placeholders:

- `{prompt}`
- `{task_id}`
- `{title}`
- `{repo_path}`
- `{worktree_path}`
- `{diff_path}`

## API

- `GET /health`
- `POST /tasks`
- `GET /tasks`
- `GET /tasks/{task_id}`
- `POST /tasks/{task_id}/run`
- `GET /tasks/{task_id}/diff`
- `GET /tasks/{task_id}/review`

The MVP does not auto-merge or push task branches. Review the generated branch and merge manually.
