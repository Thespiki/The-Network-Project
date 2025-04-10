using System;
using System.Collections.Generic;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a modem in the network.
    /// </summary>
    public class Modem : NetworkComponent
    {
        /// <summary>
        /// Gets the type name of the modem.
        /// </summary>
        public override string ComponentType => "Modem";

        /// <summary>
        /// Gets the path to the icon representing a modem.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/modem.png";

        /// <summary>
        /// Gets the maximum number of connections a modem can have.
        /// </summary>
        public override int MaxConnections => 4;

        /// <summary>
        /// Initializes a new instance of the <see cref="Modem"/> class.
        /// </summary>
        public Modem() : base()
        {
            Name = "Modem";
        }

        /// <summary>
        /// Initializes the default properties for a modem.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.0.1";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["WAN IP"] = "0.0.0.0"; // Will be assigned by ISP
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-M2000";
            Properties["Connection Type"] = "Cable"; // Cable, DSL, Fiber, Satellite
            Properties["Download Speed"] = 100; // Mbps
            Properties["Upload Speed"] = 20; // Mbps
            Properties["Router Mode"] = true;
            Properties["DHCP Enabled"] = true;
            Properties["DNS Server"] = "8.8.8.8";
            Properties["MAC Address"] = GenerateMacAddress();
            Properties["Firewall Enabled"] = true;
        }

        /// <summary>
        /// Validates the configuration of the modem.
        /// </summary>
        /// <param name="errorMessage">When this method returns, contains an error message if validation failed.</param>
        /// <returns>true if the configuration is valid; otherwise, false.</returns>
        public override bool ValidateConfiguration(out string errorMessage)
        {
            // Check IP Address format
            if (!Properties.TryGetValue("IP Address", out var ipAddressObj) || 
                !(ipAddressObj is string ipAddress) || 
                string.IsNullOrEmpty(ipAddress) ||
                !IsValidIpAddress(ipAddress))
            {
                errorMessage = "Invalid IP Address";
                return false;
            }

            // Check Subnet Mask format
            if (!Properties.TryGetValue("Subnet Mask", out var subnetMaskObj) || 
                !(subnetMaskObj is string subnetMask) || 
                string.IsNullOrEmpty(subnetMask) ||
                !IsValidIpAddress(subnetMask))
            {
                errorMessage = "Invalid Subnet Mask";
                return false;
            }

            // Check Download Speed
            if (!Properties.TryGetValue("Download Speed", out var downloadSpeedObj) || 
                !(downloadSpeedObj is double downloadSpeed) || 
                downloadSpeed <= 0)
            {
                errorMessage = "Invalid Download Speed";
                return false;
            }

            // Check Upload Speed
            if (!Properties.TryGetValue("Upload Speed", out var uploadSpeedObj) || 
                !(uploadSpeedObj is double uploadSpeed) || 
                uploadSpeed <= 0)
            {
                errorMessage = "Invalid Upload Speed";
                return false;
            }

            // Check DNS Server format if DHCP is enabled
            if (Properties.TryGetValue("DHCP Enabled", out var dhcpEnabledObj) && 
                dhcpEnabledObj is bool dhcpEnabled && 
                dhcpEnabled)
            {
                if (!Properties.TryGetValue("DNS Server", out var dnsServerObj) || 
                    !(dnsServerObj is string dnsServer) || 
                    string.IsNullOrEmpty(dnsServer) ||
                    !IsValidIpAddress(dnsServer))
                {
                    errorMessage = "Invalid DNS Server";
                    return false;
                }
            }

            errorMessage = string.Empty;
            return true;
        }

        /// <summary>
        /// Validates an IP address string.
        /// </summary>
        /// <param name="ipAddress">The IP address to validate.</param>
        /// <returns>true if the IP address is valid; otherwise, false.</returns>
        private bool IsValidIpAddress(string ipAddress)
        {
            string[] octets = ipAddress.Split('.');
            if (octets.Length != 4)
                return false;

            foreach (string octet in octets)
            {
                if (!int.TryParse(octet, out int value) || value < 0 || value > 255)
                    return false;
            }

            return true;
        }

        /// <summary>
        /// Generates a random MAC address.
        /// </summary>
        /// <returns>A string representing a MAC address.</returns>
        private string GenerateMacAddress()
        {
            var random = new Random();
            byte[] macBytes = new byte[6];
            random.NextBytes(macBytes);

            // Ensure the MAC address is unicast and locally administered
            macBytes[0] = (byte)((macBytes[0] & 0xFE) | 0x02);

            return string.Format("{0:X2}:{1:X2}:{2:X2}:{3:X2}:{4:X2}:{5:X2}",
                macBytes[0], macBytes[1], macBytes[2], macBytes[3], macBytes[4], macBytes[5]);
        }

        /// <summary>
        /// Creates a deep copy of this modem.
        /// </summary>
        /// <returns>A new instance of a modem with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Modem clone = new Modem
            {
                Id = Id,
                Name = Name,
                Position = Position,
                IsSelected = IsSelected
            };

            // Copy properties
            foreach (var kvp in Properties)
            {
                clone.Properties[kvp.Key] = kvp.Value;
            }

            return clone;
        }
    }
}
