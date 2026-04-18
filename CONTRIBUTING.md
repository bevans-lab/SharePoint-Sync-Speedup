# Contributing to SharePoint Sync Speedup

Thank you for your interest in contributing! Here's how to get started.

## Branch Strategy

| Branch | Purpose |
|--------|---------|
| `main` | Stable, release-ready code. Protected. |
| `develop` | Integration branch for in-progress work. |
| `feature/*` | New features — branch from `develop`. |
| `bugfix/*` | Bug fixes — branch from `develop`. |
| `release/*` | Release preparation — branch from `develop`, merge to `main`. |
| `hotfix/*` | Critical fixes — branch from `main`, merge to both `main` and `develop`. |

## Workflow

1. Fork the repository (external contributors) or create a feature branch (team members).
2. Branch from `develop`:
   ```
   git checkout develop
   git pull origin develop
   git checkout -b feature/your-feature-name
   ```
3. Make your changes, keeping commits focused and atomic.
4. Test your build locally (see [README.md](README.md) for build instructions).
5. Open a Pull Request targeting `develop`.

## Code Standards

- C++17 minimum.
- Compile cleanly with `/W4` (MSVC) or `-Wall -Wextra` (GCC/Clang).
- No external runtime dependencies — the binary must be fully self-contained.
- All child processes must be launched with `CREATE_NO_WINDOW` (invisible to user).

## Commit Messages

Use concise, imperative mood:
```
Add scheduled task creation logic
Fix script path escaping on spaces
```

## Reporting Issues

Use GitHub Issues. Include:
- Windows version and build number
- Intune deployment context (user/system)
- Steps to reproduce
- Expected vs. actual behavior
