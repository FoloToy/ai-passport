"""把 ASSETS.md 渲染成排版好的 HTML，供 Edge 无头模式打印成 PDF。

不用 pandoc 的 PDF 输出：本机没有 LaTeX，中文会缺字。
先转 HTML 再用浏览器打印，字体、表格、分页都可控。
"""
import html
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MD = os.path.join(ROOT, "ASSETS.md")
HTML = os.path.join(ROOT, "_assets_render.html")


def inline(t: str) -> str:
    t = html.escape(t)
    # 行内代码
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    # 粗体
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    # 斜体（避免误伤 ** 已处理）
    t = re.sub(r"(?<![\*\w])\*([^*\n]+)\*(?!\*)", r"<em>\1</em>", t)
    return t


def convert(md: str) -> str:
    lines = md.splitlines()
    out: list[str] = []
    i = 0
    n = len(lines)
    in_code = False
    code_buf: list[str] = []
    list_stack: list[str] = []

    def close_lists():
        while list_stack:
            out.append(f"</{list_stack.pop()}>")

    while i < n:
        line = lines[i]

        # 代码块
        if line.startswith("```"):
            if in_code:
                out.append("<pre><code>" + html.escape("\n".join(code_buf)) + "</code></pre>")
                code_buf = []
                in_code = False
            else:
                close_lists()
                in_code = True
            i += 1
            continue
        if in_code:
            code_buf.append(line)
            i += 1
            continue

        stripped = line.strip()

        # 分隔线
        if re.fullmatch(r"-{3,}", stripped):
            close_lists()
            out.append('<hr class="sep">')
            i += 1
            continue

        # 空行
        if not stripped:
            close_lists()
            i += 1
            continue

        # 表格
        if stripped.startswith("|") and i + 1 < n and re.match(r"^\|[\s:\-|]+\|$", lines[i + 1].strip()):
            close_lists()
            header = [c.strip() for c in stripped.strip("|").split("|")]
            i += 2
            rows = []
            while i < n and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            t = ['<table><thead><tr>']
            for h in header:
                t.append(f"<th>{inline(h)}</th>")
            t.append("</tr></thead><tbody>")
            for r in rows:
                t.append("<tr>")
                for c in r:
                    t.append(f"<td>{inline(c)}</td>")
                t.append("</tr>")
            t.append("</tbody></table>")
            out.append("".join(t))
            continue

        # 引用
        if stripped.startswith(">"):
            close_lists()
            buf = []
            while i < n and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].strip())
                i += 1
            out.append('<blockquote>' + inline(" ".join(buf)) + "</blockquote>")
            continue

        # 标题
        m = re.match(r"^(#{1,6})\s+(.*)$", stripped)
        if m:
            close_lists()
            lvl = len(m.group(1))
            out.append(f"<h{lvl}>{inline(m.group(2))}</h{lvl}>")
            i += 1
            continue

        # 列表
        m = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", line)
        if m:
            indent = len(m.group(1))
            ordered = bool(re.match(r"\d+\.", m.group(2)))
            tag = "ol" if ordered else "ul"
            if not list_stack:
                list_stack.append(tag)
                out.append(f"<{tag}>")
            elif list_stack[-1] != tag:
                while list_stack:
                    out.append(f"</{list_stack.pop()}>")
                list_stack.append(tag)
                out.append(f"<{tag}>")
            # 收集续行
            buf = [m.group(3)]
            i += 1
            while i < n and lines[i].strip() and not re.match(r"^\s*([-*]|\d+\.)\s+", lines[i]) \
                    and not lines[i].strip().startswith(("#", "|", ">", "```")):
                buf.append(lines[i].strip())
                i += 1
            out.append(f"<li>{inline(' '.join(buf))}</li>")
            continue

        # 普通段落
        close_lists()
        buf = [stripped]
        i += 1
        while i < n and lines[i].strip() and not re.match(
                r"^\s*([-*]|\d+\.)\s+|^#{1,6}\s|^\||^>|^```", lines[i]):
            buf.append(lines[i].strip())
            i += 1
        out.append("<p>" + inline(" ".join(buf)) + "</p>")

    if in_code and code_buf:
        out.append("<pre><code>" + html.escape("\n".join(code_buf)) + "</code></pre>")
    close_lists()
    return "\n".join(out)


CSS = """
@page { size: A4; margin: 16mm 14mm 16mm 14mm; }
* { box-sizing: border-box; }
body {
  font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
  font-size: 10.5pt; line-height: 1.65; color: #1f2328; margin: 0;
}
h1 { font-size: 21pt; margin: 0 0 6pt; padding-bottom: 8pt;
     border-bottom: 2.5pt solid #1f6feb; color: #0b1c33; }
h2 { font-size: 14.5pt; margin: 20pt 0 8pt; padding-left: 8pt;
     border-left: 4pt solid #1f6feb; color: #0b1c33;
     page-break-after: avoid; }
h3 { font-size: 12pt; margin: 14pt 0 6pt; color: #1f4d8f; page-break-after: avoid; }
h4 { font-size: 11pt; margin: 11pt 0 4pt; color: #333; page-break-after: avoid; }
p { margin: 5pt 0; }
code {
  font-family: Consolas, "Courier New", monospace; font-size: 9pt;
  background: #f0f3f7; padding: 1pt 3pt; border-radius: 2pt; color: #b3306b;
}
pre {
  background: #f6f8fa; border: 0.6pt solid #d8dee4; border-left: 3pt solid #8b949e;
  border-radius: 3pt; padding: 7pt 9pt; margin: 7pt 0; overflow: visible;
  page-break-inside: avoid;
}
pre code { background: none; padding: 0; color: #24292f; font-size: 8.6pt; line-height: 1.45; }
table {
  border-collapse: collapse; width: 100%; margin: 7pt 0; font-size: 9.4pt;
  page-break-inside: avoid;
}
th { background: #eef4fd; border: 0.6pt solid #c8d4e4; padding: 4pt 6pt;
     text-align: left; font-weight: 600; color: #0b1c33; }
td { border: 0.6pt solid #d8dee4; padding: 4pt 6pt; vertical-align: top; }
tbody tr:nth-child(even) td { background: #fafbfc; }
blockquote {
  margin: 8pt 0; padding: 6pt 10pt; background: #fff8e6;
  border-left: 3pt solid #e3b341; color: #4d3800; font-size: 9.6pt;
}
ul, ol { margin: 5pt 0 5pt 0; padding-left: 20pt; }
li { margin: 2.5pt 0; }
hr.sep { border: none; border-top: 0.8pt dashed #c8d4e4; margin: 14pt 0; }
strong { color: #0b1c33; }
"""


def main() -> int:
    with open(MD, encoding="utf-8") as f:
        md = f.read()
    body = convert(md)
    doc = ("<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
           f"<title>AI PASSPORT × WorkBuddy 资产梳理</title><style>{CSS}</style>"
           f"</head><body>{body}</body></html>")
    with open(HTML, "w", encoding="utf-8") as f:
        f.write(doc)
    print(f"HTML: {HTML}  ({len(doc)} 字符)")

    # 用独立的临时 profile：直接复用默认 profile 会和正在运行的浏览器抢锁，
    # 表现为无头进程挂住不退（Edge 实测 180s 超时）。
    import tempfile

    browsers = [
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    ]
    desk = os.path.join(os.path.expanduser("~"), "Desktop")
    pdf = os.path.join(desk, "AI_PASSPORT_资产梳理.pdf")
    if os.path.exists(pdf):
        os.remove(pdf)

    profile = tempfile.mkdtemp(prefix="wbpdf-")
    for exe in browsers:
        if not os.path.exists(exe):
            continue
        args = [
            exe, "--headless=new", "--disable-gpu", "--no-first-run",
            "--no-default-browser-check", "--disable-extensions",
            f"--user-data-dir={profile}",
            "--no-pdf-header-footer",
            "--virtual-time-budget=8000",
            f"--print-to-pdf={pdf}",
            "file:///" + HTML.replace("\\", "/"),
        ]
        try:
            subprocess.run(args, capture_output=True, timeout=90)
        except subprocess.TimeoutExpired:
            print(f"  {os.path.basename(exe)} 超时，换下一个")
            continue
        if os.path.exists(pdf) and os.path.getsize(pdf) > 5000:
            print(f"PDF: {pdf}  {os.path.getsize(pdf)/1024:.1f} KB  ({os.path.basename(exe)})")
            return 0
        print(f"  {os.path.basename(exe)} 未产出，换下一个")
    print("PDF 生成失败")
    return 1


sys.exit(main())
