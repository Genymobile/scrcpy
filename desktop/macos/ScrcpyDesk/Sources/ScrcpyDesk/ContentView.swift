import SwiftUI

struct ContentView: View {
    @StateObject private var store = DeviceStore()
    @State private var isAddingDevice = false

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
            AddDeviceView(store: store)
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
                    Text("\(store.devices.count) 台设备")
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
                List(selection: $store.selectedSerial) {
                    ForEach(store.devices) { device in
                        DeviceRow(device: device)
                            .tag(device.serial)
                            .contextMenu {
                                if device.connection == .network {
                                    Button("断开连接", role: .destructive) {
                                        store.selectedSerial = device.serial
                                        Task { await store.disconnectSelected() }
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
        if let device = store.selectedDevice {
            DevicePreviewView(device: device, store: store)
        } else {
            NoSelectionView(addAction: { isAddingDevice = true })
        }
    }
}

private struct DeviceRow: View {
    let device: AndroidDevice

    var body: some View {
        HStack(spacing: 11) {
            Image(systemName: device.connection == .usb ? "cable.connector" : "wifi")
                .font(.system(size: 16, weight: .medium))
                .foregroundStyle(device.isReady ? Color.accentColor : Color.secondary)
                .frame(width: 25)

            VStack(alignment: .leading, spacing: 3) {
                Text(device.name)
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
        }
        .padding(.vertical, 5)
    }
}

private struct DevicePreviewView: View {
    let device: AndroidDevice
    @ObservedObject var store: DeviceStore
    @State private var sessionState: EmbeddedSessionState = .idle

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 3) {
                    Text(device.name)
                        .font(.title3.weight(.semibold))
                    Text(device.serial)
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
                Spacer()

                if device.connection == .network {
                    Button("断开") {
                        Task { await store.disconnectSelected() }
                    }
                }

                Label(sessionState.label, systemImage: sessionState == .running ? "bolt.fill" : "circle.dotted")
                    .font(.callout)
                    .foregroundStyle(sessionState == .running ? Color.green : Color.secondary)
            }
            .padding(18)

            Divider()

            ZStack {
                Color(nsColor: .windowBackgroundColor)

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
                    .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
                    .shadow(color: .black.opacity(0.22), radius: 18, y: 7)
                    .padding(26)

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

            Divider()
            HStack {
                Label("scrcpy 实时视频与控制已嵌入", systemImage: "rectangle.and.hand.point.up.left.filled")
                Spacer()
                Text("点击画面后可使用鼠标、键盘和剪贴板")
            }
            .font(.caption)
            .foregroundStyle(.secondary)
            .padding(.horizontal, 18)
            .frame(height: 42)
        }
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
            Text("设备画面会直接显示在这里")
                .foregroundStyle(.secondary)
            Button("添加网络设备", action: addAction)
                .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(nsColor: .windowBackgroundColor))
    }
}
