using System;
using System.Collections.Generic;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a router in the network.
    /// </summary>
    public class Router : NetworkComponent
    {
        /// <summary>
        /// Gets the type name of the router.
        /// </summary>
        public override string ComponentType => "Router";

        /// <summary>
        /// Gets the path to the icon representing a router.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/router.png";

        /// <summary>
        /// Gets the maximum number of connections a router can have.
        /// </summary>
        public override int MaxConnections => 8;

        /// <summary>
        /// Initializes a new instance of the <see cref="Router"/> class.
        /// </summary>
        public Router() : base()
        {
            Name = "Router";
        }

        /// <summary>
        /// Initializes the default properties for a router.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.1";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "";
            Properties["DHCP Enabled"] = true;
            Properties["DNS Server"] = "8.8.8.8";
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-R1000";
            Properties["Wireless"] = true;
            Properties["Wireless SSID"] = "TNP_Network";
            Properties["Wireless Security"] = "WPA2";
            Properties["Wireless Password"] = "password123";
            Properties["Firewall Enabled"] = true;
            Properties["NAT Enabled"] = true;
            Properties["QoS Enabled"] = false;
            Properties["Bandwidth"] = 1000; // 1 Gbps
        }

        /// <summary>
        /// Validates the configuration of the router.
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

            // Check Default Gateway format (can be empty)
            if (Properties.TryGetValue("Default Gateway", out var gatewayObj) && 
                gatewayObj is string gateway && 
                !string.IsNullOrEmpty(gateway) && 
                !IsValidIpAddress(gateway))
            {
                errorMessage = "Invalid Default Gateway";
                return false;
            }

            // Check DNS Server format
            if (!Properties.TryGetValue("DNS Server", out var dnsServerObj) || 
                !(dnsServerObj is string dnsServer) || 
                string.IsNullOrEmpty(dnsServer) ||
                !IsValidIpAddress(dnsServer))
            {
                errorMessage = "Invalid DNS Server";
                return false;
            }

            // Check Wireless SSID if Wireless is enabled
            if (Properties.TryGetValue("Wireless", out var wirelessObj) && 
                wirelessObj is bool wireless && 
                wireless)
            {
                if (!Properties.TryGetValue("Wireless SSID", out var ssidObj) || 
                    !(ssidObj is string ssid) || 
                    string.IsNullOrEmpty(ssid))
                {
                    errorMessage = "Invalid Wireless SSID";
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
        /// Creates a deep copy of this router.
        /// </summary>
        /// <returns>A new instance of a router with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Router clone = new Router
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
