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
        private readonly MemoryMappedViewAccessor _accessor;
        
        private Thread? _listenerThread;
        private bool _isListening;
        private readonly int _maxSize;

        // The callback users will subscribe to
        public event Action<SharedDataset>? OnDataReady;

        public SharedValueEngine(string name, int maxSizeInBytes = 10 * 1024 * 1024)
        {
            _maxSize = maxSizeInBytes;
            string mapName = $@"Global\{name}_Map";
            string mutexName = $@"Global\{name}_Mutex";
            string eventName = $@"Global\{name}_Event";

            try
            {
                // 1. Open Memory Mapped File 
                _mmf = MemoryMappedFile.OpenExisting(mapName, MemoryMappedFileRights.ReadWrite);
                _accessor = _mmf.CreateViewAccessor(0, _maxSize);
            }
            catch (Exception ex)
            {
                throw new EngineInitializationException("MemoryMappedFile", ex);
            }

            try
            {
                // 2. Open Mutex
                _mutex = Mutex.OpenExisting(mutexName);
            }
            catch (Exception ex)
            {
                throw new EngineInitializationException("Mutex", ex);
            }

            try
            {
                // 3. Open Event
                _event = EventWaitHandle.OpenExisting(eventName);
            }
            catch (Exception ex)
            {
                throw new EngineInitializationException("EventWaitHandle", ex);
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
                    // Block the thread entirely (0% CPU) until the C++ producer fires SetEvent()
                    _event.WaitOne();

                    if (!_isListening) break; // We were woken up just to exit

                    SharedDataset dataset = ReadCurrentData();
                    
                    // Fire the C# callback!
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
                // Lock the mutex. If the C++ producer abandons it, we catch the exception.
                try
                {
                    acquired = _mutex.WaitOne(5000);
                }
                catch (AbandonedMutexException)
                {
                    acquired = true; // We own it now, but data might be part-way written
                    Console.WriteLine("[Warning] Mutex abandoned by C++ producer. Attempting to parse anyway.");
                }

                if (!acquired)
                    throw new EngineTimeoutException("Timed out waiting for Mutex during read.");

                // 1. Read the size of the flatbuffer payload (first 8 bytes for size_t, but realistically we only need 4 for sizes under 4GB)
                // Note: C++ size_t is 8 bytes on x64, 4 bytes on x86. Assuming 64-bit C++ producer.
                ulong dataSize = _accessor.ReadUInt64(0);
                if (dataSize > (ulong)_maxSize - 8)
                {
                    throw new EngineCorruptedException($"Invalid flatbuffer size detected: {dataSize}");
                }

                // 2. Read the bytes
                byte[] rawData = new byte[dataSize];
                _accessor.ReadArray(8, rawData, 0, (int)dataSize);

                // 3. Parse with FlatBuffers (Zero-copy unpacking logic applied per field access later)
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

        public void Dispose()
        {
            StopListening();
            _accessor?.Dispose();
            _mmf?.Dispose();
            _mutex?.Dispose();
            _event?.Dispose();
        }
    }
}
