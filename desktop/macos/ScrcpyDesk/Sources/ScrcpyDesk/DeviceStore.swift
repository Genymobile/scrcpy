import Foundation

@MainActor
final class DeviceStore: ObservableObject {
    @Published private(set) var devices: [AndroidDevice] = []
    @Published var selectedSerial: String?
    @Published private(set) var isRefreshing = false
    @Published var notice: String?
    @Published var errorMessage: String?

    let adbPath: URL?
    let serverPath: URL?

    private var refreshTask: Task<Void, Never>?

    init() {
        adbPath = ToolLocator.find("adb")
        serverPath = ToolLocator.findScrcpyServer()
    }

    var selectedDevice: AndroidDevice? {
        devices.first { $0.serial == selectedSerial }
    }

    var environmentMessage: String? {
        guard adbPath == nil else { return nil }
        return "未找到 adb。请安装 Android Platform Tools，或通过 Homebrew 执行：brew install --cask android-platform-tools"
    }

    func start() async {
        guard refreshTask == nil else { return }
        await refresh()
        refreshTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: 3_000_000_000)
                guard !Task.isCancelled else { break }
                await self?.refresh()
            }
        }
    }

    func refresh() async {
        guard !isRefreshing, let adbPath else { return }
        isRefreshing = true
        defer { isRefreshing = false }

        do {
            let updated = try await ADBService(executable: adbPath).devices()
            devices = updated
            errorMessage = nil

            if let selectedSerial, !updated.contains(where: { $0.serial == selectedSerial }) {
                self.selectedSerial = nil
            } else if selectedSerial == nil, updated.count == 1 {
                selectedSerial = updated[0].serial
            }
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func connect(host: String, port: String) async -> Bool {
        guard let adbPath else {
            errorMessage = environmentMessage
            return false
        }

        do {
            notice = try await ADBService(executable: adbPath).connect(host: host, port: port)
            errorMessage = nil
            await refresh()
            let target = port.isEmpty ? host : "\(host):\(port)"
            if devices.contains(where: { $0.serial == target }) {
                selectedSerial = target
            }
            return true
        } catch {
            errorMessage = error.localizedDescription
            return false
        }
    }

    func pair(host: String, port: String, code: String) async -> Bool {
        guard let adbPath else {
            errorMessage = environmentMessage
            return false
        }

        do {
            notice = try await ADBService(executable: adbPath).pair(host: host, port: port, code: code)
            errorMessage = nil
            return true
        } catch {
            errorMessage = error.localizedDescription
            return false
        }
    }

    func disconnectSelected() async {
        guard let adbPath, let device = selectedDevice, device.connection == .network else { return }
        do {
            try await ADBService(executable: adbPath).disconnect(serial: device.serial)
            selectedSerial = nil
            await refresh()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func clearMessages() {
        notice = nil
        errorMessage = nil
    }

}
