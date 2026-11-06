using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace ScrcpyMappingManager;

// Represents a single key mapping entry
public class MappingEntry
{
    [JsonPropertyName("key")]
    public string Key { get; set; } = "";

    [JsonPropertyName("x")]
    public double X { get; set; }

    [JsonPropertyName("y")]
    public double Y { get; set; }
}

// Represents a template file (templates/<name>.json)
public class TemplateFile
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = "";

    [JsonPropertyName("mappings")]
    public List<MappingEntry> Mappings { get; set; } = new();
}

// Represents scrcpy_mapping_config.json
public class AppConfig
{
    [JsonPropertyName("shortcuts")]
    public Shortcuts Shortcuts { get; set; } = new();

    [JsonPropertyName("active_template")]
    public string ActiveTemplate { get; set; } = "default";

    [JsonPropertyName("enabled")]
    public bool Enabled { get; set; } = true;
}

public class Shortcuts
{
    [JsonPropertyName("switch_template")]
    public string SwitchTemplate { get; set; } = "F2";

    [JsonPropertyName("design_mode")]
    public string DesignMode { get; set; } = "F3";

    [JsonPropertyName("toggle_mapping")]
    public string ToggleMapping { get; set; } = "F4";
}
