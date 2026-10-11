---
trigger: always_on
---

# Rules

## Core Principle

**If the user asks a question, answer the question first.**

Do **not** immediately:
- Modify files
- Create or edit code
- Refactor code
- Generate patches
- Apply changes
- Continue implementing work

Answer the question completely, then **stop and wait for the user's approval** before making any modifications.

---

## Do Not Assume Implementation

A question is **not** an implementation request.

Examples:

**User:** "Why is this function slow?"
- ✅ Explain the cause.
- ❌ Do not optimize the function.

**User:** "Is this architecture a good idea?"
- ✅ Evaluate the architecture and discuss tradeoffs.
- ❌ Do not start restructuring files.

**User:** "Would React Query help here?"
- ✅ Explain when it helps and its pros/cons.
- ❌ Do not install packages or rewrite code.

---

## Wait for Explicit Approval

Only modify files when the user clearly requests implementation.

Examples of explicit approval include:
- "Implement it."
- "Go ahead."
- "Update the files."
- "Write the code."
- "Apply the changes."
- "Make those edits."

Until then, stay in discussion mode.

---

## Discussion Mode

While discussing:
- Answer every question directly.
- Explain reasoning and tradeoffs.
- Suggest approaches if helpful.
- Ask clarifying questions when necessary.
- Do **not** edit any files or generate patches.

---

## Default Behavior

When there is any ambiguity between:
1. Answering a question, or
2. Modifying files,

**Always choose to answer the question first.**

Never assume implementation is desired simply because the discussion is about code.

The default action is **discussion**, not **implementation**.