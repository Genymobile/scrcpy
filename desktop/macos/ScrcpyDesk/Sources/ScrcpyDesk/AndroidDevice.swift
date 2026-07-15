import Foundation

struct AndroidDevice: Identifiable, Hashable {
    enum Connection: String {
        case usb = "USB"
        case network = "Wi-Fi"
    }

    let serial: String
    let state: String
    let model: String?
    let product: String?
    let device: String?

    var id: String { serial }
    var name: String { model?.replacingOccurrences(of: "_", with: " ") ?? serial }
    var isReady: Bool { state == "device" }
    var connection: Connection { serial.contains(":") ? .network : .usb }

    var stateLabel: String {
        switch state {
        case "device": return "已连接"
        case "unauthorized": return "等待授权"
        case "offline": return "离线"
        default: return state
        }
    }
}
