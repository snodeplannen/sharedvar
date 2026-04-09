using System;
using System.Threading;
using SharedMemMap;

namespace csharp_core
{
    class Program
    {
        static int Main(string[] args)
        {
            string name = "MyGlobalDataset";
            int maxEvents = 0;          // 0 = infinite (manual exit)
            int connectTimeoutSec = 30;

            // Parse command line arguments
            for (int i = 0; i < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--name" when i + 1 < args.Length:
                        name = args[++i];
                        break;
                    case "--count" when i + 1 < args.Length:
                        maxEvents = int.Parse(args[++i]);
                        break;
                    case "--timeout" when i + 1 < args.Length:
                        connectTimeoutSec = int.Parse(args[++i]);
                        break;
                    case "--help":
                        Console.WriteLine("Usage: csharp_core [options]");
                        Console.WriteLine("Options:");
                        Console.WriteLine("  --name NAME       Shared memory name (default: MyGlobalDataset)");
                        Console.WriteLine("  --count N         Exit after receiving N events (default: infinite)");
                        Console.WriteLine("  --timeout SEC     Connection timeout in seconds (default: 30)");
                        Console.WriteLine("  --help            Show this help");
                        return 0;
                }
            }

            Console.WriteLine("[Consumer] Starting MemMap Consumer...");
            Console.WriteLine($"[Consumer] Config: name={name} count={(maxEvents == 0 ? "infinite" : maxEvents.ToString())} timeout={connectTimeoutSec}s");
            Console.WriteLine("[Consumer] Waiting for C++ Producer to initialize the shared memory...");

            SharedValueEngine? engine = null;
            var connectStart = DateTime.UtcNow;

            // Retry loop until producer is online or timeout
            while (engine == null)
            {
                try
                {
                    engine = new SharedValueEngine(name);
                }
                catch (EngineInitializationException)
                {
                    if ((DateTime.UtcNow - connectStart).TotalSeconds > connectTimeoutSec)
                    {
                        Console.Error.WriteLine($"[Consumer Error] Connection timeout after {connectTimeoutSec} seconds. Producer not found.");
                        return 2;
                    }
                    Thread.Sleep(500);
                }
            }

            Console.WriteLine("[Consumer] Successfully connected to Shared Memory & Mutex!");

            int receivedCount = 0;
            int exitCode = 0;
            using var completionEvent = new ManualResetEventSlim(false);

            // Subscribe to the central callback
            engine.OnDataReady += (dataset) =>
            {
                receivedCount++;
                Console.WriteLine($"\n--- Event #{receivedCount} Received ---");
                Console.WriteLine($"  API Version: {dataset.ApiVersion}");
                Console.WriteLine($"  Last Updated: {dataset.LastUpdatedUtc}");
                
                int rows = dataset.RowsLength;
                Console.WriteLine($"  Rows: {rows}");

                for (int i = 0; i < rows; i++)
                {
                    DataRow? row = dataset.Rows(i);
                    if (row.HasValue)
                    {
                        Console.WriteLine($"    [{i}] RowId={row.Value.RowId} Active={row.Value.IsActive}");
                        
                        int detailsLength = row.Value.DetailsLength;
                        for (int j = 0; j < detailsLength; j++)
                        {
                            NestedDetails? details = row.Value.Details(j);
                            if (details.HasValue)
                            {
                                Console.WriteLine($"        Detail[{j}] Temp={details.Value.Temperature:F1} Hum={details.Value.Humidity:F1} Status={details.Value.StatusCode}");
                            }
                        }
                    }
                }

                // Auto-exit when count reached
                if (maxEvents > 0 && receivedCount >= maxEvents)
                {
                    Console.WriteLine($"[Consumer] Received {receivedCount}/{maxEvents} events. Auto-exiting.");
                    completionEvent.Set();
                }
            };

            engine.StartListening();

            if (maxEvents > 0)
            {
                // Wait for the required number of events or overall timeout
                bool completed = completionEvent.Wait(TimeSpan.FromSeconds(connectTimeoutSec + maxEvents * 10));
                if (!completed)
                {
                    Console.Error.WriteLine($"[Consumer Error] Timeout waiting for {maxEvents} events. Only received {receivedCount}.");
                    exitCode = 3;
                }
            }
            else
            {
                Console.WriteLine("[Consumer] Listening for memory map events. Press ENTER to stop.");
                Console.ReadLine();
            }

            engine.Dispose();
            Console.WriteLine($"[Consumer] Disposed. Total events received: {receivedCount}");
            return exitCode;
        }
    }
}
