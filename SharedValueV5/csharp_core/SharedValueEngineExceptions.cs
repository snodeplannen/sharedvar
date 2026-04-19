using System;

namespace SharedValueV5
{
    public class SharedValueException : Exception
    {
        public SharedValueException(string message) : base(message) { }
        public SharedValueException(string message, Exception innerException) : base(message, innerException) { }
    }

    public class EngineInitializationException : SharedValueException
    {
        public EngineInitializationException(string component, Exception innerException) 
            : base($"Failed to initialize {component}. Ensure the producer has created it, or check permissions.", innerException) { }
    }

    public class EngineTimeoutException : SharedValueException
    {
        public EngineTimeoutException(string message) : base(message) { }
    }

    public class EngineCorruptedException : SharedValueException
    {
        public EngineCorruptedException(string message) : base(message) { }
    }
}
