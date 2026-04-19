using System;
using System.IO.MemoryMappedFiles;
using System.Threading;
using SharedMemMap; // Generated FlatBuffers namespace
using Google.FlatBuffers;

namespace csharp_core
{
    public class SharedValueEngine : IDisposable
    {
        private readonly MemoryMappedFile _mmf;
        private readonly Mutex _mutex;
        private readonly EventWaitHandle _event;
        private readonly EventWaitHandle? _readyEvent;
        private readonly MemoryMappedViewAccessor _accessor;
        
        private Thread? _listenerThread;
        private bool _isListening;
        private readonly int _maxSize;

        // The callback users will subscribe to
        public event Action<SharedDataset>? OnDataReady;

        public SharedValueEngine(string name, bool isPrimaryHost, int maxSizeInBytes = 10 * 1024 * 1024)
        {
            _maxSize = maxSizeInBytes;
            string mapName = $@"Global\{name}_Map";
            string mutexName = $@"Global\{name}_Mutex";
            string eventName = $@"Global\{name}_Event";
            string readyEventName = $@"Global\{name}_Ready";

            if (!isPrimaryHost)
            {
                Console.WriteLine($"[Engine] Waiting for primary host Ready Event: {readyEventName}");
                bool readyEventOpened = false;
                while (!readyEventOpened)
                {
                    try
                    {
                        using var tempReadyEvent = EventWaitHandle.OpenExisting(readyEventName);
                        tempReadyEvent.WaitOne();
                        readyEventOpened = true;
                    }
                    catch (WaitHandleCannotBeOpenedException)
                    {
                        // Producer hasn't created it yet. Sleep to avoid wasting CPU
                        Thread.Sleep(50);
                    }
                }
            }

            try
            {
                if (isPrimaryHost)
                {
                    _mmf = MemoryMappedFile.CreateOrOpen(mapName, _maxSize);
                    _mutex = new Mutex(false, mutexName);
                    _event = new EventWaitHandle(false, EventResetMode.AutoReset, eventName);
                }
                else
                {
                    _mmf = MemoryMappedFile.OpenExisting(mapName, MemoryMappedFileRights.ReadWrite);
                    _mutex = Mutex.OpenExisting(mutexName);
                    _event = EventWaitHandle.OpenExisting(eventName);
                }
                _accessor = _mmf.CreateViewAccessor(0, _maxSize);
            }
            catch (Exception ex)
            {
                throw new EngineInitializationException("Kernel Objects", ex);
            }

            if (isPrimaryHost)
            {
                // Primary host creates and signals the ready event
                _readyEvent = new EventWaitHandle(true, EventResetMode.ManualReset, readyEventName);
            }
        }

        public void StartListening()
        {
            if (_isListening) return;
            _isListening = true;

            _listenerThread = new Thread(ListenLoop)
            {
                IsBackground = true, // Die when main process dies
                Name = "SharedValue_EventListener"
            };
            _listenerThread.Start();
        }

        public void StopListening()
        {
            _isListening = false;
            _event?.Set(); // Wake up the thread so it can exit
        }

        private void ListenLoop()
        {
            while (_isListening)
            {
                try
                {
                    // Block the thread entirely (0% CPU) until the producer fires SetEvent()
                    _event.WaitOne();

                    if (!_isListening) break; // Woken up to exit

                    SharedDataset dataset = ReadCurrentData();
                    
                    // Fire the C# callback
                    OnDataReady?.Invoke(dataset);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[Listener Error] {ex.Message}");
                }
            }
        }

        private SharedDataset ReadCurrentData()
        {
            bool acquired = false;
            try
            {
                try
                {
                    acquired = _mutex.WaitOne(5000);
                }
                catch (AbandonedMutexException)
                {
                    acquired = true; 
                    Console.WriteLine("[Warning] Mutex abandoned by producer. Attempting to parse anyway.");
                }

                if (!acquired)
                    throw new EngineTimeoutException("Timed out waiting for Mutex during read.");

                ulong dataSize = _accessor.ReadUInt64(0);
                if (dataSize > (ulong)_maxSize - 8)
                {
                    throw new EngineCorruptedException($"Invalid flatbuffer size detected: {dataSize}");
                }

                byte[] rawData = new byte[dataSize];
                _accessor.ReadArray(8, rawData, 0, (int)dataSize);

                ByteBuffer bb = new ByteBuffer(rawData);
                return SharedDataset.GetRootAsSharedDataset(bb);
            }
            finally
            {
                if (acquired)
                {
                    _mutex.ReleaseMutex();
                }
            }
        }

        public void WriteData(byte[] data)
        {
            if (data.Length + 8 > _maxSize)
            {
                throw new Exception("Data size exceeds shared memory capacity.");
            }

            bool acquired = false;
            try
            {
                try
                {
                    acquired = _mutex.WaitOne(5000);
                }
                catch (AbandonedMutexException)
                {
                    acquired = true;
                    Console.WriteLine("[Warning] Mutex abandoned. Writing anyway.");
                }

                if (!acquired)
                    throw new EngineTimeoutException("Timed out waiting for Mutex during write.");

                // Write size (8 bytes for ulong compatibility with size_t in 64-bit C++)
                _accessor.Write(0, (ulong)data.Length);

                // Write data
                _accessor.WriteArray(8, data, 0, data.Length);

                // Signal event
                _event.Set();
            }
            finally
            {
                if (acquired)
                {
                    _mutex.ReleaseMutex();
                }
            }
        }

        public void Dispose()
        {
            StopListening();
            _accessor?.Dispose();
            _mmf?.Dispose();
            _mutex?.Dispose();
            _event?.Dispose();
            _readyEvent?.Dispose();
        }
    }
}
