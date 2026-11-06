using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Windows.Forms;

namespace ScrcpyMappingManager;

public partial class MainForm : Form
{
    private readonly string _baseDir;
    private readonly string _templatesDir;
    private readonly string _configPath;

    private ListBox _templateList = null!;
    private DataGridView _grid = null!;
    private TextBox _txtSwitch = null!;
    private TextBox _txtDesign = null!;
    private TextBox _txtToggle = null!;
    private Label _lblActive = null!;

    private List<MappingEntry> _currentMappings = new();
    private string? _currentTemplate;

    public MainForm()
    {
        _baseDir = AppContext.BaseDirectory;
        _templatesDir = Path.Combine(_baseDir, "templates");
        _configPath = Path.Combine(_baseDir, "scrcpy_mapping_config.json");
        InitializeComponent();
        LoadTemplates();
        LoadConfig();
    }

    private void InitializeComponent()
    {
        this.Text = "SCRCPY 映射模板管理器";
        this.Width = 900;
        this.Height = 600;
        this.StartPosition = FormStartPosition.CenterScreen;

        var split = new SplitContainer
        {
            Dock = DockStyle.Fill,
            Orientation = Orientation.Vertical,
            SplitterDistance = 220
        };

        // ---- Left panel: template list ----
        var left = new Panel { Dock = DockStyle.Fill };
        var lblTemplates = new Label
        {
            Text = "模板列表",
            Dock = DockStyle.Top,
            Height = 24,
            TextAlign = System.Drawing.ContentAlignment.MiddleLeft
        };
        _templateList = new ListBox { Dock = DockStyle.Fill };
        _templateList.SelectedIndexChanged += (s, e) => LoadSelectedTemplate();

        var btnPanel = new FlowLayoutPanel
        {
            Dock = DockStyle.Bottom,
            Height = 40,
            FlowDirection = FlowDirection.LeftToRight
        };
        var btnNew = new Button { Text = "新建", Width = 60 };
        btnNew.Click += (s, e) => NewTemplate();
        var btnDelete = new Button { Text = "删除", Width = 60 };
        btnDelete.Click += (s, e) => DeleteTemplate();
        var btnSetActive = new Button { Text = "设为活动", Width = 80 };
        btnSetActive.Click += (s, e) => SetActiveTemplate();

        btnPanel.Controls.Add(btnNew);
        btnPanel.Controls.Add(btnDelete);
        btnPanel.Controls.Add(btnSetActive);

        _lblActive = new Label
        {
            Text = "活动: -",
            Dock = DockStyle.Bottom,
            Height = 24,
            ForeColor = System.Drawing.Color.Blue
        };

        left.Controls.Add(_templateList);
        left.Controls.Add(btnPanel);
        left.Controls.Add(_lblActive);
        left.Controls.Add(lblTemplates);
        split.Panel1.Controls.Add(left);

        // ---- Right panel: mapping editor ----
        var right = new Panel { Dock = DockStyle.Fill };

        var lblMappings = new Label
        {
            Text = "映射内容 (坐标为归一化值 0.0~1.0)",
            Dock = DockStyle.Top,
            Height = 24,
            TextAlign = System.Drawing.ContentAlignment.MiddleLeft
        };

        _grid = new DataGridView
        {
            Dock = DockStyle.Fill,
            AutoGenerateColumns = false,
            AllowUserToAddRows = true,
            AllowUserToDeleteRows = true
        };
        _grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            DataPropertyName = "Key",
            HeaderText = "按键 (如 A, W+D, MOUSE_RIGHT)",
            Width = 220
        });
        _grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            DataPropertyName = "X",
            HeaderText = "X (0~1)",
            Width = 100
        });
        _grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            DataPropertyName = "Y",
            HeaderText = "Y (0~1)",
            Width = 100
        });

        var rightBtnPanel = new FlowLayoutPanel
        {
            Dock = DockStyle.Bottom,
            Height = 40,
            FlowDirection = FlowDirection.LeftToRight
        };
        var btnAddRow = new Button { Text = "添加行", Width = 80 };
        btnAddRow.Click += (s, e) => AddMappingRow();
        var btnSave = new Button { Text = "保存模板", Width = 100 };
        btnSave.Click += (s, e) => SaveTemplate();

        rightBtnPanel.Controls.Add(btnAddRow);
        rightBtnPanel.Controls.Add(btnSave);

        right.Controls.Add(_grid);
        right.Controls.Add(rightBtnPanel);
        right.Controls.Add(lblMappings);
        split.Panel2.Controls.Add(right);

        // ---- Top: shortcuts config ----
        var topPanel = new Panel
        {
            Dock = DockStyle.Top,
            Height = 70
        };
        var lblShortcuts = new Label
        {
            Text = "快捷键配置 (按键名，如 F2, Ctrl+K):",
            Left = 8, Top = 6, Width = 280
        };
        _txtSwitch = new TextBox { Left = 8, Top = 30, Width = 120 };
        _txtDesign = new TextBox { Left = 140, Top = 30, Width = 120 };
        _txtToggle = new TextBox { Left = 272, Top = 30, Width = 120 };
        var lblSw = new Label { Text = "切换模板", Left = 8, Top = 52, Width = 120, TextAlign = System.Drawing.ContentAlignment.MiddleCenter };
        var lblDs = new Label { Text = "设计模式", Left = 140, Top = 52, Width = 120, TextAlign = System.Drawing.ContentAlignment.MiddleCenter };
        var lblTg = new Label { Text = "开关映射", Left = 272, Top = 52, Width = 120, TextAlign = System.Drawing.ContentAlignment.MiddleCenter };
        var btnSaveShortcuts = new Button { Text = "保存快捷键", Left = 410, Top = 29, Width = 110 };
        btnSaveShortcuts.Click += (s, e) => SaveShortcuts();

        topPanel.Controls.Add(lblShortcuts);
        topPanel.Controls.Add(_txtSwitch);
        topPanel.Controls.Add(_txtDesign);
        topPanel.Controls.Add(_txtToggle);
        topPanel.Controls.Add(lblSw);
        topPanel.Controls.Add(lblDs);
        topPanel.Controls.Add(lblTg);
        topPanel.Controls.Add(btnSaveShortcuts);

        this.Controls.Add(split);
        this.Controls.Add(topPanel);

        // Status strip
        var status = new StatusStrip();
        status.Items.Add(new ToolStripStatusLabel("就绪"));
        this.Controls.Add(status);
    }

    // ============================================================
    // Template operations
    // ============================================================

    private void LoadTemplates()
    {
        _templateList.Items.Clear();
        if (!Directory.Exists(_templatesDir))
        {
            Directory.CreateDirectory(_templatesDir);
        }
        foreach (var f in Directory.GetFiles(_templatesDir, "*.json"))
        {
            _templateList.Items.Add(Path.GetFileNameWithoutExtension(f));
        }
    }

    private void LoadSelectedTemplate()
    {
        if (_templateList.SelectedItem is not string name) return;
        _currentTemplate = name;
        var path = Path.Combine(_templatesDir, name + ".json");
        try
        {
            var json = File.ReadAllText(path);
            var tpl = JsonSerializer.Deserialize<TemplateFile>(json) ?? new TemplateFile { Name = name };
            _currentMappings = tpl.Mappings ?? new List<MappingEntry>();
            _grid.DataSource = null;
            _grid.DataSource = _currentMappings;
        }
        catch (Exception ex)
        {
            MessageBox.Show("加载模板失败: " + ex.Message);
        }
    }

    private void NewTemplate()
    {
        using var dlg = new Form
        {
            Text = "新建模板",
            Width = 320,
            Height = 140,
            StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false,
            MinimizeBox = false
        };
        var txt = new TextBox { Left = 20, Top = 20, Width = 260 };
        var btnOk = new Button { Text = "确定", Left = 120, Top = 60, Width = 80, DialogResult = DialogResult.OK };
        var btnCancel = new Button { Text = "取消", Left = 210, Top = 60, Width = 70, DialogResult = DialogResult.Cancel };
        dlg.Controls.Add(txt);
        dlg.Controls.Add(btnOk);
        dlg.Controls.Add(btnCancel);
        dlg.AcceptButton = btnOk;
        dlg.CancelButton = btnCancel;

        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        var name = txt.Text.Trim();
        if (string.IsNullOrEmpty(name)) return;

        var path = Path.Combine(_templatesDir, name + ".json");
        if (File.Exists(path))
        {
            MessageBox.Show("模板已存在");
            return;
        }
        var tpl = new TemplateFile { Name = name, Mappings = new List<MappingEntry>() };
        File.WriteAllText(path, JsonSerializer.Serialize(tpl, new JsonSerializerOptions { WriteIndented = true }));
        LoadTemplates();
        _templateList.SelectedItem = name;
    }

    private void DeleteTemplate()
    {
        if (_templateList.SelectedItem is not string name) return;
        if (MessageBox.Show($"确定删除模板 '{name}'?", "确认", MessageBoxButtons.YesNo) != DialogResult.Yes)
            return;
        var path = Path.Combine(_templatesDir, name + ".json");
        File.Delete(path);
        LoadTemplates();
        _grid.DataSource = null;
    }

    private void SetActiveTemplate()
    {
        if (_templateList.SelectedItem is not string name) return;
        try
        {
            var cfg = LoadConfigFile();
            cfg.ActiveTemplate = name;
            SaveConfigFile(cfg);
            UpdateActiveLabel(cfg);
            MessageBox.Show($"已将 '{name}' 设为活动模板，SCRCPY 将自动热加载");
        }
        catch (Exception ex)
        {
            MessageBox.Show("设置失败: " + ex.Message);
        }
    }

    private void AddMappingRow()
    {
        _currentMappings.Add(new MappingEntry { Key = "", X = 0.5, Y = 0.5 });
        _grid.DataSource = null;
        _grid.DataSource = _currentMappings;
    }

    private void SaveTemplate()
    {
        if (string.IsNullOrEmpty(_currentTemplate))
        {
            MessageBox.Show("请先选择一个模板");
            return;
        }
        // Commit any pending cell edit
        _grid.EndEdit();

        // Validate and clamp coordinates
        foreach (var m in _currentMappings)
        {
            if (m.X < 0) m.X = 0;
            if (m.X > 1) m.X = 1;
            if (m.Y < 0) m.Y = 0;
            if (m.Y > 1) m.Y = 1;
        }

        var tpl = new TemplateFile { Name = _currentTemplate, Mappings = _currentMappings };
        var path = Path.Combine(_templatesDir, _currentTemplate + ".json");
        File.WriteAllText(path, JsonSerializer.Serialize(tpl, new JsonSerializerOptions { WriteIndented = true }));
        MessageBox.Show("模板已保存。若该模板为活动模板，SCRCPY 将自动热加载。");
    }

    // ============================================================
    // Config / shortcuts
    // ============================================================

    private AppConfig LoadConfigFile()
    {
        if (!File.Exists(_configPath))
        {
            var cfg = new AppConfig();
            SaveConfigFile(cfg);
            return cfg;
        }
        var json = File.ReadAllText(_configPath);
        return JsonSerializer.Deserialize<AppConfig>(json) ?? new AppConfig();
    }

    private void SaveConfigFile(AppConfig cfg)
    {
        File.WriteAllText(_configPath,
            JsonSerializer.Serialize(cfg, new JsonSerializerOptions { WriteIndented = true }));
    }

    private void LoadConfig()
    {
        try
        {
            var cfg = LoadConfigFile();
            _txtSwitch.Text = cfg.Shortcuts.SwitchTemplate;
            _txtDesign.Text = cfg.Shortcuts.DesignMode;
            _txtToggle.Text = cfg.Shortcuts.ToggleMapping;
            UpdateActiveLabel(cfg);
        }
        catch
        {
            // ignore
        }
    }

    private void UpdateActiveLabel(AppConfig cfg)
    {
        _lblActive.Text = $"活动: {cfg.ActiveTemplate}  映射: {(cfg.Enabled ? "开" : "关")}";
    }

    private void SaveShortcuts()
    {
        try
        {
            var cfg = LoadConfigFile();
            cfg.Shortcuts.SwitchTemplate = _txtSwitch.Text.Trim();
            cfg.Shortcuts.DesignMode = _txtDesign.Text.Trim();
            cfg.Shortcuts.ToggleMapping = _txtToggle.Text.Trim();
            SaveConfigFile(cfg);
            MessageBox.Show("快捷键已保存，SCRCPY 将自动重新加载配置");
        }
        catch (Exception ex)
        {
            MessageBox.Show("保存失败: " + ex.Message);
        }
    }
}
