## Git Identity Rewrite Plan

### Goal
Rebase and rewrite the entire repository history so every commit records the corrected author and committer identity, ensuring remote mirrors and collaborators adopt the new metadata without ambiguity.

### Constraints
- Follow AGENTS.md: document steps, update `.agent/TODO.md`, run validations, and update `CHANGELOG.md` before finishing.
- Preserve tree content hashes whenever possible; only metadata should change.
- Use `git filter-repo` (preferred) or `git rebase --root` only if filter-repo is unavailable.
- Work from a clean worktree; stash or commit any local edits before rewriting.
- Never rewrite public history without coordinating backups and force-push plans.

### Step-by-Step Plan
1. **Confirm Target Identity**
   - Decide on the canonical `GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL`, `GIT_COMMITTER_NAME`, and `GIT_COMMITTER_EMAIL`.
   - Record them in `.agent/git_identity.env` (not committed) for repeatability during the session.
2. **Audit Existing History**
   - Run `git shortlog -sne --all` to list every identity currently recorded.
   - Identify which identities must be replaced and whether any should stay untouched (e.g., external contributors).
3. **Create a Safety Backup**
   - Make a fresh bare clone (`git clone --mirror . ../vulkano_codex_backup.git`) or tag the current HEAD.
   - Confirm the backup remote is accessible in case rollback is needed.
4. **Author Mapping Definition**
   - Create `.agent/git_identity_mailmap` listing every old identity that should map to the new one using the mailmap syntax (`New Name <new@email.com> Old Name <old@email>`).
   - Version-control this mailmap snapshot so future rewrites are reproducible.
5. **Run `git filter-repo`**
   - Execute `git filter-repo --mailmap .agent/git_identity_mailmap --force` from the repo root.
   - If `git filter-repo` is unavailable, fall back to `git rebase --root --exec 'git commit --amend --reset-author'` while exporting `GIT_AUTHOR_*`/`GIT_COMMITTER_*`.
6. **Verify Rewritten History**
   - Inspect `git log --format='%h %an <%ae> %cn <%ce>' | head` and `tail` to ensure both author and committer fields use the new identity.
   - Run `git fsck --strict` plus the full build/test suite to confirm the tree state is unchanged and healthy.
7. **Force-Push Updated History**
   - Coordinate downtime with collaborators, then push rewritten branches and tags using `git push --force-with-lease --tags origin <branch>`.
   - Prompt teammates to reset their local clones (`git fetch origin --prune`, `git reset --hard origin/<branch>`).
8. **Documentation & Cleanup**
   - Add a `CHANGELOG.md` entry under “Changed” describing the identity rewrite and its impact on contributors.
   - Update `.agent/TODO.md` to reflect completion, store the plan reference, and keep the backup until the team confirms success.

### Acceptance Criteria
- Every commit in `git log --all` shows the corrected author and committer identity.
- Build/tests still pass after rewriting (no tree or binary drift).
- Remote branches/tags are force-pushed and collaborators have been notified to resync.
- `CHANGELOG.md` documents the rewrite and references this plan.
