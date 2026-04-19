import sys
import time
import argparse

try:
    import sharedvalue5 as sv5
except ImportError as e:
    print(f"Failed to load sharedvalue5 module: {e}")
    sys.exit(1)

parser = argparse.ArgumentParser(description='SharedValueV5 Python Consumer Test')
parser.add_argument('--timeout', type=int, default=15, help='Maximum seconds to wait for connection before gracefully exiting.')
args = parser.parse_args()

print("Starting Python SharedValueV5 Test Consumer...")

# Mimic the setup from our architecture
reader = sv5.SharedDataSet("SensorNet", is_host=False)
reader.start_listening()

print(f"Listening for incoming streams from C# or VBScript producers (Timeout: {args.timeout}s)...")

# Let's listen for a bit
for _ in range(args.timeout):
    has_tables = len(reader.tables) > 0
    if has_tables:
        print("\n--- Data Received! ---")
        for table in reader.tables:
            print(f"[{table.name}] | Rows: {len(table.rows)}")
            for row in table.rows:
                # Based on VBScript Producer: Temperature or humidity, or sensor_id
                # In VBScript it creates 'SensorNet' -> 'Temps' -> 'SensorName', 'Value'
                try:
                    s_name = row["SensorName"]
                    val = row["Value"]
                    print(f"  {s_name}: {val}")
                except KeyError:
                    pass
        break
    else:
        print(".", end="", flush=True)
    time.sleep(1)

print("\nExiting python reader.")
