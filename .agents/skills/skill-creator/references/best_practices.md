# Best Practices for Token-Efficient Agent Skills

## 1. The Token Budget & Progressive Disclosure

In agentic AI frameworks (Antigravity, Gemini, Claude Code), context window management is paramount. Loading massive manuals into the system prompt wastes tokens and degrades instruction-following performance.

Skills solve this using **Progressive Disclosure**:

```
[ Primary Turn Context ]
         |
    Reads ONLY:
    - name: <skill-name>
    - description: <when to use...>  (< 50 tokens)
         |
         v
[ Does the user request match the skill? ]
    /                                \
 (NO)                                (YES)
   v                                   v
[ Do nothing. ]               [ Reads SKILL.md ] (~500 - 1500 tokens)
                                       |
                              [ Needs deep spec? ]
                               /               \
                            (NO)               (YES)
                              v                  v
                          [ Execute ]   [ Reads references/*.md ]
```

### The 3 Golden Rules of Size
1. **Description Budget:** $\le 300$ characters. Must clearly answer **WHAT** it does and **WHEN** the agent must use it.
2. **Main `SKILL.md` Budget:** Under 150 lines. Keep it dense with commands, tables, and recipes.
3. **Bulky Data Offloading:** Move KaTeX equations, raw API dumps, hardware pin tables, and long code samples to `references/` or `templates/`.

---

## 2. Writing High-Signal Trigger Descriptions

The agent decides whether to activate a skill purely from the `description` frontmatter field.

### ❌ Weak Descriptions (Causes False Negatives / False Positives)
* `"Helper for working on the synthesizer."` *(Too vague; when should it trigger?)*
* `"Comprehensive documentation about all aspects of the DSP engine, sound design, filtering, hardware, and simulator."` *(Too broad; overlaps with other skills).*
* `"Use this skill if you want to write good code."` *(Generic advice).*

### ✅ High-Signal Descriptions (High Accuracy)
* `"RP2040 firmware build, flash, and hardware integration runbook for Phase Box. Use when compiling the Raspberry Pi Pico UF2 binary, modifying GPIO pinouts, verifying dual-core execution, or debugging TinyUSB MIDI."`
* `"Desktop simulator and CoreMIDI integration runbook for Phase Box. Use when developing, debugging, or running the native Raylib desktop simulator, modifying stompbox layout geometry, or adjusting CoreMIDI routing."`

---

## 3. Designing Executable Helper Scripts (`scripts/`)

Agents waste dozens of tool calls trying to guess environment variables, PATH settings, or compiler locations.
* **Make scripts self-contained:** Have your `scripts/*.sh` check for known tool paths and set defaults automatically.
* **Always use `set -euo pipefail`:** Prevents silent failures in the middle of compilation chains.
* **Auto-detect repo root:**
  ```bash
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || cd "$SCRIPT_DIR/.." && pwd)"
  ```

---

## 4. Dual-Agent Parity (Claude + Gemini)

1. Keep all skill files in standard GitHub-Flavored Markdown.
2. In your repo root, symlink `CLAUDE.md -> AGENTS.md`.
3. In `AGENTS.md`, maintain an index table linking to every `.agents/skills/<name>/SKILL.md`.
4. Claude automatically reads `CLAUDE.md` and follows the links; Gemini automatically parses `.agents/skills/`.
