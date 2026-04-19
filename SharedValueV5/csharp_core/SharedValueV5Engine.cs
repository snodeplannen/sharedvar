using System;
using System.Threading;

namespace SharedValueV5
{
    /// <summary>
    /// High-level SharedValueV5 engine that combines Dual-Channel IPC transport
    /// with dynamic SharedDataSet serialization.
    /// 
    /// The C2P (reverse) channel is initialized in a background thread
    /// to avoid blocking when the other side hasn't started yet.
    /// </summary>
    public class SharedValueV5Engine : IDisposable
    {
        private readonly string _channelName;
        private readonly bool _isHost;
        private readonly int _maxSize;

        private SharedValueEngine? _engineP2C;
        private SharedValueEngine? _engineC2P;
        private volatile bool _c2pReady;
        private Thread? _c2pInitThread;

        /// <summary>Fired when a new SharedDataSet arrives on the P2C channel</summary>
        public event Action<SharedDataSet>? OnDataReady;

        /// <summary>Fired when a new SharedDataSet arrives on the C2P (reverse) channel</summary>
        public event Action<SharedDataSet>? OnReverseDataReady;

        public SharedValueV5Engine(string channelName, bool isHost, int maxSizeInBytes = 10 * 1024 * 1024)
        {
            _channelName = channelName;
            _isHost = isHost;
            _maxSize = maxSizeInBytes;

            // Initialize only the primary channel immediately
            _engineP2C = new SharedValueEngine($"{channelName}_P2C", isPrimaryHost: isHost, maxSizeInBytes);

            // Wire up P2C deserialization
            _engineP2C.OnDataReady += rawData =>
            {
                try
                {
                    var ds = SharedDataSet.Deserialize(rawData);
                    OnDataReady?.Invoke(ds);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[V5Engine] Failed to deserialize P2C data: {ex.Message}");
                }
            };
        }

        /// <summary>Publish a SharedDataSet to the forward (P2C) channel</summary>
        public void Publish(SharedDataSet dataSet)
        {
            byte[] data = dataSet.Serialize();
            _engineP2C!.WriteData(data);
        }

        /// <summary>Publish a SharedDataSet to the reverse (C2P) channel.
        /// Silently skipped if the reverse channel is not yet connected.</summary>
        public void PublishReverse(SharedDataSet dataSet)
        {
            if (!_c2pReady || _engineC2P == null) return;
            byte[] data = dataSet.Serialize();
            _engineC2P.WriteData(data);
        }

        /// <summary>Start listening on the forward (P2C) channel</summary>
        public void StartListening() => _engineP2C!.StartListening();

        /// <summary>Start listening on the reverse (C2P) channel.
        /// Initializes the C2P engine in a background thread so it never blocks the caller.</summary>
        public void StartListeningReverse()
        {
            _c2pInitThread = new Thread(() =>
            {
                try
                {
                    // This may block waiting for the other side's Ready Event
                    var engine = new SharedValueEngine($"{_channelName}_C2P", isPrimaryHost: !_isHost, _maxSize);

                    engine.OnDataReady += rawData =>
                    {
                        try
                        {
                            var ds = SharedDataSet.Deserialize(rawData);
                            OnReverseDataReady?.Invoke(ds);
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"[V5Engine] Failed to deserialize C2P data: {ex.Message}");
                        }
                    };

                    engine.StartListening();
                    _engineC2P = engine;
                    _c2pReady = true;
                    Console.WriteLine("[V5Engine] C2P reverse channel connected.");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[V5Engine] Failed to init C2P: {ex.Message}");
                }
            })
            {
                IsBackground = true,
                Name = "SharedValueV5_C2P_Init"
            };
            _c2pInitThread.Start();
        }

        /// <summary>Stop all listening</summary>
        public void StopListening()
        {
            _engineP2C?.StopListening();
            _engineC2P?.StopListening();
        }

        public void Dispose()
        {
            StopListening();
            _engineP2C?.Dispose();
            _engineC2P?.Dispose();
        }
    }
}
