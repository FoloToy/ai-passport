#!/usr/bin/env python3
"""Dependency-free repository checks shared by local development and CI."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parent.parent
FULL_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HTML_LINK_RE = re.compile(r"\bhref\s*=\s*['\"]([^'\"]+)['\"]", re.IGNORECASE)
MARKDOWN_REFERENCE_RE = re.compile(r"(?m)^ {0,3}\[[^\]]+\]:\s*(<[^>\n]+>|\S+)")
COMMUNITY_DOCUMENT_NAMES = {
    f"{stem}{suffix}.md"
    for stem in ("CONTRIBUTING", "CODE_OF_CONDUCT", "SECURITY", "SUPPORT")
    for suffix in ("", ".zh_CN")
}
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
    "README.md",
    "README.zh_CN.md",
}
# Register only concrete, vendored component directories, e.g. "components/foo".
# These exemptions never change the input to sensitive-content/conflict checks.
VENDORED_DOC_ROOTS: tuple[str, ...] = ()


def vendored_document_roots(errors: list[str]) -> tuple[Path, ...]:
    """Validate explicit repository-relative directories; reject symlink roots."""
    roots: list[Path] = []
    for name in VENDORED_DOC_ROOTS:
        if (
            not isinstance(name, str)
            or not name
            or "\\" in name
            or "\x00" in name
            or re.match(r"^[A-Za-z]:", name)
            or any(part in ("", ".", "..") for part in name.split("/"))
        ):
            errors.append(f"invalid VENDORED_DOC_ROOTS entry {name!r}: use a repository-relative directory")
            continue
        path = ROOT / name
        try:
            resolved = path.resolve(strict=True)
        except (OSError, RuntimeError):
            errors.append(f"invalid VENDORED_DOC_ROOTS entry {name!r}: directory does not resolve")
            continue
        if resolved != path or not resolved.is_relative_to(ROOT) or not resolved.is_dir():
            errors.append(f"invalid VENDORED_DOC_ROOTS entry {name!r}: require a real directory without symlinks")
            continue
        roots.append(resolved)
    return tuple(roots)


def is_vendored_document(path: Path, roots: tuple[Path, ...]) -> bool:
    """Require both the file's listed path and its target to stay in one root."""
    for root in roots:
        if not path.is_relative_to(root):
            continue
        try:
            if path.resolve().is_relative_to(root):
                return True
        except (OSError, RuntimeError):
            return False
    return False


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
        "docs/CHANGELOG.md",
        ".github/CONTRIBUTING.md",
        ".github/CODE_OF_CONDUCT.md",
        ".github/SECURITY.md",
        ".github/SUPPORT.md",
        "dependencies.lock",
        "sdkconfig.defaults",
        "partitions.csv",
        ".github/PULL_REQUEST_TEMPLATE.md",
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


def repo_relative(path: Path) -> str:
    """Repository-relative path with forward slashes, as the documentation uses."""
    return path.relative_to(ROOT).as_posix()


def check_markdown_links(
    files: list[Path], errors: list[str], vendored_roots: tuple[Path, ...] = ()
) -> None:
    for path in files:
        if path.suffix.lower() != ".md" or is_vendored_document(path, vendored_roots):
            continue
        text = path.read_text(encoding="utf-8")
        for raw_target in MARKDOWN_LINK_RE.findall(text):
            target = raw_target.strip().split(maxsplit=1)[0].strip("<>")
            if not target or target.startswith(("#", "http://", "https://", "mailto:")):
                continue
            local = unquote(target.split("#", 1)[0])
            resolved = (ROOT / local.lstrip("/")) if local.startswith("/") else (path.parent / local)
            if local and not resolved.resolve().exists():
                errors.append(f"{repo_relative(path)}: missing link target {target}")


def check_community_document_links(files: list[Path], errors: list[str]) -> None:
    """Keep overview/file-view navigation independent of the render directory."""
    def target_path(raw: str) -> str:
        # Ignore optional link titles, just as the existing local-link check does.
        fields = raw.strip().split(maxsplit=1)
        return fields[0].strip("<>") if fields else ""

    for path in files:
        if path.parent != ROOT / ".github" or path.name not in COMMUNITY_DOCUMENT_NAMES:
            continue
        text = path.read_text(encoding="utf-8")
        peer = (
            path.name.removesuffix(".zh_CN.md") + ".md"
            if path.name.endswith(".zh_CN.md")
            else path.stem + ".zh_CN.md"
        )
        expected = f"/.github/{peer}"
        # Keep the switch on the first nonempty line, outside raw HTML blocks
        # (Markdown inside <p> is not rendered as a link by GitHub).
        first_line = next((line.strip() for line in text.splitlines() if line.strip()), "")
        opening_targets = [target_path(raw) for raw in MARKDOWN_LINK_RE.findall(first_line)]
        if expected not in opening_targets or re.search(r"<!--|</?[A-Za-z][^>]*>", first_line):
            errors.append(
                f"{repo_relative(path)}: language switch must use a Markdown link to {expected} on the first nonempty line, outside HTML"
            )

        # Also reject relative HTML anchors so the original homepage bug cannot
        # hide behind an otherwise valid Markdown language switch.
        targets = (
            MARKDOWN_LINK_RE.findall(text)
            + MARKDOWN_REFERENCE_RE.findall(text)
            + HTML_LINK_RE.findall(text)
        )
        for raw_target in targets:
            target = target_path(raw_target)
            parsed = urlsplit(target)
            if parsed.hostname == "github.com" and parsed.path.lower().startswith(
                ("/folotoy/ai-passport/blob/", "/folotoy/ai-passport/tree/")
            ):
                errors.append(
                    f"{repo_relative(path)}: community document link must not hardcode the upstream repository/ref: {target}"
                )
                continue
            if target.startswith(("#", "http://", "https://", "mailto:")):
                continue
            if not target.startswith("/") or target.startswith("//"):
                errors.append(
                    f"{repo_relative(path)}: community document link must use a repository-root path: {target}"
                )


def check_document_languages(
    files: list[Path], errors: list[str], vendored_roots: tuple[Path, ...] = ()
) -> None:
    """Require an English default and a linked Simplified Chinese peer."""
    markdown = {
        path for path in files
        if path.suffix.lower() == ".md" and not is_vendored_document(path, vendored_roots)
    }

    for path in sorted(markdown):
        name = path.name
        text = path.read_text(encoding="utf-8")
        opening = "\n".join(text.splitlines()[:8])

        if name.endswith(".zh_CN.md"):
            default_name = f"{name[:-len('.zh_CN.md')]}.md"
            default_path = path.with_name(default_name)
            if default_path not in markdown:
                errors.append(
                    f"{repo_relative(path)}: missing English default {default_name}"
                )
            elif default_name not in opening:
                errors.append(
                    f"{repo_relative(path)}: missing top language link to {default_name}"
                )
            continue

        chinese_name = f"{path.stem}.zh_CN.md"
        chinese_path = path.with_name(chinese_name)
        if chinese_path not in markdown:
            errors.append(
                f"{repo_relative(path)}: missing Simplified Chinese peer {chinese_name}"
            )
        elif chinese_name not in opening:
            errors.append(
                f"{repo_relative(path)}: missing top language link to {chinese_name}"
            )

        english_prose = text.replace("简体中文", "")
        match = CJK_RE.search(english_prose)
        if match:
            line = english_prose.count("\n", 0, match.start()) + 1
            errors.append(
                f"{repo_relative(path)}:{line}: default Markdown must use English prose"
            )


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
                    errors.append(f"{repo_relative(path)}:{line_number}: unpinned Docker action {action}")
                continue
            if "@" not in action or not FULL_SHA_RE.fullmatch(action.rsplit("@", 1)[1]):
                errors.append(f"{repo_relative(path)}:{line_number}: action must use a full commit SHA: {action}")


def check_issue_forms(errors: list[str]) -> None:
    issue_dir = ROOT / ".github" / "ISSUE_TEMPLATE"
    for name in ("feature_request.yml", "usage_question.yml"):
        path = issue_dir / name
        if not path.is_file():
            errors.append(f"missing issue form: {repo_relative(path)}")
            continue
        text = path.read_text(encoding="utf-8")
        for field in ("name:", "description:", "body:"):
            if not re.search(rf"(?m)^{re.escape(field)}", text):
                errors.append(f"{repo_relative(path)}: missing top-level {field[:-1]}")


def check_sensitive_content(files: list[Path], errors: list[str]) -> None:
    for path in files:
        text = path.read_text(encoding="utf-8")
        for label, pattern in SECRET_PATTERNS.items():
            if pattern.search(text):
                errors.append(f"{repo_relative(path)}: possible {label}")

        for match in re.finditer(r"https://ai-passport\.folotoy\.cn/trae/\?s=([^&\s)]+)&k=([^\s)]+)", text):
            if "<" not in match.group(1) and "AAAAAA" not in match.group(1):
                errors.append(f"{repo_relative(path)}: possible unsanitized device QR link")


def check_conflict_markers(files: list[Path], errors: list[str]) -> None:
    marker = re.compile(r"(?m)^(<<<<<<< |=======\s*$|>>>>>>> )")
    for path in files:
        if marker.search(path.read_text(encoding="utf-8")):
            errors.append(f"{repo_relative(path)}: unresolved merge conflict marker")


def main() -> int:
    errors: list[str] = []
    vendored_roots = vendored_document_roots(errors)
    files = text_files()
    check_required_files(errors)
    check_markdown_links(files, errors, vendored_roots)
    check_community_document_links(files, errors)
    check_document_languages(files, errors, vendored_roots)
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
