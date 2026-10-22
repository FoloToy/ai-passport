"""断链自检：扫描指定目录下 md 文件里的 [[wikilink]]，检查目标是否存在。

Obsidian 的 wikilink 可以带显示名（[[目标|显示]]），也可以只写目标。
目标可能带 .md 后缀也可能不带；标题（#）和块引用（^）要剥掉。
"""
import os
import re
import sys

VAULT = r"G:\AI WORKSPACE\claudecode应用"
CHECK_DIRS = [
    os.path.join(VAULT, "wiki"),
    os.path.join(VAULT, "40-工作流案例", "AI-PASSPORT硬件伴侣"),
]

# 全库所有文件名（不含后缀）与相对路径
# 注意：附件（pdf/png/mp4…）也要收进 names —— Obsidian 的 ![[x.pdf]] 是合法嵌入，
# 只收集 .md 会把它误报成断链。
names = set()
for root, dirs, files in os.walk(VAULT):
    if any(x in root for x in (".obsidian", ".trash", ".smart-env", "node_modules")):
        continue
    for f in files:
        names.add(f)                                    # 带后缀：附件、图片
        rel = os.path.relpath(os.path.join(root, f), VAULT).replace("\\", "/")
        names.add(rel)
        if f.endswith(".md"):
            names.add(f[:-3])                           # 不带后缀：[[页面名]]
            names.add(rel[:-3])

LINK = re.compile(r"\[\[([^\]]+)\]\]")

total = 0
broken = []
for d in CHECK_DIRS:
    for root, _dirs, files in os.walk(d):
        for f in files:
            if not f.endswith(".md"):
                continue
            p = os.path.join(root, f)
            with open(p, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            for m in LINK.finditer(text):
                target = m.group(1).split("|")[0].strip()
                target = target.split("#")[0].split("^")[0].strip()
                if not target:
                    continue
                total += 1
                if target not in names and target + "" not in names:
                    broken.append((os.path.relpath(p, VAULT), target))

print(f"检查目录: {len(CHECK_DIRS)} 个")
print(f"wikilink 总数: {total}")
print(f"断链: {len(broken)}")
for src, tgt in broken:
    print(f"  ✗ {src}  ->  [[{tgt}]]")

# 顺带报告这些新页面的规模
print()
print("新增文件：")
for d in CHECK_DIRS:
    for root, _dirs, files in os.walk(d):
        for f in sorted(files):
            if f.endswith((".md", ".pdf")):
                p = os.path.join(root, f)
                if "AI-PASSPORT" in f or "项目总览" in f or "硬件终端" in f:
                    print(f"  {os.path.getsize(p):>8d} B  {os.path.relpath(p, VAULT)}")

sys.exit(1 if broken else 0)
