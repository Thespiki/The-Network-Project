using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using Newtonsoft.Json;
using TNP.App.Models;

namespace TNP.App.Configs
{
    /// <summary>
    /// Service for importing network configurations from .tnp files
    /// </summary>
    public class ConfigImporter
    {
        /// <summary>
        /// Imports a network configuration from a .tnp file
        /// </summary>
        /// <param name="filePath">The file path to import from</param>
        /// <returns>The imported network configuration</returns>
        public NetworkConfiguration ImportConfiguration(string filePath)
        {
            if (string.IsNullOrEmpty(filePath))
                throw new ArgumentNullException(nameof(filePath));
            
            if (!File.Exists(filePath))
                throw new FileNotFoundException("Configuration file not found", filePath);
            
            // Read and decompress the file
            string json = ReadDecompressedTnpFile(filePath);
            
            // Deserialize the TNP config
            var tnpConfig = DeserializeTnpConfig(json);
            
            // Extract and validate the network configuration
            var configuration = tnpConfig.Configuration;
            
            if (configuration == null)
                throw new InvalidOperationException("The imported file does not contain a valid network configuration");
            
            if (!configuration.ValidateConfiguration(out string errorMessage))
                throw new InvalidOperationException($"Invalid configuration in imported file: {errorMessage}");
            
            return configuration;
        }
        
        /// <summary>
        /// Reads and decompresses a .tnp file
        /// </summary>
        /// <param name="filePath">The file path to read from</param>
        /// <returns>The decompressed JSON string</returns>
        private string ReadDecompressedTnpFile(string filePath)
        {
            try
            {
                // Read the file
                using (FileStream fileStream = new FileStream(filePath, FileMode.Open))
                {
                    // Check the signature
                    byte[] signature = new byte[3];
                    if (fileStream.Read(signature, 0, signature.Length) != signature.Length ||
                        Encoding.ASCII.GetString(signature) != "TNP")
                    {
                        throw new InvalidOperationException("The file is not a valid TNP configuration file");
                    }
                    
                    // Decompress the data
                    using (GZipStream decompressionStream = new GZipStream(fileStream, CompressionMode.Decompress))
                    {
                        using (MemoryStream memoryStream = new MemoryStream())
                        {
                            decompressionStream.CopyTo(memoryStream);
                            byte[] decompressedBytes = memoryStream.ToArray();
                            return Encoding.UTF8.GetString(decompressedBytes);
                        }
                    }
                }
            }
            catch (InvalidDataException)
            {
                // This might happen if the file is not compressed with GZip
                // Try to read it as a plain JSON file instead
                return File.ReadAllText(filePath);
            }
        }
        
        /// <summary>
        /// Deserializes a TNP configuration from JSON
        /// </summary>
        /// <param name="json">The JSON string to deserialize</param>
        /// <returns>The deserialized TNP configuration</returns>
        private TnpConfiguration DeserializeTnpConfig(string json)
        {
            var settings = new JsonSerializerSettings
            {
                TypeNameHandling = TypeNameHandling.Auto
            };
            
            var tnpConfig = JsonConvert.DeserializeObject<TnpConfiguration>(json, settings);
            
            if (tnpConfig == null)
            {
                // The JSON might be a direct NetworkConfiguration, not wrapped in a TnpConfiguration
                var configuration = JsonConvert.DeserializeObject<NetworkConfiguration>(json, settings);
                
                if (configuration != null)
                {
                    tnpConfig = new TnpConfiguration
                    {
                        FormatVersion = "1.0",
                        CreatedWith = "Unknown",
                        CreatedDate = DateTime.Now,
                        Configuration = configuration
                    };
                }
                else
                {
                    throw new InvalidOperationException("Failed to deserialize the configuration");
                }
            }
            
            return tnpConfig;
        }
    }
}
