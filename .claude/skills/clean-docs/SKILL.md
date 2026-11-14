---
name: clean-docs
description: Clean up documentation in this repo, in both source-code comments and markdown files, so it follows the project's documentation rules. Use whenever the user asks to clean up, tidy, trim, audit, or fix docs, comments, docstrings, READMEs or markdown, or to remove history, changelog-style text, bug-fix notes, stale or redundant comments. Also use after a large feature or port, when comments and docs tend to accumulate narration about what changed. Apply even if the user only says "the comments are a mess" or "docs are out of date".
---

# Cleaning up documentation

Documentation in this repo explains how the code works now. It is not a record of how it got
there. Comments and docs that narrate history, repeat the code, or drift out of date make the code
harder to read and are wrong more often than they are right, so this skill removes or rewrites
them.

`AGENTS.md` in the repo root is the source of truth for the rules. Read it first, and if it
disagrees with this file, follow `AGENTS.md`. Never run `git commit` or `git push`; leave the
changes in the working tree for the user to review.

## Scope

Ask the user only if the scope is unclear. By default, clean what they name: a directory, a file,
or the files changed on the current branch (`git diff --name-only <base>...`). With no scope given,
cover the whole repo.

Skip these:

- generated code (`src/generated/`, ANTLR output) and anything under `build/`
- `.claude/worktrees/` and other git worktrees
- vendored or third-party code, and file contents that are data (test vectors, expected output)
- `docs/known-bugs.md`: it is the one place that records defects, so bugs belong there. Check it
  against the rules below (see "Markdown"), but don't strip its entries.
- license headers

## Source-code comments

Go through each file and judge every comment. Keep a comment only if it explains something the
code cannot say by itself.

**Remove:**

- history and bug-fix comments: "fixed", "previously", "used to", "now handles", "workaround for the
  bug where", "added for", "TODO: remove after", references to old behaviour, to issue numbers or
  commits, and to dates
- comments that restate the code (`// increment i`, `// return the result`)
- docstrings, JSDoc and Doxygen blocks on functions, classes and modules that explain how to call or
  use them. Remove parameter-by-parameter descriptions and usage examples.
- commented-out code
- banner and separator comments with no content
- stale comments that no longer match the code, unless you can fix them with certainty (see below)

**Keep:**

- a short comment saying what a function does, only when that is non-obvious from its name and body
- the reason behind non-obvious logic: a hardware quirk, an encoding or ABI detail, an algorithm
  choice, an invariant, or why an apparently redundant operation is needed. Explain how it works,
  not that it was once broken. For example, "the 6809 is big-endian, so the high byte is stored
  first" stays, and "fixed byte order bug" goes.
- pointers to a specification or datasheet section that explains an encoding

**Rewrite** a comment that carries useful information but is phrased as history or usage. Turn
"previously we forgot to mask the carry, so now we do" into "mask the carry: the flag register keeps
only 8 bits". If the useful part is not clear from the code, keep it brief.

A comment that looks stale is a prompt to check the code, not to delete blindly. If the comment
disagrees with the code and you can tell which is right, fix the comment. If you can't tell, leave
it, and list it in the report for the user.

Remove comments only. Leave code behaviour untouched, including formatting changes outside the
comment you are editing. After editing C and C++ files, build the project, and fix every
warning that the build reports.

## Markdown

This covers `README.md`, `docs/*.md`, per-example READMEs and any other `.md` file.

**Remove or rewrite:**

- changelog and history sections, "what's new", "changed in", "added in", "formerly", "no longer",
  "was fixed", migration notes, and release-style narration
- bug-fix explanations. A bug belongs in `docs/known-bugs.md`; when the bug is fixed, its entry
  is removed.
- how-to-use-this-function, class or module text in the docs. Docs describe how the system works
  (design, formats, ABIs, memory maps, behaviours), not call-by-call usage.
- statements that are no longer true. Verify claims against the code before keeping or fixing them:
  supported target lists, command-line flags, file paths, make targets, and example names.
- duplicated content. Keep it in one place and link to it from the others.
- TODO and "not yet implemented" notes that duplicate `docs/known-bugs.md`. Keep one source.

**Check:**

- every link, relative path and referenced file or Make target exists
- lists that mirror code (supported `--target` values, `--format` choices, examples) match
  `rcc --list-platforms`, `src/main.cpp` and the `examples/` directory
- headings, tables and code fences are consistent with the neighbouring docs
- present tense throughout

`docs/known-bugs.md` still has to follow the rules: each entry describes the bug as a present-tense
problem, with no narrative about how it was found, and entries for bugs that are already fixed are
removed. Verify a doubtful entry before removing it, by checking the code or running the case.

## Process

1. Read `AGENTS.md`, and decide the scope.
2. Make one pass per area (a source directory or a docs file) so the diff stays reviewable. Use
   `rg` to find the obvious offenders first (`fixed`, `previously`, `used to`, `no longer`,
   `workaround`, `TODO`, `HACK`, `@param`, `/**`), then read around each hit instead of deleting by
   pattern. A matching word is not always a bad comment.
3. Build and run the tests if you edited any source files: the root `Makefile`'s `all` and `test`
   targets. Comment-only edits should change nothing, so a failure means a comment edit touched
   code, and you must fix it.
4. Look at `git diff --stat` and spot-check the diff for deleted code and for comment text you
   changed by mistake.

## Report

Give a short summary: how many files you touched and what kind of changes, any comments you kept
or rewrote that the user might question, and anything you left because you couldn't verify it
(stale-looking comments, doubtful claims). Don't narrate each edit. Leave the changes uncommitted.
