import Foundation

@MainActor
final class DeviceStore: ObservableObject {
    @Published private(set) var devices: [AndroidDevice] = []
    @Published private(set) var displayedSerials: [String] = []
    @Published private(set) var deviceRemarks: [String: String] = [:]
    @Published private(set) var deviceDeepLinks: [String: String] = [:]
    @Published private(set) var launchingLinkSerials: Set<String> = []
    @Published private(set) var isRefreshing = false
    @Published private(set) var isPreconnecting = false
    @Published var notice: String?
    @Published var errorMessage: String?

    let adbPath: URL?
    let serverPath: URL?

    private var refreshTask: Task<Void, Never>?
    private var hasStarted = false
    private let displayedSerialsKey = "displayedDeviceSerials"
    private let deviceRemarksKey = "deviceRemarks"
    private let deviceDeepLinksKey = "deviceDeepLinks"

    private static let preconnectTargets = [
        "33.231.115.146:8080",
        "33.230.92.157:8080",
        "33.230.95.176:8080",
        "33.229.81.169:8080",
        "33.230.86.150:8080",
        "33.229.87.112:8080",
        "33.229.94.19:8080",
        "33.229.83.21:8080",
        "33.230.88.25:8080",
        "33.229.83.200:8080",
        "33.229.94.76:8080",
        "33.229.84.161:8080",
        "33.229.94.89:8080",
    ]

    init() {
        adbPath = ToolLocator.find("adb")
        serverPath = ToolLocator.findScrcpyServer()
        displayedSerials = UserDefaults.standard.stringArray(
            forKey: displayedSerialsKey
        ) ?? []
        deviceRemarks = UserDefaults.standard.dictionary(
            forKey: deviceRemarksKey
        ) as? [String: String] ?? [:]
        deviceDeepLinks = UserDefaults.standard.dictionary(
            forKey: deviceDeepLinksKey
        ) as? [String: String] ?? [:]
    }

    var displayedDevices: [AndroidDevice] {
        displayedSerials.compactMap { serial in
            devices.first { $0.serial == serial }
        }
    }

    func isDisplayed(_ device: AndroidDevice) -> Bool {
        displayedSerials.contains(device.serial)
    }

    func toggleDisplayed(_ device: AndroidDevice) {
        if let index = displayedSerials.firstIndex(of: device.serial) {
            displayedSerials.remove(at: index)
        } else {
            displayedSerials.append(device.serial)
        }
        UserDefaults.standard.set(displayedSerials, forKey: displayedSerialsKey)
    }

    func removeFromDisplay(serial: String) {
        displayedSerials.removeAll { $0 == serial }
        UserDefaults.standard.set(displayedSerials, forKey: displayedSerialsKey)
    }

    func displayName(for device: AndroidDevice) -> String {
        deviceRemarks[device.serial] ?? device.name
    }

    func remark(for device: AndroidDevice) -> String {
        deviceRemarks[device.serial] ?? ""
    }

    func setRemark(_ remark: String, for device: AndroidDevice) {
        let value = remark.trimmingCharacters(in: .whitespacesAndNewlines)
        if value.isEmpty {
            deviceRemarks.removeValue(forKey: device.serial)
        } else {
            deviceRemarks[device.serial] = value
        }
        UserDefaults.standard.set(deviceRemarks, forKey: deviceRemarksKey)
    }

    func deepLink(for device: AndroidDevice) -> String {
        deviceDeepLinks[device.serial] ?? ""
    }

    func setDeepLink(_ url: String, for device: AndroidDevice) {
        let value = url.trimmingCharacters(in: .whitespacesAndNewlines)
        if value.isEmpty {
            deviceDeepLinks.removeValue(forKey: device.serial)
        } else {
            deviceDeepLinks[device.serial] = value
        }
        UserDefaults.standard.set(deviceDeepLinks, forKey: deviceDeepLinksKey)
    }

    func isLaunchingLink(on device: AndroidDevice) -> Bool {
        launchingLinkSerials.contains(device.serial)
    }

    func openConfiguredLink(on device: AndroidDevice) async {
        guard let adbPath else {
            errorMessage = environmentMessage
            return
        }
        let url = deepLink(for: device)
        guard !url.isEmpty, !launchingLinkSerials.contains(device.serial) else {
            return
        }

        launchingLinkSerials.insert(device.serial)
        defer { launchingLinkSerials.remove(device.serial) }
        do {
            try await ADBService(executable: adbPath).openURL(
                serial: device.serial,
                url: url
            )
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    var environmentMessage: String? {
        guard adbPath == nil else { return nil }
        return "未找到 adb。请安装 Android Platform Tools，或通过 Homebrew 执行：brew install --cask android-platform-tools"
    }

    func start() async {
        guard !hasStarted else { return }
        hasStarted = true
        await preconnectConfiguredDevices()
        await refresh()
        refreshTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: 3_000_000_000)
                guard !Task.isCancelled else { break }
                await self?.refresh()
            }
        }
    }

    private func preconnectConfiguredDevices() async {
        guard let adbPath else { return }
        isPreconnecting = true
        defer { isPreconnecting = false }

        let service = ADBService(executable: adbPath)
        // Start the ADB daemon once before issuing concurrent connect calls.
        _ = try? await service.devices()
        await withTaskGroup(of: Void.self) { group in
            for target in Self.preconnectTargets {
                group.addTask {
                    _ = try? await service.connect(host: target, port: "")
                }
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

    func disconnect(_ device: AndroidDevice) async {
        guard let adbPath, device.connection == .network else { return }
        do {
            try await ADBService(executable: adbPath).disconnect(serial: device.serial)
            removeFromDisplay(serial: device.serial)
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
