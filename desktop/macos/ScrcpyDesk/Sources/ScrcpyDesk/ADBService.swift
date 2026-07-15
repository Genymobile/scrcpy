import Foundation

struct ADBService {
    let executable: URL

    func devices() async throws -> [AndroidDevice] {
        let output = try await checkedRun(["devices", "-l"])

        return output.text
            .split(whereSeparator: \Character.isNewline)
            .dropFirst()
            .compactMap(parseDevice)
    }

    func connect(host: String, port: String) async throws -> String {
        let target = address(host: host, port: port)
        let output = try await checkedRun(["connect", target])
        let message = output.text.trimmingCharacters(in: .whitespacesAndNewlines)

        if message.localizedCaseInsensitiveContains("failed") ||
            message.localizedCaseInsensitiveContains("unable") ||
            message.localizedCaseInsensitiveContains("cannot") {
            throw CommandError.failed(command: "adb connect", code: output.exitCode, message: message)
        }
        return message.isEmpty ? "已连接到 \(target)" : message
    }

    func pair(host: String, port: String, code: String) async throws -> String {
        let target = address(host: host, port: port)
        let output = try await checkedRun(["pair", target, code])
        let message = output.text.trimmingCharacters(in: .whitespacesAndNewlines)

        if !message.localizedCaseInsensitiveContains("success") {
            throw CommandError.failed(command: "adb pair", code: output.exitCode, message: message)
        }
        return message
    }

    func disconnect(serial: String) async throws {
        _ = try await checkedRun(["disconnect", serial])
    }

    func openURL(serial: String, url: String) async throws {
        let output = try await checkedRun([
            "-s", serial,
            "shell", "am", "start",
            "-a", "android.intent.action.VIEW",
            "-d", url,
        ])
        let message = [output.text, output.errorText]
            .filter { !$0.isEmpty }
            .joined(separator: "\n")
        let lowercased = message.lowercased()
        if lowercased.contains("error:") ||
            lowercased.contains("error type") ||
            lowercased.contains("exception") {
            throw CommandError.failed(
                command: "adb -s \(serial) shell am start",
                code: output.exitCode,
                message: message.trimmingCharacters(in: .whitespacesAndNewlines)
            )
        }
    }

    private func checkedRun(_ arguments: [String]) async throws -> CommandOutput {
        let output = try await CommandRunner.run(
            executable: executable,
            arguments: arguments,
            environment: ["ADB_MDNS_AUTO_CONNECT": "0"]
        )
        guard output.exitCode == 0 else {
            let message = output.errorText.isEmpty ? output.text : output.errorText
            throw CommandError.failed(
                command: "adb \(arguments.joined(separator: " "))",
                code: output.exitCode,
                message: message.trimmingCharacters(in: .whitespacesAndNewlines)
            )
        }
        return output
    }

    private func address(host: String, port: String) -> String {
        let cleanHost = host.trimmingCharacters(in: .whitespacesAndNewlines)
        let cleanPort = port.trimmingCharacters(in: .whitespacesAndNewlines)
        return cleanPort.isEmpty ? cleanHost : "\(cleanHost):\(cleanPort)"
    }

    private func parseDevice(_ line: Substring) -> AndroidDevice? {
        let fields = line.split(whereSeparator: \Character.isWhitespace)
        guard fields.count >= 2 else { return nil }

        let properties = Dictionary(uniqueKeysWithValues: fields.dropFirst(2).compactMap { field -> (String, String)? in
            let pair = field.split(separator: ":", maxSplits: 1).map(String.init)
            guard pair.count == 2 else { return nil }
            return (pair[0], pair[1])
        })

        return AndroidDevice(
            serial: String(fields[0]),
            state: String(fields[1]),
            model: properties["model"],
            product: properties["product"],
            device: properties["device"]
        )
    }
}
