# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]
### Added
- Mesh packing helper that preserves UV and tangent attributes during GPU upload.
- Automated UV propagation test using a dedicated OBJ asset for regression coverage.

### Fixed
- Regenerate tangent frames for imported meshes so normal maps remain stable when source assets omit tangents.
