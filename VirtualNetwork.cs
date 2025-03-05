using System.ComponentModel;

namespace TheNetworkProject
{
    public class VirtualNetwork : INotifyPropertyChanged
    {
        private string name = "";
        public string Name
        {
            get => name;
            set
            {
                if (name != value)
                {
                    name = value;
                    OnPropertyChanged("Name");
                }
            }
        }

        private bool optionsVisible;
        public bool OptionsVisible
        {
            get => optionsVisible;
            set
            {
                if (optionsVisible != value)
                {
                    optionsVisible = value;
                    OnPropertyChanged("OptionsVisible");
                }
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}