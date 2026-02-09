# GitHub Actions workflows

| Workflow | Purpose |
|----------|---------|
| **CI** | Build and test on Ubuntu with **GCC 13** and **Clang 18** (C++23). Conan install → CMake build with `NORMITRI_WARNINGS_AS_ERRORS=ON` → ctest. |
| **Docker** | Build the project Docker image and run install + build + tests inside the container (validates Docker path). |
| **Format** | Check that all tracked C/C++ files match `.clang-format` (fails if any file would be reformatted). |
| **CodeQL** | Security and quality analysis for C++; results in the Security tab and as PR checks. |
| **Release** | **CD:** Build in Docker, run tests, create GitHub Release and upload **Linux x64 CLI binary**, **source zip**, and **SHA256SUMS**. |

- **Triggers:** All workflows are **trigger-based only**. Run from **Actions** tab → choose workflow → **Run workflow** (optionally pick branch/tag). No automatic runs on push or pull_request.
- **Concurrency:** New runs cancel in-progress runs for the same ref (release does not cancel).
- **Secrets:** `GITHUB_TOKEN` is used for Conan, CodeQL, and release uploads; no extra secrets required.

**Cut a release (CD):**
1. Create and push the tag: `git tag v0.1.0 && git push origin v0.1.0`
2. In GitHub: **Actions** → **Release** → **Run workflow**; choose ref **v0.1.0** (or the branch you tagged), enter version **v0.1.0**, run.
3. The workflow builds, tests, creates the GitHub Release, and attaches `normitri_cli-linux-x86_64`, `normitri-0.1.0-source.zip`, and `SHA256SUMS.txt`.

To fix format failures locally:  
`clang-format -i $(git ls-files '*.cpp' '*.hpp' '*.h')`
