import Foundation

struct CommandOutput {
    let data: Data
    let errorData: Data
    let exitCode: Int32

    var text: String {
        String(data: data, encoding: .utf8) ?? ""
    }

    var errorText: String {
        String(data: errorData, encoding: .utf8) ?? ""
    }
}

enum CommandError: LocalizedError {
    case failedToLaunch(String)
    case failed(command: String, code: Int32, message: String)

    var errorDescription: String? {
        switch self {
        case .failedToLaunch(let message):
            return message
        case .failed(let command, let code, let message):
            return "\(command) 执行失败（\(code)）：\(message)"
        }
    }
}

enum CommandRunner {
    static func run(
        executable: URL,
        arguments: [String],
        environment: [String: String] = [:]
    ) async throws -> CommandOutput {
        try await Task.detached(priority: .userInitiated) {
            let process = Process()
            let standardOutput = Pipe()
            let standardError = Pipe()

            process.executableURL = executable
            process.arguments = arguments
            process.standardOutput = standardOutput
            process.standardError = standardError
            process.environment = ProcessInfo.processInfo.environment.merging(environment) { _, new in new }

            do {
                try process.run()
            } catch {
                throw CommandError.failedToLaunch("无法启动 \(executable.path)：\(error.localizedDescription)")
            }

            // Start draining stdout immediately. This is important for PNG screenshots,
            // which are normally larger than the operating system pipe buffer.
            let outputData = standardOutput.fileHandleForReading.readDataToEndOfFile()
            let errorData = standardError.fileHandleForReading.readDataToEndOfFile()
            process.waitUntilExit()

            return CommandOutput(
                data: outputData,
                errorData: errorData,
                exitCode: process.terminationStatus
            )
        }.value
    }
}

enum ToolLocator {
    static func find(_ name: String) -> URL? {
        var candidates: [String] = []

        if let resourceURL = Bundle.main.resourceURL {
            candidates.append(resourceURL.appendingPathComponent(name).path)
        }

        candidates += [
            "/opt/homebrew/bin/\(name)",
            "/usr/local/bin/\(name)",
            "/opt/local/bin/\(name)"
        ]

        if name == "adb" {
            candidates += [
                "\(NSHomeDirectory())/Library/Android/sdk/platform-tools/adb",
                "/Applications/Android Studio.app/Contents/sdk/platform-tools/adb"
            ]
        }

        let pathEntries = ProcessInfo.processInfo.environment["PATH"]?.split(separator: ":") ?? []
        candidates += pathEntries.map { "\($0)/\(name)" }

        return candidates
            .map(URL.init(fileURLWithPath:))
            .first { FileManager.default.isExecutableFile(atPath: $0.path) }
    }

    static func findScrcpyServer() -> URL? {
        var candidates: [URL] = []

        if let resourceURL = Bundle.main.resourceURL {
            candidates.append(resourceURL.appendingPathComponent("scrcpy-server"))
        }

        if let configured = ProcessInfo.processInfo.environment["SCRCPY_SERVER_PATH"] {
            candidates.append(URL(fileURLWithPath: configured))
        }

        let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        candidates += [
            root.appendingPathComponent("server/build/outputs/apk/release/server-release-unsigned.apk"),
            root.appendingPathComponent("server/build/outputs/apk/debug/server-debug.apk"),
            root.appendingPathComponent("scrcpy-server"),
            URL(fileURLWithPath: "/opt/homebrew/share/scrcpy/scrcpy-server"),
            URL(fileURLWithPath: "/usr/local/share/scrcpy/scrcpy-server"),
        ]

        return candidates.first { FileManager.default.isReadableFile(atPath: $0.path) }
    }
}
