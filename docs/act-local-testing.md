# Local CI/CD testing with `act`

Test GitHub Actions workflows **locally** with [act](https://github.com/nektos/act) before pushing, so you can fix workflow issues without many commits.

## Why use act?

- **Validate workflows** before pushing to GitHub
- **Debug** failures locally with verbose output
- **Save CI minutes** and avoid repeated pushes
- **Faster feedback** while editing `.github/workflows/*.yml`

## Prerequisites

### 1. Docker

`act` runs workflows in Docker containers. Install and start Docker:

**Linux / WSL:**
```bash
sudo apt-get update && sudo apt-get install -y docker.io
sudo service docker start
sudo usermod -aG docker $USER
newgrp docker   # or log out and back in
```

**Windows:** Install [Docker Desktop](https://www.docker.com/products/docker-desktop); enable WSL2 if you use WSL.

**macOS:** [Docker Desktop](https://www.docker.com/products/docker-desktop) or `brew install --cask docker`.

Verify:
```bash
docker --version
docker ps
```

### 2. Install act

**Linux / WSL:**
```bash
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
# If act is installed to ./bin:
export PATH="$PATH:$(pwd)/bin"
```

**macOS:**
```bash
brew install act
```

**Windows:** Use WSL and the Linux method above.

Check:
```bash
act --version
```

### 3. Default image (first run)

The first time you run `act`, it will ask you to pick a default image:

- **Medium** (~500MB): Good default; has common tools (recommended).
- **Large** (~17GB): Closest to GitHub runners; use if medium fails.
- **Micro** (&lt;200MB): Minimal; often too small for our workflows.

Choose **Medium** unless a job fails for missing tools.

### 4. Platform image for `ubuntu-24.04` (required for our workflows)

Our workflows use **`runs-on: ubuntu-24.04`**. By default, act does not provide an image for that runner, so you will see:

```text
🚧  Skipping unsupported platform -- Try running with `-P ubuntu-24.04=...`
```

You **must** pass a Docker image for that platform. Use one of these:

```bash
# Option A: act-oriented image (has common Actions preinstalled; may be 22.04)
-P ubuntu-24.04=catthehacker/ubuntu:act-latest

# Option B: official Ubuntu 24.04 (if act-latest is too old)
-P ubuntu-24.04=ubuntu:24.04
```

**Example — run CI locally:**
```bash
act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest
```

To avoid typing `-P ...` every time, you can set a default in **`~/.config/act/actrc`**:

```bash
mkdir -p ~/.config/act
echo '-P ubuntu-24.04=catthehacker/ubuntu:act-latest' >> ~/.config/act/actrc
```

Then `act workflow_dispatch -W .github/workflows/ci.yml` will use that image.

## Our workflows (trigger-based)

All workflows use `workflow_dispatch` (manual trigger). No inputs except **Release**, which requires a version.

| Workflow   | Job(s)              | What it does |
|-----------|----------------------|--------------|
| **CI**    | `build-and-test`     | Matrix: GCC + Clang on Ubuntu. Conan install → build → ctest. |
| **Docker**| `docker-build-and-test` | Build image, run `build_nomitri.sh all` + ctest in container. |
| **Format**| `format-check`       | clang-format check on tracked C/C++. |
| **CodeQL**| `analyze`            | Conan + build, then CodeQL C++ analysis. **Not recommended with act** (see below). |
| **Release**| `build-and-release` | Docker build + test, then create GitHub Release (needs `version` input). |

**CodeQL and act:** The CodeQL workflow often **hangs or runs 20+ minutes** when run with act. It clones the large `github/codeql-action` repo and downloads CodeQL CLI/bundles inside the runner; that can stall or time out locally. **Run CodeQL from the Actions tab on GitHub** when you need security analysis; use act for CI, Docker, Format, and Release.

## Basic usage

### List workflows and jobs

```bash
cd /path/to/Normitri   # or defect-detection
act -l
```

Shows workflow names, job names, and the `workflow_dispatch` event.

### Dry-run (recommended first)

See what would run **without** executing steps. Add **`-P ubuntu-24.04=catthehacker/ubuntu:act-latest`** if you didn’t set `~/.config/act/actrc`:

```bash
# CI
act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n

# Docker
act workflow_dispatch -W .github/workflows/docker.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n

# Format
act workflow_dispatch -W .github/workflows/format.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n

# CodeQL
act workflow_dispatch -W .github/workflows/codeql.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n

# Release (needs version input)
act workflow_dispatch -W .github/workflows/release.yml --input version=v0.1.0 -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n
```

### Run workflows (real execution)

Runs in Docker; CI and Docker workflows will take time (Conan, build, tests). Use **`-P ubuntu-24.04=catthehacker/ubuntu:act-latest`** (or rely on actrc):

```bash
# Format (fast)
act workflow_dispatch -W .github/workflows/format.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest

# CI (slow: Conan + build + test, matrix = 2 jobs)
act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest

# Docker (slow: build image + build + test)
act workflow_dispatch -W .github/workflows/docker.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest

# CodeQL: skip with act (can hang 20+ min); run from Actions tab on GitHub.

# Single CI job
act workflow_dispatch -W .github/workflows/ci.yml -j build-and-test -P ubuntu-24.04=catthehacker/ubuntu:act-latest
```

**Release:** Creating a real release from act is possible but uses your repo and token. Prefer dry-run or run Release on GitHub after tag push.

## Testing a specific job

Use **`-P ubuntu-24.04=catthehacker/ubuntu:act-latest`** (or actrc) so the job is not skipped:

```bash
# Only the format check
act workflow_dispatch -W .github/workflows/format.yml -j format-check -P ubuntu-24.04=catthehacker/ubuntu:act-latest

# Only the Docker job
act workflow_dispatch -W .github/workflows/docker.yml -j docker-build-and-test -P ubuntu-24.04=catthehacker/ubuntu:act-latest

# Only the CI build-and-test job (act may run both gcc and clang matrix cells)
act workflow_dispatch -W .github/workflows/ci.yml -j build-and-test -P ubuntu-24.04=catthehacker/ubuntu:act-latest
```

## Useful options

| Option | Description |
|--------|-------------|
| `-n` | Dry-run: show steps, don’t run them. |
| `-j <job>` | Run only this job. |
| `-v` | Verbose output. |
| `-W <file>` | Use this workflow file. |
| `-P ubuntu-24.04=<image>` | Use a specific runner image (e.g. `catthehacker/ubuntu:act-24.04` if needed). |

Example with verbose (include `-P` so the job runs):
```bash
act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -v
```

## Troubleshooting

**"Skipping unsupported platform -- Try running with `-P ubuntu-24.04=...`"**  
Our workflows use `ubuntu-24.04`; act has no default image for it. Add:
`-P ubuntu-24.04=catthehacker/ubuntu:act-latest` to your command, or set it in `~/.config/act/actrc` (see [Platform image for ubuntu-24.04](#4-platform-image-for-ubuntu-2404-required-for-our-workflows)).

## Limitations when running locally

- **Secrets:** `GITHUB_TOKEN` is faked by act; real API calls (e.g. release upload) may not work as on GitHub.
- **Artifacts:** Upload/download artifacts can behave differently or fail.
- **CodeQL:** Analysis may run, but uploading results to the Security tab needs GitHub.
- **Release:** Creating a release and uploading assets is best done on GitHub after a tag push.

Use act to check that **build, test, and format** steps succeed; for release and CodeQL uploads, rely on GitHub after pushing.

## Suggested workflow

1. Edit a workflow (e.g. `.github/workflows/ci.yml`).
2. Dry-run: `act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n`.
3. If structure looks good, run for real (same `-P`); or run only one job with `-j build-and-test`.
4. Fix any failures locally, then push and trigger on GitHub if desired.

## Quick reference

Set once (optional): `echo '-P ubuntu-24.04=catthehacker/ubuntu:act-latest' >> ~/.config/act/actrc`

```bash
act -l
act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n
act workflow_dispatch -W .github/workflows/format.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest
act workflow_dispatch -W .github/workflows/ci.yml -P ubuntu-24.04=catthehacker/ubuntu:act-latest
act workflow_dispatch -W .github/workflows/docker.yml -j docker-build-and-test -P ubuntu-24.04=catthehacker/ubuntu:act-latest
act workflow_dispatch -W .github/workflows/release.yml --input version=v0.1.0 -P ubuntu-24.04=catthehacker/ubuntu:act-latest -n
```

## Links

- [act](https://github.com/nektos/act)
- [act usage](https://nektosact.com/usage/)
- [Workflows overview](.github/workflows/README.md)
