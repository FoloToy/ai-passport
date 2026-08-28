#!/usr/bin/env python3
"""Dependency-free repository checks shared by local development and CI."""

from __future__ import annotations

import re
import subprocess
import sys
from collections import Counter
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parent.parent
FULL_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
CJK_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]")
SECRET_PATTERNS = {
    "GitHub token": re.compile(r"(?:ghp_|github_pat_)[A-Za-z0-9_]{20,}"),
    "AWS access key": re.compile(r"AKIA[0-9A-Z]{16}"),
    "private key": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
}
ROOT_MARKDOWN_ALLOWLIST = {
    "AGENTS.md",
    "AGENTS.zh_CN.md",
    "CLAUDE.md",
    "CLAUDE.zh_CN.md",
}
PROHIBITED_BASELINE_PREFIXES = (
    "assets/",
    "docs/assets/brand/",
    "docs/experiences/",
    "docs/software-design/",
    "plays/",
    "skills/",
)


def git_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return [ROOT / line for line in result.stdout.splitlines() if line]


def text_files() -> list[Path]:
    files: list[Path] = []
    for path in git_files():
        if not path.is_file() or path.stat().st_size > 2 * 1024 * 1024:
            continue
        try:
            path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        files.append(path)
    return files


def check_required_files(errors: list[str]) -> None:
    required = (
        "AGENTS.md",
        "AGENTS.zh_CN.md",
        "CLAUDE.md",
        "CLAUDE.zh_CN.md",
        "docs/README.md",
        "docs/README.zh_CN.md",
        "docs/INDEX.md",
        "docs/INDEX.zh_CN.md",
        "docs/CHANGELOG.md",
        ".github/CONTRIBUTING.md",
        ".github/CODE_OF_CONDUCT.md",
        ".github/SECURITY.md",
        ".github/SUPPORT.md",
        "dependencies.lock",
        "sdkconfig.defaults",
        "partitions.csv",
        ".github/PULL_REQUEST_TEMPLATE.md",
        ".github/workflows/build-firmware.yml",
        ".github/workflows/firmware-checks.yml",
        ".github/workflows/static-checks.yml",
        "tools/run-host-tests.sh",
        "tools/validate.sh",
    )
    for name in required:
        if not (ROOT / name).is_file():
            errors.append(f"missing required file: {name}")

    ignored = subprocess.run(
        ["git", "check-ignore", "-q", "dependencies.lock"], cwd=ROOT
    )
    if ignored.returncode == 0:
        errors.append("dependencies.lock must be tracked, not ignored")

    for path in sorted(ROOT.glob("*.md")):
        if path.name not in ROOT_MARKDOWN_ALLOWLIST:
            errors.append(
                f"{path.name}: root Markdown must move to docs/ or .github/"
            )

    for path in git_files():
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT).as_posix()
        for prefix in PROHIBITED_BASELINE_PREFIXES:
            if relative.startswith(prefix):
                errors.append(f"{relative}: content is outside the firmware baseline")


def check_markdown_links(files: list[Path], errors: list[str]) -> None:
    for path in files:
        if path.suffix.lower() != ".md":
            continue
        text = path.read_text(encoding="utf-8")
        for raw_target in MARKDOWN_LINK_RE.findall(text):
            target = raw_target.strip().split(maxsplit=1)[0].strip("<>")
            if not target or target.startswith(("#", "http://", "https://", "mailto:")):
                continue
            local = unquote(target.split("#", 1)[0])
            resolved = (ROOT / local.lstrip("/")) if local.startswith("/") else (path.parent / local)
            if local and not resolved.resolve().exists():
                errors.append(f"{path.relative_to(ROOT)}: missing link target {target}")


def check_document_languages(files: list[Path], errors: list[str]) -> None:
    """Require an English default and a linked Simplified Chinese peer."""
    markdown = {path.resolve() for path in files if path.suffix.lower() == ".md"}

    for path in sorted(markdown):
        name = path.name
        text = path.read_text(encoding="utf-8")
        opening = "\n".join(text.splitlines()[:8])

        if name.endswith(".zh_CN.md"):
            default_name = f"{name[:-len('.zh_CN.md')]}.md"
            default_path = path.with_name(default_name).resolve()
            if default_path not in markdown:
                errors.append(
                    f"{path.relative_to(ROOT)}: missing English default {default_name}"
                )
            elif default_name not in opening:
                errors.append(
                    f"{path.relative_to(ROOT)}: missing top language link to {default_name}"
                )
            continue

        chinese_name = f"{path.stem}.zh_CN.md"
        chinese_path = path.with_name(chinese_name).resolve()
        if chinese_path not in markdown:
            errors.append(
                f"{path.relative_to(ROOT)}: missing Simplified Chinese peer {chinese_name}"
            )
        elif chinese_name not in opening:
            errors.append(
                f"{path.relative_to(ROOT)}: missing top language link to {chinese_name}"
            )

        english_prose = text.replace("简体中文", "")
        match = CJK_RE.search(english_prose)
        if match:
            line = english_prose.count("\n", 0, match.start()) + 1
            errors.append(
                f"{path.relative_to(ROOT)}:{line}: default Markdown must use English prose"
            )


def normalized_local_links(path: Path, text: str) -> Counter[str]:
    links: Counter[str] = Counter()
    for raw_target in MARKDOWN_LINK_RE.findall(text):
        target = raw_target.strip().split(maxsplit=1)[0].strip("<>")
        if not target or target.startswith(("#", "http://", "https://", "mailto:")):
            continue
        local = unquote(target.split("#", 1)[0])
        resolved = (
            (ROOT / local.lstrip("/"))
            if local.startswith("/")
            else (path.parent / local)
        ).resolve()
        try:
            relative = resolved.relative_to(ROOT).as_posix()
        except ValueError:
            continue
        links[relative.replace(".zh_CN.md", ".md")] += 1
    return links


def check_document_structure(files: list[Path], errors: list[str]) -> None:
    """Keep every maintained translation structurally aligned."""
    markdown = {path.resolve() for path in files if path.suffix.lower() == ".md"}
    for default_path in sorted(markdown):
        if default_path.name.endswith(".zh_CN.md"):
            continue
        chinese_path = default_path.with_name(f"{default_path.stem}.zh_CN.md")
        if chinese_path.resolve() not in markdown:
            continue

        name = default_path.relative_to(ROOT).as_posix()

        default_text = default_path.read_text(encoding="utf-8")
        chinese_text = chinese_path.read_text(encoding="utf-8")
        default_headings = [
            len(match) for match in re.findall(r"(?m)^(#{1,6})\s+", default_text)
        ]
        chinese_headings = [
            len(match) for match in re.findall(r"(?m)^(#{1,6})\s+", chinese_text)
        ]
        if default_headings != chinese_headings:
            errors.append(f"{name}: bilingual heading levels are not aligned")

        if len(re.findall(r"(?m)^```", default_text)) != len(
            re.findall(r"(?m)^```", chinese_text)
        ):
            errors.append(f"{name}: bilingual fenced-code blocks are not aligned")

        if normalized_local_links(default_path, default_text) != normalized_local_links(
            chinese_path, chinese_text
        ):
            errors.append(f"{name}: bilingual local-link targets are not aligned")


def check_action_pins(errors: list[str]) -> None:
    workflow_dir = ROOT / ".github" / "workflows"
    for path in sorted(workflow_dir.glob("*.y*ml")):
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = re.match(r"\s*-?\s*uses:\s*['\"]?([^'\"\s]+)", line)
            if not match:
                continue
            action = match.group(1)
            if action.startswith("./"):
                continue
            if action.startswith("docker://"):
                if "@sha256:" not in action:
                    errors.append(f"{path.relative_to(ROOT)}:{line_number}: unpinned Docker action {action}")
                continue
            if "@" not in action or not FULL_SHA_RE.fullmatch(action.rsplit("@", 1)[1]):
                errors.append(f"{path.relative_to(ROOT)}:{line_number}: action must use a full commit SHA: {action}")


def check_issue_forms(errors: list[str]) -> None:
    issue_dir = ROOT / ".github" / "ISSUE_TEMPLATE"
    for name in ("feature_request.yml", "usage_question.yml"):
        path = issue_dir / name
        if not path.is_file():
            errors.append(f"missing issue form: {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8")
        for field in ("name:", "description:", "body:"):
            if not re.search(rf"(?m)^{re.escape(field)}", text):
                errors.append(f"{path.relative_to(ROOT)}: missing top-level {field[:-1]}")


def check_sensitive_content(files: list[Path], errors: list[str]) -> None:
    for path in files:
        text = path.read_text(encoding="utf-8")
        for label, pattern in SECRET_PATTERNS.items():
            if pattern.search(text):
                errors.append(f"{path.relative_to(ROOT)}: possible {label}")

        for match in re.finditer(r"https://ai-passport\.folotoy\.cn/trae/\?s=([^&\s)]+)&k=([^\s)]+)", text):
            if "<" not in match.group(1) and "AAAAAA" not in match.group(1):
                errors.append(f"{path.relative_to(ROOT)}: possible unsanitized device QR link")


def check_conflict_markers(files: list[Path], errors: list[str]) -> None:
    marker = re.compile(r"(?m)^(<<<<<<< |=======\s*$|>>>>>>> )")
    for path in files:
        if marker.search(path.read_text(encoding="utf-8")):
            errors.append(f"{path.relative_to(ROOT)}: unresolved merge conflict marker")


def main() -> int:
    errors: list[str] = []
    files = text_files()
    check_required_files(errors)
    check_markdown_links(files, errors)
    check_document_languages(files, errors)
    check_document_structure(files, errors)
    check_action_pins(errors)
    check_issue_forms(errors)
    check_sensitive_content(files, errors)
    check_conflict_markers(files, errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(f"Repository checks: PASS ({len(files)} text files scanned)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
