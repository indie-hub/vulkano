# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]
### Added
- Mesh packing helper that preserves UV and tangent attributes during GPU upload.
- Automated UV propagation test using a dedicated OBJ asset for regression coverage.
- Git identity rewrite plan documenting the procedure to reset historical author/committer metadata (.agent/plan_git_identity_rewrite.md).
- PR verification checklist for merging personal/codex-second-try/13-ui-overhalt into develop (docs/pr_test_personal_codex_second_try_13-ui-overhalt.md) plus the supporting plan in `.agent/plan_pr_merge_develop.md`.
- Expanded README with project overview, feature list, build/test instructions, and documentation map to guide new contributors.

### Changed
- Rewrote Git history so every commit now credits bruno o <bond.bales_0i@icloud.com> as both author and committer per `.agent/plan_git_identity_rewrite.md`.
- Verified the rewritten history via `git fsck --strict` and a full Release build/test run, and documented the remaining force-push + cleanup follow-ups in `.agent/TODO.md`.
- Removed the temporary `../vulkano_codex_backup.git` mirror after confirming the rewritten history was published.

### Fixed
- Regenerate tangent frames for imported meshes so normal maps remain stable when source assets omit tangents.
- Stop flipping textures vertically during load (glTF assets now match Blender by default). Legacy behaviour can be restored with `VULKANO_ENABLE_TEXTURE_FLIP=1`.
