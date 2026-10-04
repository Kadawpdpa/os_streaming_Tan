# Let friends help develop the project (GitHub + no WSL required)

## Run code on Windows/macOS without installing WSL

Use **Docker** (see details in `docs/CROSS-PLATFORM.md`). Friends using Windows or macOS only need to install Docker Desktop. No WSL, no manual cross-compiler installation:
```bash
docker build -t osk .
docker run -it --rm -p 8080:8080 -v "$PWD/www:/os/www" osk
After editing the code, rebuild with docker build -t osk . every time (or you can mount the entire project folder to edit it live. Let me know if you want to do this and I'll adjust the Dockerfile).

Invite friends to the repo
Repo → Settings → Collaborators → Add people. Enter your friend's username and have them accept the invitation.

Have your friend set up their own SSH key (a different file from yours; never share private keys with each other) following the steps we did previously.

Prevent breaking the main branch (Highly recommended for group work)
Repo → Settings → Branches → Add branch protection rule, set the name to main, and enable:

Require a pull request before merging — Disallows pushing directly to main. A PR must be opened to prevent code that fails to build from slipping into main unseen.

Require approvals (at least 1) — Another friend must review the code before merging.

(If applicable) Require status checks to pass — If CI is set up (see below), this forces the build to pass before merging.

Daily workflow:

Bash
git pull --rebase                       # Pull latest work before starting every time
git checkout -b feature/audio-pwm       # Branch out according to the task
# ...edit code, commit...
git push -u origin feature/audio-pwm
# Then open a Pull Request on GitHub for a friend to review before merging
Divide work by folder (e.g., one person handles drivers/, another handles kernel/thread.c). Keeping files separate drastically reduces conflicts.

Automated CI: Build on every push/PR (Optional, but very helpful for group work)
This file enables GitHub to automatically build the kernel every time someone pushes or opens a PR. If someone writes code that fails to compile, a red cross will appear immediately on GitHub before merging into main:

Bash
mkdir -p .github/workflows
Create file .github/workflows/build.yml:

YAML
name: build
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get update && sudo apt-get install -y gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-system-arm
      - run: make
Commit and push this file to the repo. Then, enable "Require status checks to pass" in the branch protection steps above and select the check named build.

Keep secrets safe
Do not commit tokens, passwords, or private keys to the repo, not even in a single commit (deleting it later isn't enough because it remains in the git history). If leaked, revoke it immediately (e.g., tokens in settings/tokens) and generate a new one.

This project has no secrets to store (no API keys, no passwords in the code), so the risk is already low, but it's a good habit to practice.