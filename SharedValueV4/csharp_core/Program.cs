using System;
using System.Threading;
using SharedMemMap;
using Google.FlatBuffers;

namespace csharp_core
{
    class Program
    {
        static int Main(string[] args)
        {
            string name = "MyGlobalDataset";
            int maxEvents = 0;          // 0 = infinite (manual exit)

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
                    case "--help":
                        Console.WriteLine("Usage: csharp_core [options]");
                        Console.WriteLine("Options:");
                        Console.WriteLine("  --name NAME       Shared memory base name (default: MyGlobalDataset)");
                        Console.WriteLine("  --count N         Exit after receiving N events (default: infinite)");
                        Console.WriteLine("  --help            Show this help");
                        return 0;
                }
            }

            Console.WriteLine("[Consumer] Starting MemMap Dual-Channel Client...");
            Console.WriteLine($"[Consumer] Config: name={name} count={(maxEvents == 0 ? "infinite" : maxEvents.ToString())}");
            Console.WriteLine("[Consumer] Waiting for C++ Producer to initialize the shared memory...");

            // isPrimaryHost = false because we wait for C++ to setup the mappings and set Ready Event.
            using var engineP2C = new SharedValueEngine($"{name}_P2C", isPrimaryHost: false);
            using var engineC2P = new SharedValueEngine($"{name}_C2P", isPrimaryHost: false);

            Console.WriteLine("\n[Consumer] Successfully connected to Dual-Channel MMFs!");

            int receivedCount = 0;
            using var completionEvent = new ManualResetEventSlim(false);

            engineP2C.OnDataReady += (dataset) =>
            {
                receivedCount++;
                Console.WriteLine($"\n--- Event #{receivedCount} Received from C++ ---");
                Console.WriteLine($"  API Version: {dataset.ApiVersion}");
                
                // Now let's SEND a message back!
                SendResponseToCpp(engineC2P, receivedCount);

                if (maxEvents > 0 && receivedCount >= maxEvents)
                {
                    Console.WriteLine($"[Consumer] Received {receivedCount}/{maxEvents} events. Auto-exiting.");
                    completionEvent.Set();
                }
            };

            engineP2C.StartListening();

            if (maxEvents > 0)
            {
                // Wait for the required number of events
                completionEvent.Wait();
            }
            else
            {
                Console.WriteLine("[Consumer] Listening for memory map events. Press ENTER to stop.");
                Console.ReadLine();
            }

            Console.WriteLine($"[Consumer] Exiting. Total events received: {receivedCount}");
            return 0;
        }

        static void SendResponseToCpp(SharedValueEngine engine, int counter)
        {
            var builder = new FlatBufferBuilder(1024);
            
            // Craft a simple row to talk back to C++
            var idOffset = builder.CreateString($"Response_from_CSharp_{counter}");
            DataRow.StartDataRow(builder);
            DataRow.AddRowId(builder, idOffset);
            DataRow.AddIsActive(builder, true);
            var rowOffset = DataRow.EndDataRow(builder);

            var rowsVector = SharedDataset.CreateRowsVector(builder, new[] { rowOffset });

            var apiOffset = builder.CreateString("2.0.0-CSharpResponse");
            SharedDataset.StartSharedDataset(builder);
            SharedDataset.AddApiVersion(builder, apiOffset);
            SharedDataset.AddRows(builder, rowsVector);
            var root = SharedDataset.EndSharedDataset(builder);

            builder.Finish(root.Value);

            engine.WriteData(builder.SizedByteArray());
            Console.WriteLine($"[Consumer] Replied to C++ with acknowledge counter {counter}");
        }
    }
}
