using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using Newtonsoft.Json;
using TNP.App.Models;

namespace TNP.App.Configs
{
    /// <summary>
    /// Service for exporting network configurations to .tnp files
    /// </summary>
    public class ConfigExporter
    {
        /// <summary>
        /// Exports a network configuration to a .tnp file
        /// </summary>
        /// <param name="configuration">The configuration to export</param>
        /// <param name="filePath">The file path to export to</param>
        public void ExportConfiguration(NetworkConfiguration configuration, string filePath)
        {
            if (configuration == null)
                throw new ArgumentNullException(nameof(configuration));
            
            if (string.IsNullOrEmpty(filePath))
                throw new ArgumentNullException(nameof(filePath));
                
            // Validate the configuration before exporting
            if (!configuration.ValidateConfiguration(out string errorMessage))
                throw new InvalidOperationException($"Invalid configuration: {errorMessage}");
            
            // Create a TNP config object
            var tnpConfig = CreateTnpConfig(configuration);
            
            // Serialize the TNP config to JSON
            string json = SerializeTnpConfig(tnpConfig);
            
            // Compress and save the file
            SaveCompressedTnpFile(json, filePath);
        }
        
        /// <summary>
        /// Creates a TNP configuration object from a network configuration
        /// </summary>
        /// <param name="configuration">The source configuration</param>
        /// <returns>A TNP configuration object</returns>
        private TnpConfiguration CreateTnpConfig(NetworkConfiguration configuration)
        {
            // Create a TNP configuration
            var tnpConfig = new TnpConfiguration
            {
                FormatVersion = "1.0",
                CreatedWith = "The Network Project",
                CreatedDate = DateTime.Now,
                Configuration = configuration
            };
            
            return tnpConfig;
        }
        
        /// <summary>
        /// Serializes a TNP configuration to JSON
        /// </summary>
        /// <param name="tnpConfig">The TNP configuration to serialize</param>
        /// <returns>The JSON string</returns>
        private string SerializeTnpConfig(TnpConfiguration tnpConfig)
        {
            var settings = new JsonSerializerSettings
            {
                TypeNameHandling = TypeNameHandling.Auto,
                Formatting = Formatting.Indented
            };
            
            return JsonConvert.SerializeObject(tnpConfig, settings);
        }
        
        /// <summary>
        /// Saves a compressed .tnp file
        /// </summary>
        /// <param name="json">The JSON string to compress</param>
        /// <param name="filePath">The file path to save to</param>
        private void SaveCompressedTnpFile(string json, string filePath)
        {
            // Convert the JSON to bytes
            byte[] jsonBytes = Encoding.UTF8.GetBytes(json);
            
            // Create the TNP file
            using (FileStream fileStream = new FileStream(filePath, FileMode.Create))
            {
                // Add a signature to identify TNP files
                byte[] signature = Encoding.ASCII.GetBytes("TNP");
                fileStream.Write(signature, 0, signature.Length);
                
                // Compress the JSON data
                using (GZipStream compressionStream = new GZipStream(fileStream, CompressionMode.Compress, leaveOpen: true))
                {
                    compressionStream.Write(jsonBytes, 0, jsonBytes.Length);
                }
            }
        }
    }
    
    /// <summary>
    /// Represents a TNP configuration file
    /// </summary>
    internal class TnpConfiguration
    {
        /// <summary>
        /// Gets or sets the format version
        /// </summary>
        [JsonProperty("formatVersion")]
        public string FormatVersion { get; set; }
        
        /// <summary>
        /// Gets or sets the application that created the file
        /// </summary>
        [JsonProperty("createdWith")]
        public string CreatedWith { get; set; }
        
        /// <summary>
        /// Gets or sets the creation date
        /// </summary>
        [JsonProperty("createdDate")]
        public DateTime CreatedDate { get; set; }
        
        /// <summary>
        /// Gets or sets the network configuration
        /// </summary>
        [JsonProperty("configuration")]
        public NetworkConfiguration Configuration { get; set; }
    }
}
