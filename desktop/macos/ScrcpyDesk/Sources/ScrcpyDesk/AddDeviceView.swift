import SwiftUI

struct AddDeviceView: View {
    @ObservedObject var store: DeviceStore
    let close: () -> Void

    @State private var host = ""
    @State private var connectPort = "5555"
    @State private var showPairing = false
    @State private var pairingPort = ""
    @State private var pairingCode = ""
    @State private var isWorking = false

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text("添加 Android 设备")
                        .font(.title2.weight(.semibold))
                    Text("设备与 Mac 需要位于同一网络")
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Button {
                    close()
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .font(.title2)
                        .foregroundStyle(.secondary)
                }
                .buttonStyle(.plain)
            }
            .padding(22)

            Divider()

            Form {
                Section("设备地址") {
                    TextField("IP 地址，例如 192.168.1.20", text: $host)
                        .textFieldStyle(.roundedBorder)
                    TextField("连接端口", text: $connectPort)
                        .textFieldStyle(.roundedBorder)
                }

                Section {
                    Toggle("Android 11+ 首次无线配对", isOn: $showPairing.animation())
                    if showPairing {
                        TextField("配对端口（手机配对弹窗中显示）", text: $pairingPort)
                            .textFieldStyle(.roundedBorder)
                        SecureField("六位配对码", text: $pairingCode)
                            .textFieldStyle(.roundedBorder)
                        HStack {
                            Text("先完成配对，再使用上方连接端口连接。配对端口与连接端口通常不同。")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            Spacer()
                            Button("配对") {
                                pair()
                            }
                            .disabled(!canPair || isWorking)
                        }
                    }
                }
            }
            .formStyle(.grouped)
            .padding(.horizontal, 8)

            Divider()

            HStack {
                Text("使用 adb pair / adb connect，不会保存配对码")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Spacer()
                Button("取消", action: close)
                    .keyboardShortcut(.cancelAction)
                Button("连接") { connect() }
                    .buttonStyle(.borderedProminent)
                    .keyboardShortcut(.defaultAction)
                    .disabled(!canConnect || isWorking)
            }
            .padding(18)
        }
        .frame(width: 520, height: showPairing ? 520 : 390)
    }

    private var canConnect: Bool {
        !host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        Int(connectPort) != nil
    }

    private var canPair: Bool {
        !host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        Int(pairingPort) != nil &&
        pairingCode.count >= 6
    }

    private func pair() {
        isWorking = true
        Task {
            _ = await store.pair(host: host, port: pairingPort, code: pairingCode)
            isWorking = false
        }
    }

    private func connect() {
        isWorking = true
        Task {
            let connected = await store.connect(host: host, port: connectPort)
            isWorking = false
            if connected { close() }
        }
    }
}
