using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a connection between two network elements.
    /// </summary>
    public class Connection : INotifyPropertyChanged
    {
        private NetworkElement? _source;
        private NetworkElement? _target;
        private ConnectionType _connectionType = ConnectionType.Ethernet;
        private string _label = string.Empty;
        private bool _isActive = true;
        private bool _isSelected;
        private double _bandwidth = 1000.0; // Mb/s
        private double _latency = 1.0; // ms
        private double _packetLoss = 0.0; // %

        public Connection(NetworkElement source, NetworkElement target, ConnectionType connectionType)
        {
            _source = source;
            _target = target;
            _connectionType = connectionType;
            _label = $"{source?.Name ?? "Unknown"} - {target?.Name ?? "Unknown"}";
        }

        public Guid Id { get; set; } = Guid.NewGuid();

        public NetworkElement? Source
        {
            get => _source;
            set
            {
                if (_source != value)
                {
                    _source = value;
                    OnPropertyChanged();
                }
            }
        }

        public NetworkElement? Target
        {
            get => _target;
            set
            {
                if (_target != value)
                {
                    _target = value;
                    OnPropertyChanged();
                }
            }
        }

        public ConnectionType ConnectionType
        {
            get => _connectionType;
            set
            {
                if (_connectionType != value)
                {
                    _connectionType = value;
                    OnPropertyChanged();
                }
            }
        }

        public string Label
        {
            get => _label;
            set
            {
                if (_label != value)
                {
                    _label = value;
                    OnPropertyChanged();
                }
            }
        }

        public bool IsActive
        {
            get => _isActive;
            set
            {
                if (_isActive != value)
                {
                    _isActive = value;
                    OnPropertyChanged();
                }
            }
        }

        public bool IsSelected
        {
            get => _isSelected;
            set
            {
                if (_isSelected != value)
                {
                    _isSelected = value;
                    OnPropertyChanged();
                }
            }
        }

        public double Bandwidth
        {
            get => _bandwidth;
            set
            {
                if (_bandwidth != value)
                {
                    _bandwidth = value;
                    OnPropertyChanged();
                }
            }
        }

        public double Latency
        {
            get => _latency;
            set
            {
                if (_latency != value)
                {
                    _latency = value;
                    OnPropertyChanged();
                }
            }
        }

        public double PacketLoss
        {
            get => _packetLoss;
            set
            {
                if (_packetLoss != value)
                {
                    _packetLoss = value;
                    OnPropertyChanged();
                }
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        public override string ToString()
        {
            var sourceName = Source?.Name ?? "Unknown";
            var targetName = Target?.Name ?? "Unknown";
            return $"{sourceName} <-> {targetName} ({ConnectionType})";
        }
    }
}