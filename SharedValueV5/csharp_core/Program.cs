using System;
using System.Threading;

namespace SharedValueV5
{
    class Program
    {
        static int Main(string[] args)
        {
            string channelName = "V5Demo";
            int maxEvents = 0;
            bool isHost = false;

            for (int i = 0; i < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--name" when i + 1 < args.Length:
                        channelName = args[++i];
                        break;
                    case "--count" when i + 1 < args.Length:
                        maxEvents = int.Parse(args[++i]);
                        break;
                    case "--host":
                        isHost = true;
                        break;
                    case "--help":
                        Console.WriteLine("Usage: csharp_core [options]");
                        Console.WriteLine("Options:");
                        Console.WriteLine("  --name NAME       Channel name (default: V5Demo)");
                        Console.WriteLine("  --count N         Exit after N events (default: infinite)");
                        Console.WriteLine("  --host            Run as host/producer (default: consumer)");
                        Console.WriteLine("  --help            Show this help");
                        return 0;
                }
            }

            if (isHost)
                return RunProducer(channelName);
            else
                return RunConsumer(channelName, maxEvents);
        }

        static int RunProducer(string channelName)
        {
            Console.WriteLine("[Producer] Starting SharedValueV5 Producer...");

            // 1. Define the dataset with dynamic schema
            var ds = new SharedDataSet("DemoDataSet");

            // Table 1: Sensors
            var sensoren = ds.Tables.Add("Sensoren");
            sensoren.AddColumn("SensorId", ColumnType.String);
            sensoren.AddColumn("Temperature", ColumnType.Double);
            sensoren.AddColumn("Humidity", ColumnType.Double);
            sensoren.AddColumn("IsActive", ColumnType.Bool);
            sensoren.AddColumn("StatusCode", ColumnType.Int32);
            sensoren.AddColumn("LastSeen", ColumnType.DateTime);

            // Table 2: Alarms
            var alarmen = ds.Tables.Add("Alarmen");
            alarmen.AddColumn("AlarmId", ColumnType.Int32);
            alarmen.AddColumn("Bericht", ColumnType.String);
            alarmen.AddColumn("Severity", ColumnType.Int32);

            // Lock schemas
            sensoren.LockSchema();
            alarmen.LockSchema();

            Console.WriteLine($"[Producer] Schema defined: {ds.Tables.Count} tables");
            Console.WriteLine($"[Producer]   Sensoren: {sensoren.Columns.Count} columns (locked)");
            Console.WriteLine($"[Producer]   Alarmen: {alarmen.Columns.Count} columns (locked)");

            // 2. Start engine
            using var engine = new SharedValueV5Engine(channelName, isHost: true);

            // 3. Listen for reverse channel (bidirectional)
            engine.OnReverseDataReady += (incoming) =>
            {
                Console.WriteLine($"\n[Producer] Received reverse data with {incoming.Tables.Count} tables!");
                foreach (var table in incoming.Tables)
                {
                    Console.WriteLine($"  Table: {table.Name} ({table.Rows.Count} rows)");
                    for (int r = 0; r < table.Rows.Count; r++)
                    {
                        var row = table.Rows[r];
                        Console.Write("    ");
                        for (int c = 0; c < table.Columns.Count; c++)
                        {
                            Console.Write($"{table.Columns[c].Name}={row[c]} ");
                        }
                        Console.WriteLine();
                    }
                }
            };
            engine.StartListeningReverse();

            // 4. Publish data in a loop
            int counter = 0;
            Console.WriteLine("[Producer] Publishing data. Press ENTER to stop.\n");

            var publishThread = new Thread(() =>
            {
                var rng = new Random();
                while (true)
                {
                    counter++;
                    sensoren.Rows.Clear();
                    alarmen.Rows.Clear();

                    // Add sensor rows
                    for (int s = 0; s < 3; s++)
                    {
                        var row = sensoren.NewRow();
                        row["SensorId"] = $"Sensor_{s + 1:D2}";
                        row["Temperature"] = 18.0 + rng.NextDouble() * 15.0;
                        row["Humidity"] = 40.0 + rng.NextDouble() * 40.0;
                        row["IsActive"] = rng.NextDouble() > 0.2;
                        row["StatusCode"] = rng.NextDouble() > 0.9 ? 503 : 200;
                        row["LastSeen"] = DateTime.UtcNow;
                        sensoren.Rows.Add(row);
                    }

                    // Add alarm row
                    if (counter % 3 == 0)
                    {
                        var alarm = alarmen.NewRow();
                        alarm["AlarmId"] = 1000 + counter;
                        alarm["Bericht"] = $"Automatisch alarm #{counter}";
                        alarm["Severity"] = rng.Next(1, 5);
                        alarmen.Rows.Add(alarm);
                    }

                    engine.Publish(ds);
                    Console.Write($"\r[Producer] Published update #{counter} ({sensoren.Rows.Count} sensors, {alarmen.Rows.Count} alarms)   ");

                    Thread.Sleep(1000);
                }
            }) { IsBackground = true };
            publishThread.Start();

            Console.ReadLine();
            Console.WriteLine($"\n[Producer] Exiting. Total updates: {counter}");
            return 0;
        }

        static int RunConsumer(string channelName, int maxEvents)
        {
            Console.WriteLine("[Consumer] Starting SharedValueV5 Consumer...");
            Console.WriteLine($"[Consumer] Channel: {channelName}, MaxEvents: {(maxEvents == 0 ? "infinite" : maxEvents.ToString())}");
            Console.WriteLine("[Consumer] Waiting for producer...");

            using var engine = new SharedValueV5Engine(channelName, isHost: false);

            int receivedCount = 0;
            using var completionEvent = new ManualResetEventSlim(false);

            engine.OnDataReady += (dataSet) =>
            {
                receivedCount++;
                Console.WriteLine($"\n--- Event #{receivedCount} ---");

                // Schema autodiscovery!
                foreach (var table in dataSet.Tables)
                {
                    Console.WriteLine($"  Table: {table.Name} (v{table.SchemaVersion}, {(table.IsSchemaLocked ? "locked" : "open")})");

                    // Print column headers
                    Console.Write("    ");
                    foreach (var col in table.Columns)
                        Console.Write($"{col.Name}({col.Type})\t");
                    Console.WriteLine();

                    // Print rows
                    for (int r = 0; r < table.Rows.Count; r++)
                    {
                        var row = table.Rows[r];
                        Console.Write("    ");
                        for (int c = 0; c < table.Columns.Count; c++)
                        {
                            var val = row[c];
                            if (val is DateTime dt)
                                Console.Write($"{dt:HH:mm:ss}\t");
                            else
                                Console.Write($"{val}\t");
                        }
                        Console.WriteLine();
                    }
                }

                // Send acknowledgement back (bidirectional)
                SendAck(engine, receivedCount);

                if (maxEvents > 0 && receivedCount >= maxEvents)
                {
                    Console.WriteLine($"\n[Consumer] Received {receivedCount}/{maxEvents} events. Exiting.");
                    completionEvent.Set();
                }
            };

            engine.StartListening();

            if (maxEvents > 0)
            {
                completionEvent.Wait();
            }
            else
            {
                Console.WriteLine("[Consumer] Listening. Press ENTER to stop.");
                Console.ReadLine();
            }

            Console.WriteLine($"[Consumer] Total events: {receivedCount}");
            return 0;
        }

        static void SendAck(SharedValueV5Engine engine, int counter)
        {
            var ds = new SharedDataSet("Ack");
            var ackTable = ds.Tables.Add("Acknowledgements");
            ackTable.AddColumn("Counter", ColumnType.Int32);
            ackTable.AddColumn("Message", ColumnType.String);
            ackTable.AddColumn("Timestamp", ColumnType.DateTime);

            var row = ackTable.NewRow();
            row["Counter"] = counter;
            row["Message"] = $"ACK from C# consumer #{counter}";
            row["Timestamp"] = DateTime.UtcNow;
            ackTable.Rows.Add(row);

            engine.PublishReverse(ds);
        }
    }
}
