---
name: bitbucket-pr-review
description: Review and address Bitbucket Cloud pull-request comments. Use when reading unresolved review threads, diagnosing feedback against a local PR diff, planning or implementing approved fixes, drafting replies, or posting and resolving approved comments. Do not use for ordinary code review that has no Bitbucket PR-comment workflow.
---

# Bitbucket PR Review

Use the bundled [Bitbucket helper](scripts/bitbucket_pr.py) for deterministic
comment access and mutations. It reads Bitbucket credentials from the
`api.bitbucket.org` entry in the user's `.netrc`; never print or copy those
credentials.

## Review before changing anything

1. Read the active instruction files for the target repository.
2. Identify the PR URL and verify that the local checkout and branch correspond
   to that PR. Inspect the existing local PR diff. If accurate inspection would
   require fetching or changing Git refs, obtain any approval required by the
   active instructions before doing so.
3. Read all unresolved top-level threads with:

   ```bash
   python3 <skill-dir>/scripts/bitbucket_pr.py unresolved <pr-url>
   ```

   Listing comments is read-only. If the active environment is known to
   require approval for network-enabled execution, request it on this first
   attempt through the platform dialog. Do not first run an expected-to-fail
   sandboxed request or ask for conversational permission before opening the
   dialog.
4. Inspect the relevant current code and PR-visible lines. Account for comments
   whose line numbers became stale after later commits.
5. Select the next unresolved comment and present only that comment. Include:
   - comment ID;
   - file and PR-visible line number, or general-comment status;
   - concise reviewer-text summary;
   - diagnosis based on the code and diff;
   - recommended action: code change, reply only, defer, or ask the user;
   - exact proposed reply text when applicable; and
   - expected files and functions when a code change is recommended.
6. Stop and wait for explicit user direction before editing code or presenting
   another comment. Approval for the current comment never authorizes work on
   a later comment.
7. Apply the user's direction to the current comment only. Finish it or record
   that the user explicitly deferred it before presenting the next unresolved
   comment.

Never batch comment plans or recommendations unless the user explicitly asks
for a batch. Even when unresolved comments are fetched together, reveal and
handle them one at a time by default.

The helper's `plan` command may produce a starting template, but diagnosis and
recommendations must come from inspecting the repository:

```bash
python3 <skill-dir>/scripts/bitbucket_pr.py plan <pr-url>
```

## Implement approved fixes

Apply only the approved code changes. Follow the repository's coding standards
and run its required focused checks. Do not stage, commit, push, or mutate the
PR unless the user separately authorizes those actions under the active
instructions.

After verification, report the result for the current comment. State whether
its thread can be resolved and present the exact proposed reply text. Stop and
wait for explicit approval of those exact Bitbucket mutations before posting,
resolving, or reopening anything. Do not present the next comment until the
current comment is finished or the user explicitly defers it.

## Bitbucket mutations

The helper is dry-run by default for every write command. A dry run prints the
method, URL, and payload and exits with status 2; this status means the write
was intentionally refused without `--yes`, not that Bitbucket failed.

Use these commands to preview an approved proposal before sending it:

```bash
python3 <skill-dir>/scripts/bitbucket_pr.py reply <pr-url> \
  --comment-id <id> --message-file <markdown-file>
python3 <skill-dir>/scripts/bitbucket_pr.py comment <pr-url> \
  --message-file <markdown-file>
python3 <skill-dir>/scripts/bitbucket_pr.py resolve <pr-url> --comment-id <id>
python3 <skill-dir>/scripts/bitbucket_pr.py reopen <pr-url> --comment-id <id>
```

Only after explicit approval for the displayed action, repeat that exact
command with `--yes`. The flag is a mechanical safety gate and never substitutes
for user approval. Treat reply, resolve, and reopen actions as distinct unless
the user clearly approves them together. Report the returned comment or thread
state after each sent mutation.

Never merge, approve, decline, or otherwise change PR state. This skill and its
helper intentionally expose no such operation.
