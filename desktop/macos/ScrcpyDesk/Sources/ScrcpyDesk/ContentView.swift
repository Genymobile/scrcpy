import SwiftUI

struct ContentView: View {
    @StateObject private var store = DeviceStore()
    @State private var isAddingDevice = false
    @State private var renamingDevice: AndroidDevice?
    @State private var remarkText = ""
    @State private var configuringLinkDevice: AndroidDevice?
    @State private var deepLinkText = ""

    var body: some View {
        NavigationSplitView {
            sidebar
                .navigationSplitViewColumnWidth(min: 230, ideal: 270, max: 340)
        } detail: {
            detail
        }
        .toolbar {
            ToolbarItemGroup {
                Button {
                    Task { await store.refresh() }
                } label: {
                    Label("刷新设备", systemImage: "arrow.clockwise")
                }
                .disabled(store.adbPath == nil || store.isRefreshing)

                Button {
                    store.clearMessages()
                    isAddingDevice = true
                } label: {
                    Label("添加设备", systemImage: "plus")
                }
            }
        }
        .sheet(isPresented: $isAddingDevice) {
            AddDeviceView(
                store: store,
                close: { isAddingDevice = false }
            )
        }
        .sheet(item: $renamingDevice) { device in
            RenameDeviceView(
                device: device,
                remark: $remarkText,
                save: {
                    store.setRemark(remarkText, for: device)
                    renamingDevice = nil
                }
            )
        }
        .sheet(item: $configuringLinkDevice) { device in
            ConfigureDeepLinkView(
                device: device,
                url: $deepLinkText,
                save: {
                    store.setDeepLink(deepLinkText, for: device)
                    configuringLinkDevice = nil
                },
                clear: {
                    deepLinkText = ""
                    store.setDeepLink("", for: device)
                    configuringLinkDevice = nil
                },
                cancel: { configuringLinkDevice = nil }
            )
        }
        .alert("提示", isPresented: noticePresented) {
            Button("好") { store.notice = nil }
        } message: {
            Text(store.notice ?? "")
        }
        .alert("操作失败", isPresented: errorPresented) {
            Button("好") { store.errorMessage = nil }
        } message: {
            Text(store.errorMessage ?? "")
        }
        .task {
            await store.start()
        }
    }

    private var noticePresented: Binding<Bool> {
        Binding(
            get: { store.notice != nil },
            set: { if !$0 { store.notice = nil } }
        )
    }

    private var errorPresented: Binding<Bool> {
        Binding(
            get: { store.errorMessage != nil },
            set: { if !$0 { store.errorMessage = nil } }
        )
    }

    private var sidebar: some View {
        VStack(spacing: 0) {
            HStack {
                VStack(alignment: .leading, spacing: 3) {
                    Text("Android 设备")
                        .font(.headline)
                    Text(store.isPreconnecting
                         ? "正在预连接设备…"
                         : "\(store.devices.count) 台设备")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if store.isRefreshing {
                    ProgressView().controlSize(.small)
                }
            }
            .padding(16)

            Divider()

            if let environmentMessage = store.environmentMessage {
                MissingToolView(message: environmentMessage)
            } else if store.devices.isEmpty {
                EmptyDevicesView(addAction: { isAddingDevice = true })
            } else {
                List {
                    ForEach(store.devices) { device in
                        DeviceRow(
                            device: device,
                            displayName: store.displayName(for: device),
                            isDisplayed: store.isDisplayed(device),
                            toggleDisplayed: { store.toggleDisplayed(device) }
                        )
                            .contextMenu {
                                Button("修改备注名…") {
                                    remarkText = store.remark(for: device)
                                    renamingDevice = device
                                }
                                Button {
                                    deepLinkText = store.deepLink(for: device)
                                    configuringLinkDevice = device
                                } label: {
                                    Label(
                                        store.deepLink(for: device).isEmpty
                                            ? "配置跳链…"
                                            : "修改跳链…",
                                        systemImage: "link"
                                    )
                                }
                                Divider()
                                if device.connection == .network {
                                    Button("断开连接", role: .destructive) {
                                        Task { await store.disconnect(device) }
                                    }
                                }
                            }
                    }
                }
                .listStyle(.sidebar)
            }

            Divider()
            HStack(spacing: 7) {
                Circle()
                    .fill(store.adbPath == nil ? Color.red : Color.green)
                    .frame(width: 7, height: 7)
                Text(store.adbPath == nil ? "ADB 不可用" : "ADB 已就绪")
                Spacer()
            }
            .font(.caption)
            .foregroundStyle(.secondary)
            .padding(12)
        }
    }

    @ViewBuilder
    private var detail: some View {
        if !store.displayedDevices.isEmpty {
            DeviceGalleryView(store: store)
        } else {
            NoSelectionView(addAction: { isAddingDevice = true })
        }
    }
}

private struct DeviceRow: View {
    let device: AndroidDevice
    let displayName: String
    let isDisplayed: Bool
    let toggleDisplayed: () -> Void

    var body: some View {
        HStack(spacing: 11) {
            Image(systemName: device.connection == .usb ? "cable.connector" : "wifi")
                .font(.system(size: 16, weight: .medium))
                .foregroundStyle(device.isReady ? Color.accentColor : Color.secondary)
                .frame(width: 25)

            VStack(alignment: .leading, spacing: 3) {
                Text(displayName)
                    .lineLimit(1)
                HStack(spacing: 5) {
                    Circle()
                        .fill(device.isReady ? Color.green : Color.orange)
                        .frame(width: 6, height: 6)
                    Text(device.stateLabel)
                    Text("·")
                    Text(device.connection.rawValue)
                }
                .font(.caption)
                .foregroundStyle(.secondary)
            }
            Spacer(minLength: 4)
            Button(action: toggleDisplayed) {
                Label(
                    isDisplayed ? "移出" : "展示",
                    systemImage: isDisplayed ? "minus.circle.fill" : "plus.circle.fill"
                )
                .labelStyle(.titleAndIcon)
                .font(.caption.weight(.medium))
            }
            .buttonStyle(.borderless)
            .foregroundStyle(isDisplayed ? Color.orange : Color.accentColor)
            .help(isDisplayed ? "从右侧展示中移除" : "加入右侧展示")
            .disabled(!device.isReady)
        }
        .padding(.vertical, 5)
    }
}

private struct DeviceGalleryView: View {
    @ObservedObject var store: DeviceStore

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 3) {
                    Text("多设备控制台")
                        .font(.title3.weight(.semibold))
                    Text("正在同时展示 \(store.displayedDevices.count) 台设备，横向滚动查看更多")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Label("全部实时可操作", systemImage: "hand.tap.fill")
                    .font(.callout)
                    .foregroundStyle(.green)
            }
            .padding(.horizontal, 18)
            .frame(height: 68)

            Divider()

            GeometryReader { proxy in
                let cardWidth = min(440, max(330, proxy.size.height * 0.57))
                ScrollView(.horizontal) {
                    HStack(spacing: 18) {
                        ForEach(store.displayedDevices) { device in
                            DevicePreviewCard(
                                device: device,
                                store: store,
                                remove: { store.removeFromDisplay(serial: device.serial) }
                            )
                            .frame(
                                width: cardWidth,
                                height: max(300, proxy.size.height - 40)
                            )
                        }
                    }
                    .padding(20)
                    .frame(minWidth: proxy.size.width, alignment: .leading)
                }
                .scrollIndicators(.visible)
                .background(PersistentHorizontalScroller())
            }

            Divider()
            HStack {
                Label("每台设备都是独立 scrcpy 会话", systemImage: "rectangle.3.group.fill")
                Spacer()
                Text("点击任意画面后即可使用鼠标、键盘和剪贴板")
            }
            .font(.caption)
            .foregroundStyle(.secondary)
            .padding(.horizontal, 18)
            .frame(height: 42)
        }
    }
}

private struct PersistentHorizontalScroller: NSViewRepresentable {
    func makeNSView(context: Context) -> ScrollerConfiguratorView {
        ScrollerConfiguratorView()
    }

    func updateNSView(_ nsView: ScrollerConfiguratorView, context: Context) {
        nsView.configureScrollView()
    }
}

private final class ScrollerConfiguratorView: NSView {
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        configureScrollView()
        DispatchQueue.main.async { [weak self] in
            self?.configureScrollView()
        }
    }

    fileprivate func configureScrollView() {
        guard let scrollView = enclosingScrollView else { return }
        // The overlay style follows the user's system preference and fades
        // out. Legacy style reserves a stable strip and remains visible.
        scrollView.scrollerStyle = .legacy
        scrollView.autohidesScrollers = false
        scrollView.hasHorizontalScroller = true
        scrollView.horizontalScroller?.isHidden = false
        scrollView.hasVerticalScroller = false
        scrollView.tile()
    }
}

private struct DevicePreviewCard: View {
    let device: AndroidDevice
    @ObservedObject var store: DeviceStore
    let remove: () -> Void
    @State private var sessionState: EmbeddedSessionState = .idle

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 3) {
                    Text(store.displayName(for: device))
                        .font(.title3.weight(.semibold))
                    Text(device.serial)
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
                Spacer()
                let deepLink = store.deepLink(for: device)
                if store.isLaunchingLink(on: device) {
                    ProgressView()
                        .controlSize(.small)
                        .frame(width: 18, height: 18)
                        .help("正在加载页面")
                } else {
                    Button {
                        Task { await store.openConfiguredLink(on: device) }
                    } label: {
                        Image(systemName: "arrow.up.right.square.fill")
                    }
                    .buttonStyle(.borderless)
                    .foregroundStyle(deepLink.isEmpty ? Color.secondary : Color.accentColor)
                    .disabled(deepLink.isEmpty || !device.isReady)
                    .help(
                        deepLink.isEmpty
                            ? "请先在左侧右键设备配置跳链"
                            : "加载页面：\(deepLink)"
                    )
                }
                Label(sessionState.label, systemImage: sessionState == .running ? "bolt.fill" : "circle.dotted")
                    .font(.callout)
                    .foregroundStyle(sessionState == .running ? Color.green : Color.secondary)
                    .labelStyle(.iconOnly)
                    .help(sessionState.label)

                Button(action: remove) {
                    Image(systemName: "xmark.circle.fill")
                }
                .buttonStyle(.borderless)
                .foregroundStyle(.secondary)
                .help("移出展示")
            }
            .padding(14)

            Divider()

            ZStack {
                Color.black

                if !device.isReady {
                    SessionPlaceholder(
                        icon: "exclamationmark.triangle",
                        title: device.state == "unauthorized" ? "请在 Android 设备上允许 USB 调试" : "设备当前不可用"
                    )
                } else if let adbPath = store.adbPath, let serverPath = store.serverPath {
                    EmbeddedScrcpyView(
                        serial: device.serial,
                        adbPath: adbPath,
                        serverPath: serverPath,
                        state: $sessionState
                    )
                    .background(Color.black)

                    if sessionState == .starting {
                        VStack(spacing: 12) {
                            ProgressView().controlSize(.large).tint(.white)
                            Text(sessionState.label)
                        }
                        .foregroundStyle(.white.opacity(0.8))
                        .allowsHitTesting(false)
                    } else if sessionState == .failed {
                        SessionPlaceholder(icon: "xmark.octagon", title: "无法启动内嵌 scrcpy 会话")
                    }
                } else {
                    SessionPlaceholder(
                        icon: "shippingbox",
                        title: "未找到 scrcpy-server，请重新运行构建脚本"
                    )
                }
            }
        }
        .background(Color(nsColor: .controlBackgroundColor))
        .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(Color.primary.opacity(0.1), lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.16), radius: 12, y: 5)
    }

}

private struct ConfigureDeepLinkView: View {
    let device: AndroidDevice
    @Binding var url: String
    let save: () -> Void
    let clear: () -> Void
    let cancel: () -> Void
    @FocusState private var urlFocused: Bool

    private var trimmedURL: String {
        url.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 5) {
                Text("配置页面跳链")
                    .font(.title2.weight(.semibold))
                Text(device.serial)
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
                    .textSelection(.enabled)
            }

            TextField("例如：https://example.com 或 imeituan://…", text: $url)
                .textFieldStyle(.roundedBorder)
                .focused($urlFocused)
                .onSubmit {
                    if !trimmedURL.isEmpty {
                        save()
                    }
                }

            Text("保存后，点击设备预览右上角的页面加载按钮，将通过 Android VIEW Intent 打开此地址。")
                .font(.caption)
                .foregroundStyle(.secondary)

            HStack {
                Button("清除配置", action: clear)
                    .disabled(trimmedURL.isEmpty)

                Spacer()

                Button("取消", action: cancel)
                    .keyboardShortcut(.cancelAction)
                Button("保存", action: save)
                    .keyboardShortcut(.defaultAction)
                    .disabled(trimmedURL.isEmpty)
            }
        }
        .padding(24)
        .frame(width: 520)
        .onAppear { urlFocused = true }
    }
}

private struct SessionPlaceholder: View {
    let icon: String
    let title: String

    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: icon)
                .font(.system(size: 34))
            Text(title)
        }
        .foregroundStyle(.secondary)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

private struct RenameDeviceView: View {
    let device: AndroidDevice
    @Binding var remark: String
    let save: () -> Void
    @Environment(\.dismiss) private var dismiss
    @FocusState private var remarkFocused: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 5) {
                Text("修改设备备注名")
                    .font(.title2.weight(.semibold))
                Text(device.serial)
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
                    .textSelection(.enabled)
            }

            TextField("例如：前台测试机", text: $remark)
                .textFieldStyle(.roundedBorder)
                .focused($remarkFocused)
                .onSubmit(save)

            Text("留空并保存即可恢复设备原始名称：\(device.name)")
                .font(.caption)
                .foregroundStyle(.secondary)

            HStack {
                Button("清除备注") {
                    remark = ""
                    save()
                }
                .disabled(remark.isEmpty)

                Spacer()

                Button("取消") { dismiss() }
                    .keyboardShortcut(.cancelAction)
                Button("保存", action: save)
                    .keyboardShortcut(.defaultAction)
            }
        }
        .padding(24)
        .frame(width: 430)
        .onAppear { remarkFocused = true }
    }
}

private struct EmptyDevicesView: View {
    let addAction: () -> Void

    var body: some View {
        VStack(spacing: 12) {
            Spacer()
            Image(systemName: "iphone.slash")
                .font(.system(size: 32))
                .foregroundStyle(.secondary)
            Text("还没有设备")
                .font(.headline)
            Text("连接 USB 设备，或通过局域网添加")
                .font(.caption)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
            Button("添加设备", action: addAction)
            Spacer()
        }
        .padding()
    }
}

private struct MissingToolView: View {
    let message: String

    var body: some View {
        VStack(spacing: 12) {
            Spacer()
            Image(systemName: "wrench.and.screwdriver")
                .font(.system(size: 30))
                .foregroundStyle(.orange)
            Text("需要安装 ADB")
                .font(.headline)
            Text(message)
                .font(.caption)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .textSelection(.enabled)
            Spacer()
        }
        .padding(20)
    }
}

private struct NoSelectionView: View {
    let addAction: () -> Void

    var body: some View {
        VStack(spacing: 15) {
            Image(systemName: "display.and.arrow.down")
                .font(.system(size: 48, weight: .light))
                .foregroundStyle(.secondary)
            Text("选择一个 Android 设备")
                .font(.title2.weight(.semibold))
            Text("在左侧设备行点击“加入展示”，设备画面会出现在这里")
                .foregroundStyle(.secondary)
            Button("添加网络设备", action: addAction)
                .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(nsColor: .windowBackgroundColor))
    }
}
