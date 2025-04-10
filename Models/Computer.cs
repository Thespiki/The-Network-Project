using System;
using System.Collections.Generic;
using System.Linq;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a computer in the network.
    /// </summary>
    public class Computer : NetworkComponent
    {
        private string _hostname;
        private string _operatingSystem;

        /// <summary>
        /// Gets the type name of the computer.
        /// </summary>
        public override string ComponentType => "Computer";

        /// <summary>
        /// Gets the path to the icon representing a computer.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/computer.png";

        /// <summary>
        /// Gets the maximum number of connections a computer can have.
        /// </summary>
        public override int MaxConnections => 4;

        /// <summary>
        /// Initializes a new instance of the <see cref="Computer"/> class.
        /// </summary>
        public Computer() : base()
        {
            Name = "Computer";
            _hostname = "DESKTOP-TNP";
            _operatingSystem = "Windows 10";
        }

        /// <summary>
        /// Initializes the default properties for a computer.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.10";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "192.168.1.1";
            Properties["DNS Server"] = "8.8.8.8";
            Properties["Hostname"] = "DESKTOP-TNP";
            Properties["Operating System"] = "Windows 10";
            Properties["CPU"] = "Intel Core i5";
            Properties["RAM"] = "16 GB";
            Properties["Storage"] = "512 GB SSD";
            Properties["MAC Address"] = "00:11:22:33:44:55";
            Properties["DHCP Enabled"] = true;
            Properties["Wireless"] = true;
            Properties["Firewall Enabled"] = true;
            Properties["Antivirus"] = "Generic Antivirus";
            Properties["Domain"] = "";
        }

        /// <summary>
        /// Gets or sets the hostname of the computer.
        /// </summary>
        public string Hostname
        {
            get => _hostname;
            set
            {
                if (_hostname != value)
                {
                    _hostname = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the operating system of the computer.
        /// </summary>
        public string OperatingSystem
        {
            get => _operatingSystem;
            set
            {
                if (_operatingSystem != value)
                {
                    _operatingSystem = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Validates the configuration of the computer.
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

            // Check Hostname
            if (string.IsNullOrEmpty(Hostname))
            {
                errorMessage = "Hostname cannot be empty";
                return false;
            }

            // Check Operating System
            if (string.IsNullOrEmpty(OperatingSystem))
            {
                errorMessage = "Operating System cannot be empty";
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
        /// Creates a deep copy of this computer.
        /// </summary>
        /// <returns>A new instance of a computer with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Computer clone = new Computer
            {
                Id = Id,
                Name = Name,
                IsSelected = IsSelected,
                Hostname = Hostname,
                OperatingSystem = OperatingSystem
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
