#!/usr/bin/env bash
# Export a clean source tree without local build state or private configuration.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output=${1:-"$project_dir/build-container/github/mic-overlay"}
python3 - "$project_dir" "$output" <<'PY'
from pathlib import Path
import shutil
import sys

source = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
if output.exists():
    sys.exit(f"Destination already exists: {output}. Choose a new directory.")
if output == source or source.is_relative_to(output):
    sys.exit("Destination must not replace the project or one of its parents.")

root_files = (".gitignore", ".gitattributes", "CMakeLists.txt", "README.md",
              "CONTRIBUTING.md", "LICENSE", "TODO.md")
source_dirs = ("assets", "cmake", "config", "containers", "docs", "packaging",
               "qml", "scripts", "src", "tests")
private_names = {".git", ".agents", ".codex", ".artifacts", "__pycache__",
                 ".idea", ".vscode", "config.local.ini"}
allowed_suffixes = {".cpp", ".h", ".qml", ".svg", ".cmake", ".md", ".ini",
                    ".example", ".py", ".sh", ".in", ".png", ".jpg", ".webp", ".gif"}
files = [source / name for name in root_files]
for directory in source_dirs:
    for path in (source / directory).rglob("*"):
        relative = path.relative_to(source)
        if any(part in private_names or part.startswith(".env") for part in relative.parts):
            continue
        if path.is_symlink():
            sys.exit(f"Source symlinks are not exported: {relative}")
        if path.is_file() and (path.suffix in allowed_suffixes
                               or path.name == "PKGBUILD"
                               or path.name.startswith("Containerfile")):
            files.append(path)
for path in files:
    if not path.is_file() or path.is_symlink():
        sys.exit(f"Expected a regular source file: {path.relative_to(source)}")

output.mkdir(parents=True)
for path in sorted(files):
    destination = output / path.relative_to(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)
print(f"GitHub source tree: {output}")
print(f"Exported {len(files)} files. Build outputs and local configuration were preserved in the original project.")
PY
