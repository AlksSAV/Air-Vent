# Agent Rules — Air-Vent

## General

- Always respond in Russian unless the user explicitly asks for another language.
- No conversational filler: no "Хорошо, сейчас...", "Сейчас я проверю...", "Давай посмотрим...".
- State the next tool call directly, without announcing it.
- When the user says "no", do not repeat the tool call. Offer an alternative instead.
- Keep answers under three lines when possible. Clarity > brevity.

## Code changes

- **Comments in code must be in English only.** Never write comments in Russian.
- **Do not invent logic that is not already present in the code or the spec.** Read the code first, then change only what is asked.
- Do not add explanatory text inside code blocks that is not part of the code (e.g. "this is not X, it is Y").
- Do not add comments to explain "what is wrong" with existing code. Fix the code, leave the comments as they are.
- Do not change existing comments unless they are in the file you are editing and the user explicitly asked for it.
- Never translate comments from Russian to English (or vice versa) unless the user asks.

## Documentation

- Documentation (README.md, USER_GUIDE.md, TODO.md) may be edited in Russian.
- When adding a section to documentation, place it in the correct existing section, do not create new top-level sections without asking.
- When the user says "close the issue", mark it as `[x]` in TODO.md and do not add extra notes.

## File paths

- Always use absolute paths when calling `read_file`, `write_file`, or `replace`.
- Combine the project root (`D:\GitHub\Air-Vent`) with the relative path.

## Safety

- Before running a shell command that modifies the filesystem or the codebase, briefly state what it does.
- Do not commit, push, or delete anything without explicit permission.
- Never hardcode secrets, API keys, or credentials.

## Git

- Draft commit messages in English.
- Focus on "why" not "what".
- Verify with `git status` after each commit.
