from __future__ import annotations

from pathlib import Path

from fastapi import FastAPI, HTTPException, Response

from app.db import init_db
from app.models import RunResult, TaskCreate, TaskCreated, TaskRead
from app.repository import create_task, get_task, list_tasks
from app.tasks.runner import run_task


app = FastAPI(title="Dev Hub", version="0.1.0")


@app.on_event("startup")
def startup() -> None:
    init_db()


@app.get("/health")
def health() -> dict[str, bool]:
    return {"ok": True}


@app.post("/tasks", response_model=TaskCreated)
def create_task_endpoint(payload: TaskCreate) -> TaskCreated:
    task = create_task(payload)
    return TaskCreated(task_id=task.id, status=task.status)


@app.get("/tasks", response_model=list[TaskRead])
def list_tasks_endpoint() -> list[TaskRead]:
    return list_tasks()


@app.get("/tasks/{task_id}", response_model=TaskRead)
def get_task_endpoint(task_id: int) -> TaskRead:
    task = get_task(task_id)
    if task is None:
        raise HTTPException(status_code=404, detail="task not found")
    return task


@app.post("/tasks/{task_id}/run", response_model=RunResult)
def run_task_endpoint(task_id: int) -> RunResult:
    if get_task(task_id) is None:
        raise HTTPException(status_code=404, detail="task not found")
    return run_task(task_id)


def _read_task_file(task_id: int, field: str, media_type: str) -> Response:
    task = get_task(task_id)
    if task is None:
        raise HTTPException(status_code=404, detail="task not found")
    path_value = getattr(task, field)
    if not path_value:
        raise HTTPException(status_code=404, detail=f"{field} not available")
    path = Path(path_value)
    if not path.exists():
        raise HTTPException(status_code=404, detail=f"{field} file missing")
    return Response(path.read_text(encoding="utf-8"), media_type=media_type)


@app.get("/tasks/{task_id}/diff")
def get_task_diff(task_id: int) -> Response:
    return _read_task_file(task_id, "diff_path", "text/x-diff; charset=utf-8")


@app.get("/tasks/{task_id}/review")
def get_task_review(task_id: int) -> Response:
    return _read_task_file(task_id, "review_path", "text/plain; charset=utf-8")
