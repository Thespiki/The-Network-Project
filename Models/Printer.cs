using System;
using System.Collections.Generic;
using System.Linq;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a printer in the network.
    /// </summary>
    public class Printer : NetworkComponent
    {
        private string _printerModel;
        private bool _isOnline;

        /// <summary>
        /// Gets the type name of the printer.
        /// </summary>
        public override string ComponentType => "Printer";

        /// <summary>
        /// Gets the path to the icon representing a printer.
        /// </summary>
        public new string IconPath => "pack://application:,,,/TNP.App;component/Assets/printer.png";

        /// <summary>
        /// Gets the maximum number of connections a printer can have.
        /// </summary>
        public override int MaxConnections => 2;

        /// <summary>
        /// Initializes a new instance of the <see cref="Printer"/> class.
        /// </summary>
        public Printer() : base()
        {
            Name = "Printer";
            _printerModel = "TNP-P1000";
            _isOnline = true;
        }

        /// <summary>
        /// Initializes the default properties for a printer.
        /// </summary>
        protected override void InitializeDefaultProperties()
        {
            Properties["IP Address"] = "192.168.1.8";
            Properties["Subnet Mask"] = "255.255.255.0";
            Properties["Default Gateway"] = "192.168.1.1";
            Properties["DNS Server"] = "8.8.8.8";
            Properties["Brand"] = "Generic";
            Properties["Model"] = "TNP-P1000";
            Properties["Type"] = "Laser";
            Properties["Color"] = true;
            Properties["DuplexPrinting"] = true;
            Properties["PaperSize"] = "A4";
            Properties["Resolution"] = "600 dpi";
            Properties["Status"] = "Ready";
            Properties["TonerLevel"] = 80; // percentage
            Properties["SharedOnNetwork"] = true;
            Properties["Location"] = "Office";
        }

        /// <summary>
        /// Gets or sets the printer model.
        /// </summary>
        public string PrinterModel
        {
            get => _printerModel;
            set
            {
                if (_printerModel != value)
                {
                    _printerModel = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether the printer is online.
        /// </summary>
        public bool IsOnline
        {
            get => _isOnline;
            set
            {
                if (_isOnline != value)
                {
                    _isOnline = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Validates the configuration of the printer.
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

            // Check printer model
            if (string.IsNullOrEmpty(PrinterModel))
            {
                errorMessage = "Printer model cannot be empty";
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
        /// Creates a deep copy of this printer.
        /// </summary>
        /// <returns>A new instance of a printer with the same properties.</returns>
        public override NetworkComponent Clone()
        {
            Printer clone = new Printer
            {
                Id = Id,
                Name = Name,
                IsSelected = IsSelected,
                PrinterModel = PrinterModel,
                IsOnline = IsOnline
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
