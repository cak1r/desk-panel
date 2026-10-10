"""
config.py is gitignored (it holds the broker password), so tests load
config.example.py under the name `config` for modules that import it.
"""

import importlib.util
import sys
from pathlib import Path

if "config" not in sys.modules:
    _example = Path(__file__).resolve().parent.parent / "config.example.py"
    _spec = importlib.util.spec_from_file_location("config", _example)
    _config = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(_config)
    sys.modules["config"] = _config
