#!/usr/bin/env python3
"""
Build script for PlatformIO to set version from VERSION file.
Place in platformio.ini:
  extra_scripts = pre:set_version.py
"""

import os
import subprocess
from datetime import datetime

Import("env")

# Read version from VERSION file
version_file = os.path.join(env.get("PROJECT_DIR"), "VERSION")
if os.path.exists(version_file):
    with open(version_file, "r") as f:
        version = f.read().strip()
else:
    version = "0.0.0"

# Get git hash if available
try:
    git_hash = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=env.get("PROJECT_DIR"),
        stderr=subprocess.DEVNULL
    ).decode("utf-8").strip()
except:
    git_hash = ""

# Get build date
build_date = datetime.now().strftime("%Y-%m-%d")

# Set build flags
env.Append(CPPDEFINES=[
    ("MESHGRID_VERSION", f'\\"{version}\\"'),
    ("MESHGRID_BUILD_DATE", f'\\"{build_date}\\"'),
    ("MESHGRID_GIT_HASH", f'\\"{git_hash}\\"'),
])

print(f"meshgrid v{version} ({build_date}) [{git_hash}]")
