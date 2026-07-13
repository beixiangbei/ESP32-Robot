from app.agents.runner import render_command


def test_render_command_replaces_placeholders() -> None:
    command = render_command(
        ["codex", "exec", "{prompt}", "--task", "{task_id}"],
        {"prompt": "hello", "task_id": 42},
    )
    assert command == ["codex", "exec", "hello", "--task", "42"]
