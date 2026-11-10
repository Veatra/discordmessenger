# Instructions for coding agents

## Scope and workflow

- These instructions apply throughout this repository. Follow more specific
  `AGENTS.md` files in subdirectories when present.
- Use the existing checkout. Cloud tasks are already isolated; do not create a
  Git worktree unless the user requests one.
- Treat requests to fix or implement something as authorization to complete the
  work, validate it, and deliver it. Questions or investigations alone do not
  authorize unrelated edits.
- Inspect the current Git status before changing files. Preserve user changes,
  and commit only work belonging to the current task.
- Keep changes focused. Avoid unrelated refactors, dependency upgrades, and
  formatting churn. Preserve each file's existing line endings and style.
- Never commit credentials, Discord tokens, local settings, build outputs, or
  downloaded dependencies.

## Project and compatibility

- Discord Messenger is a Windows desktop application written in C++11.
  `src/core` contains application and gateway logic; `src/windows` contains
  Win32 UI code. Resources are in `src/resource.rc` and `res`.
- Linux development uses an i686 MinGW cross-compiler. Visual Studio project
  files are under `vs`. Read `README.md`, `Makefile`, and
  `.github/workflows/build.yml` for the current build workflow.
- Preserve the intended legacy Windows compatibility. Avoid introducing newer
  Windows APIs or C++ features without checking existing compatibility wrappers
  and discussing a necessary compatibility change with the user.
- Keep MinGW and Visual Studio source lists consistent when adding, removing,
  or renaming files. Do not edit vendored libraries or submodule revisions unless
  required by the task.

## Build and validation

Run commands from the repository root unless stated otherwise.

1. Initialize required submodules with `git submodule update --init --recursive`.
2. Use the prepared toolchain when available. In this cloud environment,
   `/workspace/dm-tools/activate.sh` sets `PATH` and `OPENSSL_DIR`; source it
   before building. This helper is environment-local, not part of the repository.
3. A representative Unicode release build with WebP support is:

   ```sh
   make -j4 all DEBUG=no UNICODE=yes
   ```

   It requires `OPENSSL_DIR` pointing to the Windows OpenSSL libraries and
   headers, and `LIBWEBP_DIR` pointing to the built Windows WebP library.
   Use the dependencies described in the README and CI workflow, not Linux
   libraries linked into a Windows target.
4. When WebP is unavailable, the supported optional fallback is:

   ```sh
   make -j4 all DEBUG=no UNICODE=yes DISABLE_WEBP=1 USER_DEFINES=-DWEBP_DISABLED
   ```

   Both options are required by the current code. The Makefile checks `1`,
   despite the README mentioning `yes`. Disclose disabled WebP in build reports.
5. Specify `all` explicitly: included dependency files can otherwise affect the
   default target. Use a separate `BUILD_DIR` and `BIN_DIR`, or clear only the
   relevant generated outputs, when switching Unicode/ANSI or other build flags.
   Remove stale generated dependency files if they reference deleted headers.
6. Run relevant existing tests and add focused regression coverage when useful
   for behavior changes. Do not mistake third-party dependency tests for an
   application test suite. Environment-local test helpers, if present, supplement
   validation but are not guaranteed to exist in another environment.
7. Compilation does not validate Windows GUI or live Discord behavior. State
   precisely what was tested and what remains untested. Do not require real
   Discord credentials for compilation or isolated tests.

The executable is `bin/DiscordMessenger.exe`. For a runnable Windows bundle,
inspect its imported DLLs and include the required matching Windows libraries
beside it. Do not assume CI packaging covers every toolchain's runtime DLLs.
Do not commit generated binaries; publish release assets only when requested.

## Connection and UI behavior

- Keep automatic reconnects quiet, including when minimized or hidden in the
  tray. Do not restore the removed connection popup or steal focus during
  background recovery.
- Preserve actionable authentication and security errors. Never weaken TLS or
  certificate verification to make connections succeed.
- Keep gateway state changes and Win32 timer/UI operations on the appropriate
  thread. Ignore callbacks from obsolete connections and release connection
  resources and timers when replacing or closing a session.
- Treat connection ID zero as valid; negative IDs represent no connection.
- Avoid rapid retry loops, blocking the UI, and logging credentials or complete
  authentication payloads.

## Commit and GitHub delivery preference

- For completed, requested repository edits, commit and push the changes to
  `origin/master` after appropriate validation, unless the user's current
  instructions specify otherwise. This is the repository owner's standing
  preference; do not ask for the same authorization every task.
- Before pushing, check the remote branch and integrate upstream changes safely.
  Never force-push or overwrite others' work. Use a concise commit message that
  explains the resulting change.
- If branch protection prevents a direct push, push a descriptive feature branch
  and open a pull request targeting `master`, when available permissions permit.
- Reuse platform GitHub authentication. If write access or PR creation is blocked,
  preserve the local commit and explain the precise blocker. Never request secret
  values in chat or claim a push/PR succeeded without confirming it.
- Report the commit or pull-request link, validation results, and material
  limitations. Saving or publishing a Codex cloud environment does not push Git
  changes and does not substitute for this delivery step.
