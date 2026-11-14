#!/usr/bin/env bash
set -euo pipefail

# scaffold_skill.sh - Instant scaffolding for token-efficient agent skills
# Usage: ./scaffold_skill.sh <skill-name> [local|global] ["Description of skill"]

NAME="${1:-}"
SCOPE="${2:-local}"
DESC="${3:-Runbook and specialized procedures for $NAME. Use when working on $NAME tasks.}"

if [ -z "$NAME" ]; then
    echo "Usage: $0 <skill-name> [local|global] [\"Description\"]"
    echo "Example: $0 esp32-firmware local \"ESP32 build, flash, and FreeRTOS debugging runbook.\""
    exit 1
fi

# Sanitize skill name (lowercase, letters, numbers, hyphens)
NAME="$(echo "$NAME" | tr '[:upper:]' '[:lower:]' | tr '_' '-' | tr -cd 'a-z0-9-')"

if [ "$SCOPE" = "global" ]; then
    TARGET_DIR="$HOME/.gemini/config/skills/$NAME"
else
    # Find repo root or use current directory
    REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
    TARGET_DIR="$REPO_ROOT/.agents/skills/$NAME"
fi

echo "==> Creating skill directory: $TARGET_DIR"
mkdir -p "$TARGET_DIR/scripts"
mkdir -p "$TARGET_DIR/references"
mkdir -p "$TARGET_DIR/templates"

# Create SKILL.md
cat << EOF > "$TARGET_DIR/SKILL.md"
---
name: $NAME
description: >-
  $DESC
---

# $NAME Skill

This skill provides step-by-step procedures, runbooks, and references for **$NAME**.

---

## ⚡ Ground Rules & Architecture

1. **Core Constraints:** Document non-negotiable architectural rules here.
2. **Decoupled Design:** Ensure components are modular and testable without hardware/services.

---

## 🛠️ Common Workflows

### 1. Build & Test
\`\`\`bash
# Insert primary build/test command
\`\`\`

### 2. Primary Procedure
1. Step 1: Explain action.
2. Step 2: Verification step.

---

## 🔍 Key Symbols & Target Files

| Domain / Task | Target Files | Key Symbols / Notes |
| :--- | :--- | :--- |
| **Primary Logic** | \`src/...\` | Key functions or classes |

---

## 📚 Deep-Dive References
* Detailed specifications: [spec.md](./references/spec.md)
EOF

# Create a sample reference file
cat << EOF > "$TARGET_DIR/references/spec.md"
# $NAME Reference Specification

Place bulky technical details, mathematical formulas, protocol specs, or hardware schematics here to keep SKILL.md token-efficient.
EOF

chmod +x "$TARGET_DIR/SKILL.md"

echo "==> Successfully created skill at $TARGET_DIR"
echo "    - SKILL.md (main instruction file)"
echo "    - references/spec.md (progressive disclosure doc)"
echo "    - scripts/ (place executable helper scripts here)"
echo "    - templates/ (place starter code/config templates here)"

if [ "$SCOPE" = "local" ] && [ -f "$REPO_ROOT/AGENTS.md" ]; then
    echo "==> Remember to link $NAME in $REPO_ROOT/AGENTS.md!"
fi
