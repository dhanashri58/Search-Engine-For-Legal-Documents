from __future__ import annotations

import re
import subprocess
from pathlib import Path

from flask import Flask, render_template, request


BASE_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = BASE_DIR.parent

app = Flask(__name__)

RESULT_LINE_RE = re.compile(r"^\[(\d+)\]\s+(.+)\s+\(Score:\s*([0-9.]+)\)$")
DOC_COUNT_RE = re.compile(r"SEARCH ENGINE LOADED:\s*(\d+)\s+documents indexed", re.IGNORECASE)

SAMPLE_QUERIES = [
    "contract breach",
    "privacy OR compliance",
    "meta doc10.txt",
    "ac con",
]


def find_executable() -> Path | None:
    candidates = [
        PROJECT_ROOT / "search_engine.exe",
        PROJECT_ROOT / "search_engine",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def prettify_path(raw_path: str) -> str:
    raw_path = raw_path.strip()
    try:
        path = Path(raw_path)
        resolved = path.resolve()
        relative = resolved.relative_to(PROJECT_ROOT.resolve())
        return str(relative)
    except Exception:
        return raw_path


def parse_engine_output(raw_output: str) -> dict:
    doc_count = None
    count_match = DOC_COUNT_RE.search(raw_output)
    if count_match:
        doc_count = int(count_match.group(1))

    parts = raw_output.split("ADS@Search >>")
    response_block = parts[1].strip() if len(parts) > 1 else ""
    lines = [line.strip() for line in response_block.splitlines() if line.strip()]

    parsed = {
        "doc_count": doc_count,
        "mode": "empty",
        "headline": None,
        "results": [],
        "suggestions": [],
        "message": None,
        "raw": response_block,
    }

    if not lines:
        parsed["message"] = "No output was returned by the engine."
        return parsed

    first = lines[0]

    if first.startswith("--- Displaying Top"):
        parsed["mode"] = "results"
        parsed["headline"] = first.strip("- ").strip()
        for line in lines[1:]:
            match = RESULT_LINE_RE.match(line)
            if not match:
                continue
            rank = int(match.group(1))
            raw_path = match.group(2)
            score = float(match.group(3))
            parsed["results"].append(
                {
                    "rank": rank,
                    "path": prettify_path(raw_path),
                    "filename": Path(raw_path).name,
                    "score": f"{score:.2f}",
                }
            )
        if not parsed["results"]:
            parsed["mode"] = "raw"
            parsed["message"] = response_block
        return parsed

    if first.startswith("AVL Result:"):
        parsed["mode"] = "metadata"
        parsed["headline"] = "Metadata Lookup"
        parsed["message"] = first
        return parsed

    if "Trie Autocomplete Suggestions" in first:
        parsed["mode"] = "autocomplete"
        parsed["headline"] = "Autocomplete Suggestions"
        parsed["suggestions"] = lines[1:]
        return parsed

    if first.startswith("No results found"):
        parsed["mode"] = "empty"
        parsed["headline"] = "No Matches"
        parsed["message"] = first
        return parsed

    parsed["mode"] = "raw"
    parsed["headline"] = "Engine Output"
    parsed["message"] = response_block
    return parsed


@app.route("/", methods=["GET", "POST"])
def index():
    query = ""
    response = None
    error = None

    if request.method == "POST":
        query = request.form.get("query", "").strip()

        if query:
            executable = find_executable()
            if not executable:
                error = (
                    "Search engine executable not found. Build `search_engine` "
                    "or `search_engine.exe` in the project root first."
                )
            else:
                try:
                    completed = subprocess.run(
                        [str(executable)],
                        input=f"{query}\nquit\n",
                        text=True,
                        capture_output=True,
                        cwd=PROJECT_ROOT,
                        timeout=30,
                        check=False,
                    )
                    response = parse_engine_output(completed.stdout)
                    if completed.returncode != 0 and completed.stderr.strip():
                        error = completed.stderr.strip()
                except subprocess.TimeoutExpired:
                    error = "The search engine timed out while processing the query."
                except OSError as exc:
                    error = f"Failed to run the search engine: {exc}"

    return render_template(
        "index.html",
        query=query,
        response=response,
        error=error,
        sample_queries=SAMPLE_QUERIES,
    )


if __name__ == "__main__":
    app.run(debug=True)
