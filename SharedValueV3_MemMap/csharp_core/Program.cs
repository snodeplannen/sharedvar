using System;
using System.Threading;
using SharedMemMap;

namespace csharp_core
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("[Consumer] Starting MemMap Consumer...");
            Console.WriteLine("[Consumer] Waiting for C++ Producer to initialize the shared memory...");

            SharedValueEngine? engine = null;

            // Simple retry loop until producer is online
            while (engine == null)
            {
                try
                {
                    engine = new SharedValueEngine("MyGlobalDataset");
                }
                catch (EngineInitializationException)
                {
                    Thread.Sleep(1000);
                }
            }

            Console.WriteLine("[Consumer] Successfully connected to Shared Memory & Mutex!");

            long lastUpdate = DateTimeOffset.Now.ToUnixTimeMilliseconds();

            // Subscribe to the central callback
            engine.OnDataReady += (dataset) =>
            {
                long now = DateTimeOffset.Now.ToUnixTimeMilliseconds();
                long latency = now - lastUpdate;
                lastUpdate = now;

                Console.WriteLine($"\n--- New Event Triggered! (Latency/Pacing: {latency}ms) ---");
                Console.WriteLine($"API Version: {dataset.ApiVersion}");
                Console.WriteLine($"Last Updated: {dataset.LastUpdatedUtc}");
                
                int rows = dataset.RowsLength;
                Console.WriteLine($"Found {rows} Rows in Shared Memory:");

                for (int i = 0; i < rows; i++)
                {
                    DataRow? row = dataset.Rows(i);
                    if (row.HasValue)
                    {
                        Console.WriteLine($" -> Row ID: {row.Value.RowId}, Active: {row.Value.IsActive}");
                        
                        // Parse nested details
                        int detailsLength = row.Value.DetailsLength;
                        for (int j = 0; j < detailsLength; j++)
                        {
                            NestedDetails? details = row.Value.Details(j);
                            if (details.HasValue)
                            {
                                Console.WriteLine($"    -> Temp: {details.Value.Temperature:F1}, Hum: {details.Value.Humidity:F1}");
                            }
                        }
                    }
                }
            };

            engine.StartListening();

            Console.WriteLine("[Consumer] Listening for memory map events. Press ENTER to stop.");
            Console.ReadLine();

            engine.Dispose();
        }
    }
}
