using System;
using System.Collections.Generic;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a server in the network.
    /// </summary>
    public class Server : NetworkComponent
    {
        /// <summary>
        /// Gets the type name of the server.
        /// </summary>
        public override string ComponentType => "Server";

        /// <summary>
        /// Gets the path to the icon representing a server.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/server.png";

        /// <summary>
        /// Gets the maximum number of connections a server can have.
        /// </summary>
        public override int MaxConnections => 4;

        /// <summary>
        /// Initializes a new instance of the <see cref="Server"/> class.
        /// </summary>
        public Server() : base()
        {
            Name = "Server";
        }

        /// <summary>
        /// Initializes the default properties for a server.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.10";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "192.168.1.1";
            Properties["DNS Server"] = "8.8.8.8";
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-S5000";
            Properties["Operating System"] = "TNP Server OS";
            Properties["OS Version"] = "1.0";
            Properties["CPU"] = "Generic CPU";
            Properties["CPU Cores"] = 8;
            Properties["Memory"] = 64; // GB
            Properties["Storage"] = 2000; // GB
            Properties["Server Type"] = "Web Server"; // Web, Database, File, Application, etc.
            Properties["Services"] = new List<string> { "HTTP", "HTTPS", "FTP" };
            Properties["RAID Level"] = 5;
            Properties["Redundant Power"] = true;
            Properties["Virtualization"] = false;
        }

        /// <summary>
        /// Validates the configuration of the server.
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

            // Check Default Gateway format
            if (!Properties.TryGetValue("Default Gateway", out var gatewayObj) || 
                !(gatewayObj is string gateway) || 
                string.IsNullOrEmpty(gateway) ||
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

            // Check CPU Cores
            if (!Properties.TryGetValue("CPU Cores", out var cpuCoresObj) || 
                !(cpuCoresObj is int cpuCores) || 
                cpuCores <= 0)
            {
                errorMessage = "Invalid CPU Cores";
                return false;
            }

            // Check Memory
            if (!Properties.TryGetValue("Memory", out var memoryObj) || 
                !(memoryObj is int memory) || 
                memory <= 0)
            {
                errorMessage = "Invalid Memory";
                return false;
            }

            // Check Storage
            if (!Properties.TryGetValue("Storage", out var storageObj) || 
                !(storageObj is int storage) || 
                storage <= 0)
            {
                errorMessage = "Invalid Storage";
                return false;
            }

            // Check Services
            if (!Properties.TryGetValue("Services", out var servicesObj) || 
                !(servicesObj is List<string> services) || 
                services.Count == 0)
            {
                errorMessage = "No Services configured";
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
        /// Creates a deep copy of this server.
        /// </summary>
        /// <returns>A new instance of a server with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Server clone = new Server
            {
                Id = Id,
                Name = Name,
                Position = Position,
                IsSelected = IsSelected
            };

            // Copy properties
            foreach (var kvp in Properties)
            {
                // Deep copy for collections
                if (kvp.Value is List<string> services)
                {
                    clone.Properties[kvp.Key] = new List<string>(services);
                }
                else
                {
                    clone.Properties[kvp.Key] = kvp.Value;
                }
            }

            return clone;
        }
    }
}
