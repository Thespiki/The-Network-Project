using System;
using System.Collections.Generic;
using System.Linq;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a wireless access point in the network.
    /// </summary>
    public class AccessPoint : NetworkComponent
    {
        private bool _accessPointEnabled;
        private string _ssid;
        private string _securityType;
        private string _password;

        /// <summary>
        /// Gets the type name of the access point.
        /// </summary>
        public override string ComponentType => "AccessPoint";

        /// <summary>
        /// Gets the path to the icon representing an access point.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/access-point.png";

        /// <summary>
        /// Gets the maximum number of connections an access point can have.
        /// </summary>
        public override int MaxConnections => 16;

        /// <summary>
        /// Initializes a new instance of the <see cref="AccessPoint"/> class.
        /// </summary>
        public AccessPoint() : base()
        {
            Name = "Access Point";
            _accessPointEnabled = true;
            _ssid = "TNP_Network";
            _securityType = "WPA2";
            _password = "password123";
        }

        /// <summary>
        /// Initializes the default properties for an access point.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.5";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "192.168.1.1";
            Properties["DNS Server"] = "8.8.8.8";
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-AP1000";
            Properties["Enabled"] = true;
            Properties["SSID"] = "TNP_Network";
            Properties["Security Type"] = "WPA2";
            Properties["Password"] = "password123";
            Properties["Channel"] = 6;
            Properties["Band"] = "2.4 GHz";
            Properties["Hidden"] = false;
            Properties["Bandwidth"] = 300; // Mbps
            Properties["Range"] = 30; // meters
        }

        /// <summary>
        /// Gets or sets a value indicating whether the access point is enabled.
        /// </summary>
        public bool AccessPointEnabled
        {
            get => _accessPointEnabled;
            set
            {
                if (_accessPointEnabled != value)
                {
                    _accessPointEnabled = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the SSID of the access point.
        /// </summary>
        public string SSID
        {
            get => _ssid;
            set
            {
                if (_ssid != value)
                {
                    _ssid = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the security type of the access point.
        /// </summary>
        public string SecurityType
        {
            get => _securityType;
            set
            {
                if (_securityType != value)
                {
                    _securityType = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the password of the access point.
        /// </summary>
        public string Password
        {
            get => _password;
            set
            {
                if (_password != value)
                {
                    _password = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Validates the configuration of the access point.
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

            // Check SSID
            if (!Properties.TryGetValue("SSID", out var ssidObj) || 
                !(ssidObj is string ssid) || 
                string.IsNullOrEmpty(ssid))
            {
                errorMessage = "SSID cannot be empty";
                return false;
            }

            // Check Password (if security type is not Open)
            if (Properties.TryGetValue("Security Type", out var securityTypeObj) && 
                securityTypeObj is string securityType && 
                securityType != "Open")
            {
                if (!Properties.TryGetValue("Password", out var passwordObj) || 
                    !(passwordObj is string password) || 
                    string.IsNullOrEmpty(password))
                {
                    errorMessage = "Password cannot be empty for secure networks";
                    return false;
                }

                if (securityType == "WPA2" && (password.Length < 8 || password.Length > 63))
                {
                    errorMessage = "WPA2 password must be between 8 and 63 characters";
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
        /// Creates a deep copy of this access point.
        /// </summary>
        /// <returns>A new instance of an access point with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            AccessPoint clone = new AccessPoint
            {
                Id = Id,
                Name = Name,
                IsSelected = IsSelected,
                AccessPointEnabled = AccessPointEnabled,
                SSID = SSID,
                SecurityType = SecurityType,
                Password = Password
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
