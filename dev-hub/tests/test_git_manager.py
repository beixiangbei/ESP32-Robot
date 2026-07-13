from app.workspace.git_manager import slugify


def test_slugify_keeps_safe_text() -> None:
    assert slugify("Fix motor stop") == "fix-motor-stop"


def test_slugify_has_fallback() -> None:
    assert slugify("中文任务") == "task"
