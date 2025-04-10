namespace TNP.App.Models
{
    /// <summary>
    /// Defines the types of connections between network elements.
    /// </summary>
    public enum ConnectionType
    {
        /// <summary>
        /// Standard Ethernet connection.
        /// </summary>
        Ethernet,

        /// <summary>
        /// Wireless connection.
        /// </summary>
        WiFi,

        /// <summary>
        /// Fiber optic connection.
        /// </summary>
        Fiber,

        /// <summary>
        /// Serial connection.
        /// </summary>
        Serial,

        /// <summary>
        /// USB connection.
        /// </summary>
        USB,

        /// <summary>
        /// Bluetooth connection.
        /// </summary>
        Bluetooth,

        /// <summary>
        /// Custom connection type.
        /// </summary>
        Custom
    }
}