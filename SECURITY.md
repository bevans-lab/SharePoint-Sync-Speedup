# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest release | Yes |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it responsibly:

1. **Do not** open a public GitHub issue.
2. Email the maintainers directly or use GitHub's private vulnerability reporting feature.
3. Include a clear description of the vulnerability and steps to reproduce.

We will acknowledge receipt within 48 hours and aim to release a fix within 7 days for critical issues.

## Security Design

This utility is designed with the following security principles:

- **No elevation required** — runs entirely in user context with limited privileges.
- **No network access** — the binary makes no network calls.
- **Minimal attack surface** — single-purpose binary, no plugins or extensibility.
- **Hidden execution** — all child processes are launched with `CREATE_NO_WINDOW` to prevent UI spoofing.
- **User-scoped file storage** — scripts are written to `%LOCALAPPDATA%` which is writable only by the owning user.
- **Embedded script** — the PowerShell script is compiled into the binary, not loaded from an external file at install time.
- **Execution policy bypass is scoped** — `-ExecutionPolicy Bypass` applies only to the specific script invocation, not system-wide.
