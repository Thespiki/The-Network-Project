using System;
using System.Collections.Generic;
using System.Linq;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a firewall in the network.
    /// </summary>
    public class Firewall : NetworkComponent
    {
        private string _firewallRules;

        /// <summary>
        /// Gets the type name of the firewall.
        /// </summary>
        public override string ComponentType => "Firewall";

        /// <summary>
        /// Gets the path to the icon representing a firewall.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/firewall.png";

        /// <summary>
        /// Gets the maximum number of connections a firewall can have.
        /// </summary>
        public override int MaxConnections => 10;

        /// <summary>
        /// Gets or sets the firewall rules.
        /// </summary>
        public string FirewallRules
        {
            get => _firewallRules;
            set
            {
                if (_firewallRules != value)
                {
                    _firewallRules = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="Firewall"/> class.
        /// </summary>
        public Firewall() : base()
        {
            Name = "Firewall";
            _firewallRules = "Default Rules";
        }

        /// <summary>
        /// Initializes the default properties for a firewall.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.254";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "192.168.1.1";
            Properties["DNS Server"] = "8.8.8.8";
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-FW1000";
            Properties["Throughput"] = 1000; // Mbps
            Properties["Stateful"] = true;
            Properties["IPS Enabled"] = true;
            Properties["IDS Enabled"] = true;
            Properties["VPN Support"] = true;
            Properties["NAT Enabled"] = true;
            Properties["Log Level"] = "Info";
            Properties["Rule Processing"] = "First Match";
            Properties["Policy"] = "Default Deny";
        }

        /// <summary>
        /// Validates the configuration of the firewall.
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

            // Check firewall rules (implementation-specific validation)
            if (string.IsNullOrEmpty(FirewallRules))
            {
                errorMessage = "Firewall rules cannot be empty";
                return false;
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
        /// Creates a deep copy of this firewall.
        /// </summary>
        /// <returns>A new instance of a firewall with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Firewall clone = new Firewall
            {
                Id = Id,
                Name = Name,
                IsSelected = IsSelected,
                FirewallRules = FirewallRules
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
