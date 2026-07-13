from __future__ import annotations

from app.db import connect
from app.models import TaskCreate, TaskRead, TaskStatus


def create_task(payload: TaskCreate) -> TaskRead:
    with connect() as conn:
        cursor = conn.execute(
            """
            INSERT INTO tasks (repo_path, title, prompt, status)
            VALUES (?, ?, ?, ?)
            """,
            (payload.repo_path, payload.title, payload.prompt, TaskStatus.CREATED.value),
        )
        task_id = int(cursor.lastrowid)
        row = conn.execute("SELECT * FROM tasks WHERE id = ?", (task_id,)).fetchone()
        return TaskRead.from_row(row)


def list_tasks() -> list[TaskRead]:
    with connect() as conn:
        rows = conn.execute("SELECT * FROM tasks ORDER BY id DESC").fetchall()
        return [TaskRead.from_row(row) for row in rows]


def get_task(task_id: int) -> TaskRead | None:
    with connect() as conn:
        row = conn.execute("SELECT * FROM tasks WHERE id = ?", (task_id,)).fetchone()
        return TaskRead.from_row(row) if row else None


def update_task(task_id: int, **fields: object) -> TaskRead:
    if not fields:
        task = get_task(task_id)
        if task is None:
            raise KeyError(task_id)
        return task
    assignments = ", ".join(f"{key} = ?" for key in fields)
    values = list(fields.values())
    values.append(task_id)
    with connect() as conn:
        conn.execute(f"UPDATE tasks SET {assignments} WHERE id = ?", values)
        row = conn.execute("SELECT * FROM tasks WHERE id = ?", (task_id,)).fetchone()
        if row is None:
            raise KeyError(task_id)
        return TaskRead.from_row(row)
