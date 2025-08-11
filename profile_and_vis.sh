#!/bin/bash

set -euo pipefail

# build (will take ~0 time if already built)
mkdir -p build
cd build || exit
cmake ..
make

# run cache profiler and gen .csv file
./cache_profiler
cd ..
echo

# setup python venv and dependencies
if [ ! -e "./venv/bin/activate" ]; then
    echo "Setting up Python venv, this may take a second..."
    python3 -m venv venv
    source ./venv/bin/activate
    pip install uv
    uv pip install -r requirements.txt
    echo "Finished setting up Python venv."
    echo
else
    source ./venv/bin/activate
fi
# we can now be sure that venv is setup and activated

echo "Visualizing measurements..."
python3 visualize_measurements.py
echo "Done visualizing measurements. Check your graphs/ folder to view."
