using System;
using System.Collections.Generic;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a network switch in the network.
    /// </summary>
    public class Switch : NetworkComponent
    {
        /// <summary>
        /// Gets the type name of the switch.
        /// </summary>
        public override string ComponentType => "Switch";

        /// <summary>
        /// Gets the path to the icon representing a switch.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/switch.png";

        /// <summary>
        /// Gets the maximum number of connections a switch can have.
        /// </summary>
        public override int MaxConnections => 24;

        /// <summary>
        /// Initializes a new instance of the <see cref="Switch"/> class.
        /// </summary>
        public Switch() : base()
        {
            Name = "Switch";
        }

        /// <summary>
        /// Initializes the default properties for a switch.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.2";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "192.168.1.1";
            Properties["Management VLAN"] = 1;
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-S2400";
            Properties["Port Count"] = 24;
            Properties["VLAN Support"] = true;
            Properties["VLAN Count"] = 10;
            Properties["PoE Support"] = false;
            Properties["Managed"] = true;
            Properties["Layer"] = 2; // Layer 2 switch
            Properties["Bandwidth"] = 1000; // 1 Gbps
            Properties["Backplane Capacity"] = 48; // 48 Gbps
        }

        /// <summary>
        /// Validates the configuration of the switch.
        /// </summary>
        /// <param name="errorMessage">When this method returns, contains an error message if validation failed.</param>
        /// <returns>true if the configuration is valid; otherwise, false.</returns>
        public override bool ValidateConfiguration(out string errorMessage)
        {
            // Check IP Address format if switch is managed
            if (Properties.TryGetValue("Managed", out var managedObj) && 
                managedObj is bool managed && 
                managed)
            {
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
            }

            // Check Port Count
            if (!Properties.TryGetValue("Port Count", out var portCountObj) || 
                !(portCountObj is int portCount) || 
                portCount <= 0)
            {
                errorMessage = "Invalid Port Count";
                return false;
            }

            // Check VLAN Count if VLAN Support is enabled
            if (Properties.TryGetValue("VLAN Support", out var vlanSupportObj) && 
                vlanSupportObj is bool vlanSupport && 
                vlanSupport)
            {
                if (!Properties.TryGetValue("VLAN Count", out var vlanCountObj) || 
                    !(vlanCountObj is int vlanCount) || 
                    vlanCount <= 0)
                {
                    errorMessage = "Invalid VLAN Count";
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
        /// Creates a deep copy of this switch.
        /// </summary>
        /// <returns>A new instance of a switch with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Switch clone = new Switch
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
