#!/usr/bin/env python3
"""Update configuration dependency only when the command/flags change."""
import sys
from pathlib import Path
p = Path(sys.argv[1])
value = "\n".join(sys.argv[2:]) + "\n"
p.parent.mkdir(parents=True, exist_ok=True)
if not p.exists() or p.read_text() != value:
    p.write_text(value)
