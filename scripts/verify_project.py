#!/usr/bin/env python3
"""Static governance integrity checks: no system writes, safe for CI."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
required=[
 'AGENTS.md','LICENSE','ATTRIBUTION.md','THIRD-PARTY-NOTICES.md','zam.toml',
 'src/main.zm','src/memory.zm','src/policy.zm','native/async_privileged.c','native/memreduct_helper.c',
 'docs/PRD.md','docs/ARCHITECTURE.md','docs/SECURITY.md','docs/TEST_PLAN.md','docs/ROADMAP.md',
 'docs/AI_COLLABORATION.md','docs/GITHUB_BOOTSTRAP.md',
 'docs/ADR/0001-zamak-platform.md','docs/ADR/0002-privileged-boundary.md','docs/ADR/0003-memory-and-collab.md',
 'memory/LONG_TERM.md','memory/MEDIUM_TERM.md','memory/SHORT_TERM.md','memory/SESSION_LOG.md',
 'prompts/CODEX-P0-BOOTSTRAP.md','prompts/CODEX-P1.md','prompts/CHATGPT-REVIEW.md',
 '.github/pull_request_template.md','.github/workflows/test.yml']
missing=[p for p in required if not (root/p).is_file()]
if missing:
    raise SystemExit('Missing required files: '+', '.join(missing))
if 'GNU GENERAL PUBLIC LICENSE' not in (root/'LICENSE').read_text():
    raise SystemExit('License content does not look like GPL')
if 'name = "memreduct_zamak"' not in (root/'zam.toml').read_text():
    raise SystemExit('Unexpected Zamak project identifier')
print(f'PASS: {len(required)} required project and governance files present; GPL and manifest sanity checked')
