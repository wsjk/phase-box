---
name: skill-creator
description: >-
  Meta-skill and operational runbook for designing, scaffolding, and maintaining token-efficient agent skills.
  Use whenever creating new skills, optimizing existing skills for token conservation, establishing dual-agent
  parity (Claude and Gemini), or scaffolding procedural runbooks.
---

# Skill Creator: Agent Meta-Skill

This skill teaches agents how to design, scaffold, and maintain **token-efficient, high-precision agent skills** for both Claude and Gemini.

---

## ⚡ The 5 Golden Rules of Efficient Skills

1. **Progressive Disclosure:** Keep `SKILL.md` under 150 lines. Move large reference specs, tables, and math to `references/*.md`.
2. **Explicit Triggering:** Frontmatter `description` must state **WHAT** it does and **WHEN** it activates (using third-person: *"Use when..."*).
3. **Executable Automation:** Encapsulate brittle shell commands, paths, and environment variable discovery into `scripts/*.sh`.
4. **No Fluff:** Never include general coding advice ("write clean code"). Focus strictly on project-specific rules, invariants, and recipes.
5. **Dual-Agent Parity:** Skills must work seamlessly for both Claude Code (`CLAUDE.md`) and Gemini/Antigravity (`AGENTS.md`).

---

## 🛠️ Step-by-Step Skill Creation Protocol

When tasked with creating a new skill, follow this 5-step workflow:

### Step 1: Scaffold Directory & Boilerplate
Run the built-in scaffolding script:
```bash
# For a project-specific skill (.agents/skills/<name>):
./.agents/skills/skill-creator/scripts/scaffold_skill.sh <skill-name> local "Description of when to use"

# For a machine-wide global skill (~/.gemini/config/skills/<name>):
./.agents/skills/skill-creator/scripts/scaffold_skill.sh <skill-name> global "Description of when to use"
```

This creates the standard layout:
```text
skills/<skill-name>/
├── SKILL.md            # Required: Dense actionable runbook with YAML frontmatter
├── references/         # Deep-dive specs, API dumps, formulas (progressive disclosure)
├── scripts/            # Self-contained helper scripts with auto-path discovery
└── templates/          # Copy-pasteable boilerplate files or config starters
```

### Step 2: Write Frontmatter with High-Signal Triggers
Ensure the YAML frontmatter in `SKILL.md` is strictly formatted:
```yaml
---
name: my-skill-name
description: >-
  Brief 1-3 sentence summary. Use when doing <task X>, working on <file Y>,
  or troubleshooting <error Z>.
---
```

### Step 3: Write Actionable Recipes in `SKILL.md`
Organize `SKILL.md` with high-density sections:
* **Ground Rules:** Architectural constraints (e.g. *"Never do blocking I/O on Core 0"*).
* **Common Workflows:** Copy-pasteable build/test commands.
* **Step-by-Step Recipes:** Discrete numbered steps for common modifications.
* **File Map:** Table mapping tasks $\to$ files $\to$ key symbols.

### Step 4: Offload Bulky Content
* Math formulas & detailed specs $\to$ `references/spec.md`.
* Starter code / configs $\to$ `templates/`.
* Complex build commands $\to$ `scripts/build.sh`.

### Step 5: Register for Dual-Agent Discovery
1. **Update `AGENTS.md`:** Add an entry in the skills table.
2. **Ensure `CLAUDE.md` is symlinked:** `ln -s AGENTS.md CLAUDE.md`.
3. *(Optional for global skills)*: Reference in `~/.claude/CLAUDE.md`.

---

## 📋 Quality Checklist Before Committing

- [ ] `name` is lowercase, hyphenated, and matches folder name.
- [ ] `description` specifies explicit triggers ("Use when...").
- [ ] `SKILL.md` is under 150 lines.
- [ ] All shell scripts have `set -euo pipefail` and `chmod +x`.
- [ ] Relative links to `references/` or `scripts/` work.
- [ ] Registered in `AGENTS.md` and accessible to Claude via `CLAUDE.md`.
- [ ] Verified by running the skill's build/test workflow.

For architectural patterns and triggering guidelines, see [references/best_practices.md](./references/best_practices.md).
