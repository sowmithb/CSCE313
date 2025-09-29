#!/bin/bash
set -euo pipefail

echo "Starting benchmark tests..."
echo "=========================="

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'
OUTPUT_FILE="benchmark_results.txt"
: > "$OUTPUT_FILE"

echo "Building…"
make

# Ensure both binaries exist (client execs server)
for bin in ./client ./server; do
  if [ ! -x "$bin" ]; then
    echo -e "${RED}Error: missing $bin after make.${NC}"
    exit 1
  fi
done

# Pick a timeout command if available
TIMEOUT=""
if command -v timeout >/dev/null 2>&1; then
  TIMEOUT="timeout 300"
elif command -v gtimeout >/dev/null 2>&1; then
  TIMEOUT="gtimeout 300"
fi

# Ensure BIMDC exists
if ! ls BIMDC/*.csv >/dev/null 2>&1; then
  echo -e "${RED}No CSV files found under BIMDC/.${NC}"
  exit 1
fi

echo -e "\n${YELLOW}Running benchmark on all CSV files in BIMDC…${NC}\n"

SUCCESS_COUNT=0
TOTAL_COUNT=0

# Iterate files via glob (handles “no files” safely with the check above)
for file in BIMDC/*.csv; do
  [ -f "$file" ] || continue
  filename=$(basename "$file")

  # Cross-platform file size
  if filesize=$(stat -f%z "$file" 2>/dev/null); then :; else
    filesize=$(stat -c%s "$file")
  fi

  printf "Testing %s (%s bytes)… " "$filename" "$filesize"

  # Use /usr/bin/time -p to capture wall time (real)
  # Keep stderr for timing, suppress client stdout
  TMP=$(mktemp)
  if { $TIMEOUT /usr/bin/time -p ./client -f "$filename" >/dev/null; } 2> "$TMP"; then
    exec_time=$(awk '/^real/{print $2}' "$TMP")
    echo "$filesize $exec_time" >> "$OUTPUT_FILE"
    echo -e "${GREEN}SUCCESS${NC} (${exec_time}s)"
    SUCCESS_COUNT=$((SUCCESS_COUNT+1))
  else
    echo -e "${RED}FAILED${NC}"
    echo "$filesize 0" >> "$OUTPUT_FILE"
  fi
  rm -f "$TMP"
  TOTAL_COUNT=$((TOTAL_COUNT+1))
done

echo ""
echo "=========================="
echo "Benchmark completed!"
echo "Successful runs: $SUCCESS_COUNT/$TOTAL_COUNT"
echo "Results saved to: $OUTPUT_FILE"
echo ""

# Display results summary
if [ -f "$OUTPUT_FILE" ] && [ -s "$OUTPUT_FILE" ]; then
    echo "Results summary (file_size execution_time):"
    echo "-------------------------------------------"
    while read -r size time; do
        if [ "$time" != "0" ]; then
            printf "%-12s %s\n" "$size" "$time"
        else
            printf "%-12s %s (FAILED)\n" "$size" "$time"
        fi
    done < "$OUTPUT_FILE"
else
    echo "No results to display."
fi

echo ""
echo "To create a plot, you can use the following Python script:"
echo "python3 -c \""
echo "import matplotlib.pyplot as plt"
echo "import numpy as np"
echo "data = np.loadtxt('$OUTPUT_FILE')"
echo "plt.figure(figsize=(10, 6))"
echo "plt.scatter(data[:,0], data[:,1])"
echo "plt.xlabel('File Size (bytes)')"
echo "plt.ylabel('Execution Time (seconds)')"
echo "plt.title('Client Performance: File Size vs Execution Time')"
echo "plt.grid(True)"
echo "plt.savefig('benchmark_plot.png')"
echo "plt.show()"
echo "\""
