using System;
using System.IO.MemoryMappedFiles;
using System.Threading;

namespace SharedValueV5
{
    /// <summary>
    /// Low-level IPC transport engine using Memory-Mapped Files, Named Mutex, and Named Events.
    /// This is the V4 engine adapted for V5: it operates on raw byte[] instead of FlatBuffers.
    /// </summary>
    public class SharedValueEngine : IDisposable
    {
        private readonly MemoryMappedFile _mmf;
        private readonly Mutex _mutex;
        private readonly EventWaitHandle _event;
        private readonly EventWaitHandle? _readyEvent;
        private readonly MemoryMappedViewAccessor _accessor;
        
        private Thread? _listenerThread;
        private volatile bool _isListening;
        private readonly int _maxSize;

        /// <summary>Fired when new data is available in the shared memory</summary>
        public event Action<byte[]>? OnDataReady;

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
                _readyEvent = new EventWaitHandle(true, EventResetMode.ManualReset, readyEventName);
            }
        }

        public void StartListening()
        {
            if (_isListening) return;
            _isListening = true;

            _listenerThread = new Thread(ListenLoop)
            {
                IsBackground = true,
                Name = "SharedValueV5_EventListener"
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
                    _event.WaitOne();
                    if (!_isListening) break;

                    byte[] rawData = ReadRawData();
                    OnDataReady?.Invoke(rawData);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[Listener Error] {ex.Message}");
                }
            }
        }

        /// <summary>Read raw bytes from shared memory under mutex protection</summary>
        public byte[] ReadRawData()
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
                    Console.WriteLine("[Warning] Mutex abandoned by other process.");
                }

                if (!acquired)
                    throw new EngineTimeoutException("Timed out waiting for Mutex during read.");

                ulong dataSize = _accessor.ReadUInt64(0);
                if (dataSize > (ulong)_maxSize - 8)
                    throw new EngineCorruptedException($"Invalid data size: {dataSize}");

                byte[] rawData = new byte[dataSize];
                _accessor.ReadArray(8, rawData, 0, (int)dataSize);
                return rawData;
            }
            finally
            {
                if (acquired) _mutex.ReleaseMutex();
            }
        }

        /// <summary>Write raw bytes to shared memory under mutex protection, then signal event</summary>
        public void WriteData(byte[] data)
        {
            if (data.Length + 8 > _maxSize)
                throw new InvalidOperationException("Data size exceeds shared memory capacity.");

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

                _accessor.Write(0, (ulong)data.Length);
                _accessor.WriteArray(8, data, 0, data.Length);
                _event.Set();
            }
            finally
            {
                if (acquired) _mutex.ReleaseMutex();
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
