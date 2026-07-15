import AppKit
import CScrcpy
import SwiftUI

enum EmbeddedSessionState: Equatable {
    case idle
    case starting
    case running
    case failed
    case disconnected

    var label: String {
        switch self {
        case .idle: return "等待连接"
        case .starting: return "正在建立 scrcpy 会话…"
        case .running: return "实时交互中"
        case .failed: return "scrcpy 会话启动失败"
        case .disconnected: return "设备已断开"
        }
    }
}

final class ScrcpyRenderView: NSView, NSTextInputClient {
    private static weak var activeView: ScrcpyRenderView?
    private static var sharedEventMonitor: Any?

    var didAttachToWindow: (() -> Void)?
    private var trackingArea: NSTrackingArea?
    private var mouseButtons: UInt32 = 0
    private var compositionText = NSAttributedString()
    private var compositionSelection = NSRange(location: NSNotFound, length: 0)

    override var acceptsFirstResponder: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window != nil {
            window?.acceptsMouseMovedEvents = true
            installEventMonitorIfNeeded()
            didAttachToWindow?()
        }
    }

    deinit {
        if Self.activeView === self {
            Self.activeView = nil
        }
    }

    private func installEventMonitorIfNeeded() {
        Self.activeView = self
        guard Self.sharedEventMonitor == nil else { return }
        let mouseMask: NSEvent.EventTypeMask = [
            .leftMouseDown, .leftMouseUp, .rightMouseDown, .rightMouseUp,
            .otherMouseDown, .otherMouseUp, .mouseMoved,
            .leftMouseDragged, .rightMouseDragged, .otherMouseDragged,
            .scrollWheel
        ]
        let mask = mouseMask.union([.keyDown, .keyUp, .flagsChanged])
        Self.sharedEventMonitor = NSEvent.addLocalMonitorForEvents(matching: mask) { event in
            guard let view = Self.activeView else { return event }
            return view.handleHostEvent(event) ? nil : event
        }
    }

    fileprivate func handleHostEvent(_ event: NSEvent) -> Bool {
        let localPoint = localPoint(for: event)
        let isInside = bounds.contains(localPoint)
        let isDragging = mouseButtons != 0

        switch event.type {
        case .leftMouseDown where isInside: mouseDown(with: event)
        case .leftMouseUp where isInside || isDragging: mouseUp(with: event)
        case .rightMouseDown where isInside: rightMouseDown(with: event)
        case .rightMouseUp where isInside || isDragging: rightMouseUp(with: event)
        case .otherMouseDown where isInside: otherMouseDown(with: event)
        case .otherMouseUp where isInside || isDragging: otherMouseUp(with: event)
        case .mouseMoved where isInside: mouseMoved(with: event)
        case .leftMouseDragged where isInside || isDragging: mouseDragged(with: event)
        case .rightMouseDragged where isInside || isDragging: rightMouseDragged(with: event)
        case .otherMouseDragged where isInside || isDragging: otherMouseDragged(with: event)
        case .scrollWheel where isInside: scrollWheel(with: event)
        case .keyDown where window?.firstResponder === self: keyDown(with: event)
        case .keyUp where window?.firstResponder === self: keyUp(with: event)
        case .flagsChanged where window?.firstResponder === self: flagsChanged(with: event)
        default: return false
        }
        return true
    }

    override func updateTrackingAreas() {
        if let trackingArea {
            removeTrackingArea(trackingArea)
        }
        let area = NSTrackingArea(
            rect: bounds,
            options: [.activeInKeyWindow, .inVisibleRect, .mouseMoved, .enabledDuringMouseDrag],
            owner: self
        )
        addTrackingArea(area)
        trackingArea = area
        super.updateTrackingAreas()
    }

    override func mouseDown(with event: NSEvent) {
        window?.makeFirstResponder(self)
        sendMouseButton(event, button: 1, down: true)
    }

    override func mouseUp(with event: NSEvent) {
        sendMouseButton(event, button: 1, down: false)
    }

    override func rightMouseDown(with event: NSEvent) {
        window?.makeFirstResponder(self)
        sendMouseButton(event, button: 3, down: true)
    }

    override func rightMouseUp(with event: NSEvent) {
        sendMouseButton(event, button: 3, down: false)
    }

    override func otherMouseDown(with event: NSEvent) {
        window?.makeFirstResponder(self)
        sendMouseButton(event, button: event.buttonNumber == 2 ? 2 : 4, down: true)
    }

    override func otherMouseUp(with event: NSEvent) {
        sendMouseButton(event, button: event.buttonNumber == 2 ? 2 : 4, down: false)
    }

    override func mouseMoved(with event: NSEvent) {
        sendMouseMotion(event)
    }

    override func mouseDragged(with event: NSEvent) {
        sendMouseMotion(event)
    }

    override func rightMouseDragged(with event: NSEvent) {
        sendMouseMotion(event)
    }

    override func otherMouseDragged(with event: NSEvent) {
        sendMouseMotion(event)
    }

    override func scrollWheel(with event: NSEvent) {
        let point = scrcpyPoint(for: event)
        _ = scrcpy_embedded_mouse_wheel(
            Float(event.scrollingDeltaX),
            Float(event.scrollingDeltaY),
            Float(point.x),
            Float(point.y)
        )
    }

    override func keyDown(with event: NSEvent) {
        sendKey(event, down: true)

        let modifiers = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        if !modifiers.contains(.command),
           !modifiers.contains(.control),
           !modifiers.contains(.option) {
            interpretKeyEvents([event])
        }
    }

    override func keyUp(with event: NSEvent) {
        sendKey(event, down: false)
    }

    override func flagsChanged(with event: NSEvent) {
        guard let name = modifierKeyName(for: event.keyCode) else { return }
        let flags = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        let down: Bool
        switch event.keyCode {
        case 54, 55: down = flags.contains(.command)
        case 56, 60: down = flags.contains(.shift)
        case 58, 61: down = flags.contains(.option)
        case 59, 62: down = flags.contains(.control)
        default: down = flags.contains(.capsLock)
        }
        sendNamedKey(name, down: down, repeatKey: false, flags: flags)
    }

    private func sendMouseButton(_ event: NSEvent, button: UInt8, down: Bool) {
        let mask = UInt32(1) << UInt32(button - 1)
        if down {
            mouseButtons |= mask
        } else {
            mouseButtons &= ~mask
        }
        let point = scrcpyPoint(for: event)
        _ = scrcpy_embedded_mouse_button(
            down,
            button,
            Float(point.x),
            Float(point.y),
            UInt8(clamping: event.clickCount)
        )
    }

    private func sendMouseMotion(_ event: NSEvent) {
        let point = scrcpyPoint(for: event)
        _ = scrcpy_embedded_mouse_motion(
            Float(point.x),
            Float(point.y),
            Float(event.deltaX),
            Float(-event.deltaY),
            mouseButtons
        )
    }

    private func scrcpyPoint(for event: NSEvent) -> CGPoint {
        let point = localPoint(for: event)
        return CGPoint(x: point.x, y: bounds.height - point.y)
    }

    private func localPoint(for event: NSEvent) -> CGPoint {
        if event.window === window {
            return convert(event.locationInWindow, from: nil)
        }
        guard let window else { return .zero }
        let windowPoint = window.convertPoint(fromScreen: NSEvent.mouseLocation)
        return convert(windowPoint, from: nil)
    }

    private func sendKey(_ event: NSEvent, down: Bool) {
        guard let name = keyName(for: event) else { return }
        let flags = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        sendNamedKey(name, down: down, repeatKey: event.isARepeat, flags: flags)
    }

    private func sendNamedKey(
        _ name: String,
        down: Bool,
        repeatKey: Bool,
        flags: NSEvent.ModifierFlags
    ) {
        name.withCString {
            _ = scrcpy_embedded_key(
                down,
                $0,
                flags.contains(.shift),
                flags.contains(.control),
                flags.contains(.option),
                flags.contains(.command),
                repeatKey
            )
        }
    }

    private func keyName(for event: NSEvent) -> String? {
        switch event.keyCode {
        case 36, 76: return "Return"
        case 48: return "Tab"
        case 49: return "Space"
        case 51: return "Backspace"
        case 53: return "Escape"
        case 115: return "Home"
        case 116: return "PageUp"
        case 117: return "Delete"
        case 119: return "End"
        case 121: return "PageDown"
        case 123: return "Left"
        case 124: return "Right"
        case 125: return "Down"
        case 126: return "Up"
        default:
            guard let value = event.charactersIgnoringModifiers?.lowercased(),
                  !value.isEmpty else { return nil }
            return value
        }
    }

    func insertText(_ string: Any, replacementRange: NSRange) {
        let text: String
        if let attributed = string as? NSAttributedString {
            text = attributed.string
        } else if let plain = string as? String {
            text = plain
        } else {
            return
        }
        if !text.isEmpty {
            text.withCString { _ = scrcpy_embedded_text($0) }
        }
        unmarkText()
    }

    override func doCommand(by selector: Selector) {
        // Navigation and editing commands have already been forwarded from
        // keyDown() as scrcpy SDK key events.
    }

    func setMarkedText(
        _ string: Any,
        selectedRange: NSRange,
        replacementRange: NSRange
    ) {
        if let attributed = string as? NSAttributedString {
            compositionText = attributed
        } else if let plain = string as? String {
            compositionText = NSAttributedString(string: plain)
        }
        compositionSelection = selectedRange
    }

    func unmarkText() {
        compositionText = NSAttributedString()
        compositionSelection = NSRange(location: NSNotFound, length: 0)
    }

    func selectedRange() -> NSRange { compositionSelection }

    func markedRange() -> NSRange {
        compositionText.length == 0
            ? NSRange(location: NSNotFound, length: 0)
            : NSRange(location: 0, length: compositionText.length)
    }

    func hasMarkedText() -> Bool { compositionText.length > 0 }

    func attributedSubstring(
        forProposedRange range: NSRange,
        actualRange: NSRangePointer?
    ) -> NSAttributedString? {
        nil
    }

    func validAttributesForMarkedText() -> [NSAttributedString.Key] { [] }

    func firstRect(
        forCharacterRange range: NSRange,
        actualRange: NSRangePointer?
    ) -> NSRect {
        guard let window else { return .zero }
        let local = NSRect(x: bounds.midX, y: bounds.minY, width: 1, height: 1)
        return window.convertToScreen(convert(local, to: nil))
    }

    func characterIndex(for point: NSPoint) -> Int { NSNotFound }

    private func modifierKeyName(for keyCode: UInt16) -> String? {
        switch keyCode {
        case 54: return "Right GUI"
        case 55: return "Left GUI"
        case 56: return "Left Shift"
        case 60: return "Right Shift"
        case 58: return "Left Alt"
        case 61: return "Right Alt"
        case 59: return "Left Ctrl"
        case 62: return "Right Ctrl"
        case 57: return "CapsLock"
        default: return nil
        }
    }
}

final class ScrcpyHostView: NSView {
    var didAttachToWindow: (() -> Void)?
    var geometryDidChange: ((NSRect) -> Void)?

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window != nil {
            didAttachToWindow?()
            notifyGeometryChange()
        }
    }

    override func layout() {
        super.layout()
        notifyGeometryChange()
    }

    private func notifyGeometryChange() {
        guard let window, !bounds.isEmpty else { return }
        geometryDidChange?(window.convertToScreen(convert(bounds, to: nil)))
    }
}

final class EmbeddedRenderWindow: NSWindow {
    weak var inputView: ScrcpyRenderView?

    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { false }

    override func sendEvent(_ event: NSEvent) {
        if inputView?.handleHostEvent(event) == true {
            return
        }
        super.sendEvent(event)
    }
}

struct EmbeddedScrcpyView: NSViewRepresentable {
    let serial: String
    let adbPath: URL
    let serverPath: URL
    @Binding var state: EmbeddedSessionState

    func makeCoordinator() -> Coordinator {
        Coordinator(state: $state)
    }

    func makeNSView(context: Context) -> ScrcpyHostView {
        let view = ScrcpyHostView(frame: .zero)
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.black.cgColor
        view.layer?.cornerRadius = 12
        view.layer?.masksToBounds = true
        view.didAttachToWindow = { [weak view, weak coordinator = context.coordinator] in
            guard let view else { return }
            coordinator?.attachIfNeeded(
                view: view,
                adbPath: adbPath,
                serverPath: serverPath
            )
        }
        context.coordinator.request(serial: serial)
        return view
    }

    func updateNSView(_ nsView: ScrcpyHostView, context: Context) {
        context.coordinator.request(serial: serial)
        context.coordinator.attachIfNeeded(
            view: nsView,
            adbPath: adbPath,
            serverPath: serverPath
        )
    }

    static func dismantleNSView(_ nsView: ScrcpyHostView, coordinator: Coordinator) {
        coordinator.stop()
        nsView.didAttachToWindow = nil
        nsView.geometryDidChange = nil
    }

    @MainActor
    final class Coordinator {
        private var stateBinding: Binding<EmbeddedSessionState>
        private var timer: Timer?
        private var initialized = false
        private var requestedSerial: String?
        private var activeSerial: String?
        private var attemptedSerial: String?
        private weak var hostWindow: NSWindow?
        private var renderWindow: NSWindow?
        private var renderView: ScrcpyRenderView?

        init(state: Binding<EmbeddedSessionState>) {
            stateBinding = state
        }

        func request(serial: String) {
            guard requestedSerial != serial else { return }
            requestedSerial = serial
            attemptedSerial = nil

            if activeSerial != nil {
                scrcpy_embedded_stop()
            } else {
                startRequestedSessionIfPossible()
            }
        }

        func attachIfNeeded(view: ScrcpyHostView, adbPath: URL, serverPath: URL) {
            guard !initialized, let window = view.window else { return }

            let surface = ScrcpyRenderView(frame: NSRect(origin: .zero, size: view.bounds.size))
            surface.wantsLayer = true
            surface.layer?.backgroundColor = NSColor.black.cgColor
            surface.layer?.cornerRadius = 12
            surface.layer?.masksToBounds = true

            let screenRect = window.convertToScreen(view.convert(view.bounds, to: nil))
            let child = EmbeddedRenderWindow(
                contentRect: screenRect,
                styleMask: .borderless,
                backing: .buffered,
                defer: false
            )
            child.backgroundColor = .clear
            child.isOpaque = false
            child.hasShadow = false
            child.isMovable = false
            child.ignoresMouseEvents = false
            child.contentView = surface
            child.inputView = surface
            window.addChildWindow(child, ordered: .above)
            child.makeKeyAndOrderFront(nil)

            hostWindow = window
            renderWindow = child
            renderView = surface
            view.geometryDidChange = { [weak self] rect in
                self?.renderWindow?.setFrame(rect, display: true)
            }

            let iconDirectory = Bundle.main.resourceURL?.path
                ?? FileManager.default.currentDirectoryPath + "/app/data"

            let ok = adbPath.path.withCString { adbCString in
                serverPath.path.withCString { serverCString in
                    iconDirectory.withCString { iconCString in
                        scrcpy_embedded_initialize(
                            Unmanaged.passUnretained(child).toOpaque(),
                            Unmanaged.passUnretained(surface).toOpaque(),
                            adbCString,
                            serverCString,
                            iconCString
                        )
                    }
                }
            }

            guard ok else {
                window.removeChildWindow(child)
                child.orderOut(nil)
                renderWindow = nil
                renderView = nil
                stateBinding.wrappedValue = .failed
                return
            }

            initialized = true
            timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
                MainActor.assumeIsolated {
                    self?.pump()
                }
            }
            startRequestedSessionIfPossible()
        }

        func stop() {
            requestedSerial = nil
            if initialized {
                scrcpy_embedded_stop()
                let deadline = Date().addingTimeInterval(3)
                while Date() < deadline {
                    let current = Self.map(scrcpy_embedded_pump())
                    if current == .idle || current == .failed || current == .disconnected {
                        break
                    }
                    RunLoop.current.run(until: Date().addingTimeInterval(0.01))
                }
            }
            timer?.invalidate()
            timer = nil
            initialized = false
            activeSerial = nil
            if let renderWindow {
                hostWindow?.removeChildWindow(renderWindow)
                renderWindow.orderOut(nil)
            }
            renderWindow = nil
            renderView = nil
            hostWindow = nil
        }

        private func pump() {
            let rawStatus = scrcpy_embedded_pump()
            let newState = Self.map(rawStatus)
            stateBinding.wrappedValue = newState

            if newState == .idle || newState == .failed || newState == .disconnected {
                activeSerial = nil
                startRequestedSessionIfPossible()
            }
        }

        private func startRequestedSessionIfPossible() {
            guard initialized,
                  let serial = requestedSerial,
                  activeSerial == nil,
                  attemptedSerial != serial else { return }

            let current = Self.map(scrcpy_embedded_get_status())
            guard current == .idle || current == .failed || current == .disconnected else { return }

            attemptedSerial = serial
            let started = serial.withCString { scrcpy_embedded_start($0) }
            if started {
                activeSerial = serial
                stateBinding.wrappedValue = .starting
            } else {
                stateBinding.wrappedValue = .failed
            }
        }

        private static func map(_ status: scrcpy_embedded_status) -> EmbeddedSessionState {
            switch status {
            case SCRCPY_EMBEDDED_STARTING: return .starting
            case SCRCPY_EMBEDDED_RUNNING: return .running
            case SCRCPY_EMBEDDED_FAILED: return .failed
            case SCRCPY_EMBEDDED_DISCONNECTED: return .disconnected
            default: return .idle
            }
        }
    }
}
