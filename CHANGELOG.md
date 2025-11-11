# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]
### Added
- Mesh packing helper that preserves UV and tangent attributes during GPU upload.
- Automated UV propagation test using a dedicated OBJ asset for regression coverage.
- Git identity rewrite plan documenting the procedure to reset historical author/committer metadata (.agent/plan_git_identity_rewrite.md).

### Fixed
- Regenerate tangent frames for imported meshes so normal maps remain stable when source assets omit tangents.
- Stop flipping textures vertically during load (glTF assets now match Blender by default). Legacy behaviour can be restored with `VULKANO_ENABLE_TEXTURE_FLIP=1`.
