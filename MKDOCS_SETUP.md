# MkDocs Setup for PCAN Documentation

## Quick Start

The project now has MkDocs configured with a Python virtual environment for Arch Linux compatibility.

### Running MkDocs

**Option 1: Using the helper script (recommended)**
```bash
./mkdocs.sh serve
```

**Option 2: Using venv directly**
```bash
./venv/bin/mkdocs serve
```

**Option 3: Activate venv manually**
```bash
source venv/bin/activate
mkdocs serve
deactivate  # when done
```

### Access Documentation

Once running, open your browser to:
```
http://127.0.0.1:8000/pcan/
```

### Building Static Site

```bash
./mkdocs.sh build
```

Output will be in `site/` directory.

### Deploying to GitHub Pages

```bash
./mkdocs.sh gh-deploy
```

## Setup Details

- **Virtual Environment**: `venv/` (excluded from git)
- **Dependencies**: Installed from `requirements.txt`
- **Helper Script**: `mkdocs.sh` for easy command execution
- **Build Output**: `site/` (excluded from git)

## Warnings

The MkDocs server shows warnings about missing documentation files. These are placeholder pages in the navigation that you can create as needed:

- `docs/getting-started/overview.md`
- `docs/getting-started/prerequisites.md`
- `docs/getting-started/installation.md`
- `docs/hardware/specifications.md`
- `docs/build/instructions.md`
- And many more...

These warnings are normal and won't prevent the site from working. Create the files as you add content.

## Current Status

✅ Virtual environment created
✅ All dependencies installed
✅ MkDocs server working
✅ Documentation accessible at http://127.0.0.1:8000/pcan/

The documentation site is ready to use! You can start adding content to the `docs/` directory.
