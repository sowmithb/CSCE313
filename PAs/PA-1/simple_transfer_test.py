#!/usr/bin/env python3
"""
Simple script to measure file transfer times and create visualization.
"""

import os
import subprocess
import time
import json
import matplotlib.pyplot as plt

def measure_transfer_time(filename):
    """Measure transfer time for a given file"""
    print(f"Measuring transfer time for {filename}...")
    
    # Start timing
    start_time = time.time()
    
    # Run the client with the specified file
    cmd = ["./client", "-f", filename]
    
    try:
        # Run the command and capture output
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        
        # End timing
        end_time = time.time()
        transfer_time = end_time - start_time
        
        if result.returncode != 0:
            print(f"Error transferring {filename}: {result.stderr}")
            return None
            
        return transfer_time
        
    except subprocess.TimeoutExpired:
        print(f"Timeout transferring {filename}")
        return None
    except Exception as e:
        print(f"Exception transferring {filename}: {e}")
        return None

def get_file_size_mb(filepath):
    """Get file size in MB"""
    size_bytes = os.path.getsize(filepath)
    return size_bytes / (1024 * 1024)

def main():
    """Main function to measure transfer times and create visualization"""
    print("Starting file transfer time measurements...")
    
    # Make sure the project is built
    print("Building project...")
    build_result = subprocess.run(["make"], capture_output=True)
    if build_result.returncode != 0:
        print("Build failed!")
        return
    
    # Create received directory
    os.makedirs("received", exist_ok=True)
    
    # Test files to transfer
    test_files = [
        "test_files/test_1KB.dat",
        "test_files/test_10KB.dat", 
        "test_files/test_100KB.dat",
        "test_files/test_1MB.dat",
        "test_files/test_10MB.dat"
    ]
    
    # Measure transfer times
    results = []
    
    for filepath in test_files:
        if os.path.exists(filepath):
            filename = os.path.basename(filepath)
            file_size_mb = get_file_size_mb(filepath)
            
            print(f"\nTesting {filename} ({file_size_mb:.3f} MB)")
            
            # Measure transfer time
            transfer_time = measure_transfer_time(filename)
            
            if transfer_time is not None:
                throughput_mbps = (file_size_mb * 8) / transfer_time  # Mbps
                result = {
                    'filename': filename,
                    'file_size_mb': file_size_mb,
                    'transfer_time_s': transfer_time,
                    'throughput_mbps': throughput_mbps
                }
                results.append(result)
                print(f"  Transfer time: {transfer_time:.4f} seconds")
                print(f"  Throughput: {throughput_mbps:.2f} Mbps")
            else:
                print(f"  Failed to transfer {filename}")
        else:
            print(f"File not found: {filepath}")
    
    if not results:
        print("No successful transfers to analyze")
        return
    
    # Save results to JSON
    with open('transfer_results.json', 'w') as f:
        json.dump(results, f, indent=2)
    
    # Create visualization
    create_visualization(results)
    
    print(f"\nResults saved to transfer_results.json")
    print(f"Visualization saved to transfer_analysis.png")

def create_visualization(results):
    """Create visualization of transfer times vs file size"""
    if not results:
        print("No results to visualize")
        return
    
    # Extract data
    file_sizes = [r['file_size_mb'] for r in results]
    transfer_times = [r['transfer_time_s'] for r in results]
    throughputs = [r['throughput_mbps'] for r in results]
    
    # Create figure with subplots
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 10))
    
    # Plot 1: Transfer Time vs File Size (Linear scale)
    ax1.plot(file_sizes, transfer_times, 'bo-', linewidth=2, markersize=8)
    ax1.set_xlabel('File Size (MB)')
    ax1.set_ylabel('Transfer Time (seconds)')
    ax1.set_title('Transfer Time vs File Size (Linear Scale)')
    ax1.grid(True, alpha=0.3)
    
    # Add data labels
    for i, (x, y) in enumerate(zip(file_sizes, transfer_times)):
        ax1.annotate(f'{y:.3f}s', (x, y), textcoords="offset points", 
                    xytext=(0,10), ha='center')
    
    # Plot 2: Transfer Time vs File Size (Log scale)
    ax2.loglog(file_sizes, transfer_times, 'ro-', linewidth=2, markersize=8)
    ax2.set_xlabel('File Size (MB)')
    ax2.set_ylabel('Transfer Time (seconds)')
    ax2.set_title('Transfer Time vs File Size (Log Scale)')
    ax2.grid(True, alpha=0.3)
    
    # Plot 3: Throughput vs File Size
    ax3.plot(file_sizes, throughputs, 'go-', linewidth=2, markersize=8)
    ax3.set_xlabel('File Size (MB)')
    ax3.set_ylabel('Throughput (Mbps)')
    ax3.set_title('Throughput vs File Size')
    ax3.grid(True, alpha=0.3)
    
    # Add data labels
    for i, (x, y) in enumerate(zip(file_sizes, throughputs)):
        ax3.annotate(f'{y:.1f} Mbps', (x, y), textcoords="offset points", 
                    xytext=(0,10), ha='center')
    
    # Plot 4: Efficiency Analysis
    # Calculate efficiency as throughput per MB
    efficiencies = [t/s for t, s in zip(throughputs, file_sizes)]
    ax4.plot(file_sizes, efficiencies, 'mo-', linewidth=2, markersize=8)
    ax4.set_xlabel('File Size (MB)')
    ax4.set_ylabel('Efficiency (Mbps/MB)')
    ax4.set_title('Transfer Efficiency vs File Size')
    ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('transfer_analysis.png', dpi=300, bbox_inches='tight')
    print("Visualization created and saved as transfer_analysis.png")
    
    # Print summary statistics
    print(f"\n=== TRANSFER ANALYSIS SUMMARY ===")
    print(f"Files tested: {len(results)}")
    print(f"File size range: {min(file_sizes):.3f} MB - {max(file_sizes):.3f} MB")
    print(f"Transfer time range: {min(transfer_times):.4f}s - {max(transfer_times):.4f}s")
    print(f"Throughput range: {min(throughputs):.2f} Mbps - {max(throughputs):.2f} Mbps")
    
    # Calculate average throughput
    avg_throughput = sum(throughputs) / len(throughputs)
    print(f"Average throughput: {avg_throughput:.2f} Mbps")

if __name__ == "__main__":
    main()
