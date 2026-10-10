"""
Startup check that the user's config.py has every setting config.example.py
defines -- config.py is gitignored, so it silently falls behind whenever a
new feature adds settings.
"""

import ast
from pathlib import Path

EXAMPLE_PATH = Path(__file__).with_name("config.example.py")


def example_setting_names(example_path: Path = EXAMPLE_PATH) -> list[str]:
    tree = ast.parse(example_path.read_text(encoding="utf-8"))
    names = []
    for node in tree.body:
        targets = node.targets if isinstance(node, ast.Assign) else [node.target] if isinstance(node, ast.AnnAssign) else []
        for t in targets:
            if isinstance(t, ast.Name) and t.id.isupper():
                names.append(t.id)
    return names


def missing_settings(config_module, example_path: Path = EXAMPLE_PATH) -> list[str]:
    return [n for n in example_setting_names(example_path) if not hasattr(config_module, n)]
