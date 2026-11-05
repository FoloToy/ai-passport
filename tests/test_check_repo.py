#!/usr/bin/env python3
"""Host tests for documentation exemptions and community-document navigation."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_repo", ROOT / "tools" / "check_repo.py")
assert SPEC and SPEC.loader
CHECKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKS)

UPSTREAM_DOC = "# Upstream\n\n[Guide](upstream-only.md)\n"


class VendoredDocumentationTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="ai-passport-doc-tests-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        root_patch = patch.object(CHECKS, "ROOT", self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)
        self.vendor = self.root / "components" / "vendor_audio"
        self.vendor.mkdir(parents=True)
        config_patch = patch.object(CHECKS, "VENDORED_DOC_ROOTS", ("components/vendor_audio",))
        config_patch.start()
        self.addCleanup(config_patch.stop)

    def document(self, name: str, content: str = UPSTREAM_DOC) -> Path:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def roots(self) -> tuple[Path, ...]:
        errors: list[str] = []
        roots = CHECKS.vendored_document_roots(errors)
        self.assertEqual(errors, [])
        return roots

    def symlink(self, link: Path, target: Path, *, directory: bool = False) -> None:
        try:
            link.symlink_to(target, target_is_directory=directory)
        except NotImplementedError:
            self.skipTest("This platform does not support symlinks")
        except OSError as error:
            if getattr(error, "winerror", None) != 1314:
                raise
            self.skipTest("Windows symlink tests require Developer Mode or elevation")

    def document_errors(self, files: list[Path]) -> list[str]:
        errors: list[str] = []
        roots = self.roots()
        CHECKS.check_markdown_links(files, errors, roots)
        CHECKS.check_document_languages(files, errors, roots)
        return errors

    def test_empty_registry_keeps_upstream_documents_checked(self) -> None:
        path = self.document("components/vendor_audio/README.md")
        with patch.object(CHECKS, "VENDORED_DOC_ROOTS", ()):
            errors = self.document_errors([path])
        self.assertEqual(len(errors), 2)
        self.assertTrue(any("missing link target" in error for error in errors))
        self.assertTrue(any("missing Simplified Chinese peer" in error for error in errors))

    def test_explicit_root_exempts_nested_links_and_language_rules(self) -> None:
        files = [
            self.document("components/vendor_audio/README.md"),
            self.document("components/vendor_audio/docs/manual.md", "# 上游文档\n"),
            self.document("components/vendor_audio/docs/other.zh_CN.md", "# 上游中文\n"),
        ]
        self.assertEqual(self.document_errors(files), [])

    def test_first_party_and_similar_prefixes_remain_checked(self) -> None:
        for name in (
            "docs/README.md",
            "components/vendor_audio_extra/README.md",
            "components/vendor_audio.md",
            "components/other/vendor_audio/README.md",
            "components/vendor_audio2/README.md",
        ):
            with self.subTest(name=name):
                errors = self.document_errors([self.document(name)])
                self.assertEqual(len(errors), 2)
                self.assertTrue(all(name in error for error in errors))

    def test_valid_first_party_pair_and_links_still_pass(self) -> None:
        files = [
            self.document("docs/guide.md", "[简体中文](guide.zh_CN.md)\n# Guide\n"),
            self.document("docs/guide.zh_CN.md", "[English](guide.md)\n# 指南\n"),
        ]
        self.assertEqual(self.document_errors(files), [])

    def test_invalid_root_registrations_fail_closed(self) -> None:
        file_path = self.document("components/vendor_audio/README.md")
        for name in (
            "", ".", "..", "/", str(self.vendor), "../outside",
            "components/..", "components/./vendor_audio", "components//vendor_audio",
            "components/vendor_audio/", "components\\vendor_audio", "C:/vendor_audio",
            "components/missing", str(file_path.relative_to(self.root)), 1,
        ):
            with self.subTest(name=name), patch.object(CHECKS, "VENDORED_DOC_ROOTS", (name,)):
                errors: list[str] = []
                self.assertEqual(CHECKS.vendored_document_roots(errors), ())
                self.assertEqual(len(errors), 1)
                self.assertIn("invalid VENDORED_DOC_ROOTS entry", errors[0])

    def test_registered_root_cannot_be_a_symlink_or_use_symlink_parent(self) -> None:
        self.symlink(self.root / "alias", self.root / "components", directory=True)
        self.symlink(self.root / "outside", self.root.parent, directory=True)
        for name in ("alias/vendor_audio", "outside"):
            with self.subTest(name=name), patch.object(CHECKS, "VENDORED_DOC_ROOTS", (name,)):
                errors: list[str] = []
                self.assertEqual(CHECKS.vendored_document_roots(errors), ())
                self.assertTrue(errors)

    def test_vendored_symlinks_into_first_party_remain_checked(self) -> None:
        target = self.document("docs/guide.md")
        linked_file = self.vendor / "guide.md"
        self.symlink(linked_file, target)
        linked_directory = self.vendor / "project_docs"
        self.symlink(linked_directory, target.parent, directory=True)
        for path in (linked_file, linked_directory / "guide.md"):
            with self.subTest(path=path):
                self.assertFalse(CHECKS.is_vendored_document(path, self.roots()))
                self.assertEqual(len(self.document_errors([path])), 2)

    def test_symlink_outside_repository_is_not_exempt_and_does_not_crash(self) -> None:
        with tempfile.TemporaryDirectory(prefix="ai-passport-doc-external-") as external:
            target = Path(external) / "guide.md"
            target.write_text(UPSTREAM_DOC, encoding="utf-8")
            link = self.vendor / "guide.md"
            self.symlink(link, target)
            self.assertEqual(len(self.document_errors([link])), 2)

    def test_first_party_symlink_into_vendored_directory_remains_checked(self) -> None:
        target = self.document("components/vendor_audio/README.md")
        link = self.root / "README.md"
        self.symlink(link, target)
        errors = self.document_errors([link])
        self.assertEqual(len(errors), 1)
        self.assertIn("missing link target", errors[0])

    def test_main_keeps_security_and_conflict_checks_for_vendored_text(self) -> None:
        # Assemble deliberately synthetic fixtures so the test source itself is clean.
        samples = {
            "token.md": "ghp_" + "x" * 24,
            "access.md": "AKIA" + "0" * 16,
            "key.md": "-----BEGIN " + "PRIVATE KEY-----",
            "device.md": "https://ai-passport.folotoy.cn/" + "trae/?s=" + "synthetic&k=synthetic",
            "conflict.md": "<" * 7 + " HEAD\n",
        }
        files = [
            self.document("components/vendor_audio/" + name, UPSTREAM_DOC + sample)
            for name, sample in samples.items()
        ]
        with (
            patch.object(CHECKS, "git_files", return_value=files),
            patch.object(CHECKS, "check_required_files"),
            patch.object(CHECKS, "check_action_pins"),
            patch.object(CHECKS, "check_issue_forms"),
            contextlib.redirect_stderr(io.StringIO()) as output,
        ):
            self.assertEqual(CHECKS.text_files(), files)
            self.assertEqual(CHECKS.main(), 1)
        errors = output.getvalue().splitlines()
        self.assertEqual(len(errors), 5)
        for expected in (
            "possible GitHub token", "possible AWS access key", "possible private key",
            "possible unsanitized device QR link", "unresolved merge conflict marker",
        ):
            self.assertTrue(any(expected in error for error in errors), errors)


class RootMarkdownLanguageTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="ai-passport-root-doc-tests-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        root_patch = patch.object(CHECKS, "ROOT", self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)

    def document(self, name: str, content: str) -> Path:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def language_errors(self, files: list[Path]) -> list[str]:
        errors: list[str] = []
        CHECKS.check_document_languages(files, errors)
        CHECKS.check_root_language_links(files, errors)
        CHECKS.check_root_readme_links(files, errors)
        return errors

    def test_root_readme_needs_no_language_pair(self) -> None:
        readme = self.document("README.md", "# 项目说明\n\n没有英文或中文配对。\n")
        self.assertEqual(self.language_errors([readme]), [])

    def test_root_agents_requires_english_and_chinese_peer(self) -> None:
        agents = self.document("AGENTS.md", "# 仓库规范\n")
        errors = self.language_errors([agents])
        self.assertTrue(any("missing Simplified Chinese peer AGENTS.zh_CN.md" in error for error in errors))
        self.assertTrue(any("default Markdown must use English prose" in error for error in errors))

        files = [
            self.document(
                "AGENTS.md",
                '<a href="AGENTS.zh_CN.md">简体中文</a>\n# Repository Guidelines\n',
            ),
            self.document(
                "AGENTS.zh_CN.md",
                '<a href="AGENTS.md">English</a>\n# AI 仓库规范\n',
            ),
        ]
        self.assertEqual(self.language_errors(files), [])

    def test_root_multilingual_links_must_be_reciprocal(self) -> None:
        chinese = self.document("README.md", '<a href="README.en_US.md">English</a>\n')
        english = self.document("README.en_US.md", '<a href="README.md">简体中文</a>\n')
        self.assertEqual(self.language_errors([chinese, english]), [])

        english.write_text("# English\n", encoding="utf-8")
        errors = self.language_errors([chinese, english])
        self.assertEqual(errors, ["README.md: language link to README.en_US.md must be reciprocal"])

    def test_root_markdown_allows_multiple_language_versions(self) -> None:
        files = [
            self.document(
                "README.md",
                '<a href="README.en_US.md">English</a> · <a href="README.zh_CN.md">简体中文</a>\n',
            ),
            self.document(
                "README.en_US.md",
                '<a href="README.md">简体中文</a> · <a href="README.zh_CN.md">简体中文</a>\n',
            ),
            self.document(
                "README.zh_CN.md",
                '<a href="README.md">简体中文</a> · <a href="README.en_US.md">English</a>\n',
            ),
        ]
        self.assertEqual(self.language_errors(files), [])

    def test_root_readme_accepts_supported_language_sets(self) -> None:
        sets = [
            {"README.md": "# 项目\n"},
            {
                "README.md": '<a href="README.zh_CN.md">简体中文</a>\n',
                "README.zh_CN.md": '<a href="README.md">简体中文</a>\n',
            },
            {
                "README.md": '<a href="README.en_US.md">English</a>\n',
                "README.en_US.md": '<a href="README.md">简体中文</a>\n',
            },
            {
                "README.md": '<a href="README.zh_CN.md">简体中文</a> · <a href="README.en_US.md">English</a>\n',
                "README.zh_CN.md": '<a href="README.md">简体中文</a>\n',
                "README.en_US.md": '<a href="README.md">简体中文</a>\n',
            },
        ]
        for content in sets:
            with self.subTest(files=sorted(content)):
                files = [self.document(name, text) for name, text in content.items()]
                self.assertEqual(self.language_errors(files), [])

    def test_root_readme_versions_require_default_links(self) -> None:
        files = [
            self.document("README.md", "# 项目\n"),
            self.document("README.en_US.md", "# Project\n"),
        ]
        self.assertEqual(
            self.language_errors(files),
            ["README.md: missing top language link to README.en_US.md"],
        )

    def test_root_readme_version_requires_default(self) -> None:
        lone_translation = self.document("README.ja_JP.md", "# プロジェクト\n")
        self.assertEqual(
            self.language_errors([lone_translation]),
            ["README.md: root README is required when language versions exist"],
        )

    def test_root_missing_html_language_target_is_rejected(self) -> None:
        page = self.document("README.md", '<a href="README.en_US.md">English</a>\n')
        errors = self.language_errors([page])
        self.assertEqual(errors, ["README.md: missing language link target README.en_US.md"])


class CommunityDocumentLinksTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="ai-passport-community-doc-tests-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        root_patch = patch.object(CHECKS, "ROOT", self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)

    def document(self, name: str, content: str) -> Path:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def errors(self, files: list[Path]) -> list[str]:
        errors: list[str] = []
        CHECKS.check_community_document_links(files, errors)
        return errors

    def test_all_community_language_pairs_use_existing_root_targets(self) -> None:
        files = []
        for name in sorted(CHECKS.COMMUNITY_DOCUMENT_NAMES):
            peer = (
                name.removesuffix(".zh_CN.md") + ".md"
                if name.endswith(".zh_CN.md") else name.removesuffix(".md") + ".zh_CN.md"
            )
            files.append(self.document(f".github/{name}", f"[Language](/.github/{peer})\n"))
        errors = self.errors(files)
        CHECKS.check_markdown_links(files, errors)
        CHECKS.check_document_languages(files, errors)
        self.assertEqual(errors, [])

    def test_original_html_switch_is_rejected_despite_existing_peer(self) -> None:
        page = self.document(
            ".github/CODE_OF_CONDUCT.md",
            '<p align="right"><a href="CODE_OF_CONDUCT.zh_CN.md">Language</a></p>\n',
        )
        self.document(".github/CODE_OF_CONDUCT.zh_CN.md", "# Translation\n")
        errors = self.errors([page])
        self.assertTrue(any("language switch must use a Markdown link" in error for error in errors))
        self.assertTrue(any("repository-root path" in error for error in errors))

    def test_wrong_directory_and_fixed_upstream_switches_are_rejected(self) -> None:
        for target in (
            "CODE_OF_CONDUCT.zh_CN.md",
            "/CODE_OF_CONDUCT.zh_CN.md",
            "https://github.com/FoloToy/ai-passport/blob/main/.github/CODE_OF_CONDUCT.zh_CN.md",
        ):
            with self.subTest(target=target):
                page = self.document(".github/CODE_OF_CONDUCT.md", f"[Language]({target})\n")
                self.assertTrue(any("language switch" in error for error in self.errors([page])))

    def test_root_target_still_requires_a_real_file(self) -> None:
        page = self.document(
            ".github/SUPPORT.md", "[Language](/.github/SUPPORT.zh_CN.md)\n"
        )
        errors = self.errors([page])
        CHECKS.check_markdown_links([page], errors)
        self.assertEqual(len(errors), 1)
        self.assertIn("missing link target", errors[0])

    def test_language_switch_allows_markdown_titles_and_angle_targets(self) -> None:
        for target in (
            '/.github/SUPPORT.zh_CN.md "Chinese"',
            '</.github/SUPPORT.zh_CN.md> "Chinese"',
        ):
            with self.subTest(target=target):
                page = self.document(".github/SUPPORT.md", f"\nEnglish | [Language]({target})\n")
                self.assertEqual(self.errors([page]), [])

    def test_markdown_switch_inside_raw_html_is_rejected(self) -> None:
        for content in (
            '<p align="right">\n[Language](/.github/SUPPORT.zh_CN.md)\n</p>\n',
            '<p>[Language](/.github/SUPPORT.zh_CN.md)</p>\n',
            '<!-- [Language](/.github/SUPPORT.zh_CN.md) -->\n',
        ):
            with self.subTest(content=content):
                page = self.document(".github/SUPPORT.md", content)
                self.assertTrue(any("language switch" in error for error in self.errors([page])))

    def test_relative_body_links_are_rejected_for_markdown_and_html(self) -> None:
        for link in (
            "[Security](SECURITY.md)",
            "[Guide](../docs/README.md)",
            '<a href="SECURITY.md">Security</a>',
            "<a HREF='SECURITY.md'>Security</a>",
            "[Security](//example.com/SECURITY.md)",
            "[Security][security]\n\n[security]: SECURITY.md",
            '[Security][security]\n\n[security]: <SECURITY.md> "Policy"',
        ):
            with self.subTest(link=link):
                page = self.document(
                    ".github/SUPPORT.md", "[Language](/.github/SUPPORT.zh_CN.md)\n\n" + link
                )
                self.assertTrue(any("repository-root path" in error for error in self.errors([page])))

    def test_root_links_external_links_and_fragments_are_allowed(self) -> None:
        page = self.document(
            ".github/CONTRIBUTING.md",
            "[Language](/.github/CONTRIBUTING.zh_CN.md)\n\n"
            "[Guide](/docs/README.md) [Rules](/AGENTS.md) [License](/LICENSE)\n"
            "[Security](/.github/SECURITY.md#reporting-a-vulnerability)\n"
            "[Site](https://example.com) [Mail](mailto:security@example.com) [Top](#top)\n",
        )
        self.assertEqual(self.errors([page]), [])

    def test_root_reference_links_and_external_attribution_are_allowed(self) -> None:
        page = self.document(
            ".github/CODE_OF_CONDUCT.md",
            "[Language](/.github/CODE_OF_CONDUCT.zh_CN.md)\n\n"
            "[Security][security] [Upstream][attribution]\n\n"
            '[security]: </.github/SECURITY.md> "Security policy"\n'
            "[attribution]: https://github.com/mozilla/diversity\n",
        )
        self.assertEqual(self.errors([page]), [])

    def test_body_cannot_pin_internal_docs_to_upstream_but_reporting_is_allowed(self) -> None:
        for link in (
            "[Rules](https://github.com/FoloToy/ai-passport/blob/main/AGENTS.md)",
            "[Docs](https://github.com/FoloToy/ai-passport/tree/main/docs)",
            "[Rules][rules]\n[rules]: https://github.com/FoloToy/ai-passport/blob/main/AGENTS.md",
            '<a href="https://github.com/FoloToy/ai-passport/blob/main/AGENTS.md">Rules</a>',
        ):
            with self.subTest(link=link):
                page = self.document(
                    ".github/SUPPORT.md", "[Language](/.github/SUPPORT.zh_CN.md)\n\n" + link
                )
                self.assertTrue(any("must not hardcode" in error for error in self.errors([page])))
        page = self.document(
            ".github/SECURITY.md",
            "[Language](/.github/SECURITY.zh_CN.md)\n\n"
            "[Report](https://github.com/FoloToy/ai-passport/security/advisories/new)\n",
        )
        self.assertEqual(self.errors([page]), [])

    def test_other_documents_and_pr_template_keep_their_own_context(self) -> None:
        files = [
            self.document("docs/README.md", "[Language](README.zh_CN.md)\n"),
            self.document(".github/PULL_REQUEST_TEMPLATE.md", "[Language](PULL_REQUEST_TEMPLATE.zh_CN.md)\n"),
        ]
        self.assertEqual(self.errors(files), [])

    def test_main_runs_community_link_guard(self) -> None:
        page = self.document(
            ".github/SECURITY.md", '<a href="SECURITY.zh_CN.md">Language</a>\n'
        )
        with (
            patch.object(CHECKS, "git_files", return_value=[page]),
            patch.object(CHECKS, "check_required_files"),
            patch.object(CHECKS, "check_action_pins"),
            patch.object(CHECKS, "check_issue_forms"),
            contextlib.redirect_stderr(io.StringIO()) as output,
        ):
            self.assertEqual(CHECKS.main(), 1)
        self.assertIn("language switch must use a Markdown link", output.getvalue())


if __name__ == "__main__":
    unittest.main()
