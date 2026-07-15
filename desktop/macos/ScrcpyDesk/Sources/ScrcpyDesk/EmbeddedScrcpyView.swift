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
    private static let registeredViews = NSHashTable<ScrcpyRenderView>.weakObjects()
    private static var sharedEventMonitor: Any?

    var didAttachToWindow: (() -> Void)?
    var session: OpaquePointer?
    var visibleInteractionRect: NSRect = .zero
    private var trackingArea: NSTrackingArea?
    private var mouseButtons: UInt32 = 0
    private var compositionText = NSAttributedString()
    private var compositionSelection = NSRange(location: NSNotFound, length: 0)
    private var isDragging: Bool { mouseButtons != 0 }

    override var acceptsFirstResponder: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window != nil {
            window?.acceptsMouseMovedEvents = true
            Self.registeredViews.add(self)
            installEventMonitorIfNeeded()
            didAttachToWindow?()
        } else {
            Self.registeredViews.remove(self)
        }
    }

    deinit {
        Self.registeredViews.remove(self)
    }

    private func installEventMonitorIfNeeded() {
        guard Self.sharedEventMonitor == nil else { return }
        let mouseMask: NSEvent.EventTypeMask = [
            .leftMouseDown, .leftMouseUp, .rightMouseDown, .rightMouseUp,
            .otherMouseDown, .otherMouseUp, .mouseMoved,
            .leftMouseDragged, .rightMouseDragged, .otherMouseDragged,
            .scrollWheel
        ]
        let mask = mouseMask.union([.keyDown, .keyUp, .flagsChanged])
        Self.sharedEventMonitor = NSEvent.addLocalMonitorForEvents(matching: mask) { event in
            let views = Self.registeredViews.allObjects
            switch event.type {
            case .leftMouseUp, .rightMouseUp, .otherMouseUp,
                 .leftMouseDragged, .rightMouseDragged, .otherMouseDragged:
                if let captured = views.first(where: \.isDragging) {
                    return captured.handleHostEvent(event) ? nil : event
                }
            default:
                break
            }
            for view in views.reversed()
                where view.handleHostEvent(event) {
                return nil
            }
            return event
        }
    }

    fileprivate func handleHostEvent(_ event: NSEvent) -> Bool {
        guard session != nil, let renderWindow = window else { return false }
        // The render windows are mouse-transparent children of the main app
        // window, so normal gallery input arrives on their parent. Never
        // route events from sheets, alerts, popovers or other modal windows
        // into Android; otherwise their controls cannot receive clicks.
        let inputWindow = renderWindow.parent ?? renderWindow
        guard event.window === inputWindow || event.window === renderWindow else {
            return false
        }
        let localPoint = localPoint(for: event)
        let isInside = visibleInteractionRect.contains(localPoint)
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
        guard let session else { return }
        _ = scrcpy_embedded_session_mouse_wheel(
            session,
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
        guard let session else { return }
        _ = scrcpy_embedded_session_mouse_button(
            session,
            down,
            button,
            Float(point.x),
            Float(point.y),
            UInt8(clamping: event.clickCount)
        )
    }

    private func sendMouseMotion(_ event: NSEvent) {
        let point = scrcpyPoint(for: event)
        guard let session else { return }
        _ = scrcpy_embedded_session_mouse_motion(
            session,
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
            guard let session else { return }
            _ = scrcpy_embedded_session_key(
                session,
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
            guard let session else { return }
            text.withCString { _ = scrcpy_embedded_session_text(session, $0) }
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
    var geometryDidChange: ((NSRect, NSRect) -> Void)?
    var sheetVisibilityDidChange: ((Bool) -> Void)?
    private var scrollObserver: NSObjectProtocol?
    private var windowObservers: [NSObjectProtocol] = []

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        stopObservingScroll()
        stopObservingWindow()
        guard window != nil else { return }
        observeScroll()
        observeWindow()
        didAttachToWindow?()
        notifyGeometryChange()
    }

    override func layout() {
        super.layout()
        notifyGeometryChange()
    }

    deinit {
        stopObservingScroll()
        stopObservingWindow()
    }

    private func observeScroll() {
        guard let clipView = enclosingScrollView?.contentView else { return }
        clipView.postsBoundsChangedNotifications = true
        scrollObserver = NotificationCenter.default.addObserver(
            forName: NSView.boundsDidChangeNotification,
            object: clipView,
            queue: .main
        ) { [weak self] _ in
            self?.notifyGeometryChange()
        }
    }

    private func stopObservingScroll() {
        if let scrollObserver {
            NotificationCenter.default.removeObserver(scrollObserver)
            self.scrollObserver = nil
        }
    }

    private func observeWindow() {
        guard let window else { return }
        let center = NotificationCenter.default
        windowObservers = [
            center.addObserver(
                forName: NSWindow.willBeginSheetNotification,
                object: window,
                queue: .main
            ) { [weak self] _ in
                self?.sheetVisibilityDidChange?(true)
            },
            center.addObserver(
                forName: NSWindow.didEndSheetNotification,
                object: window,
                queue: .main
            ) { [weak self] _ in
                self?.sheetVisibilityDidChange?(false)
                self?.notifyGeometryChange()
            },
        ]
    }

    private func stopObservingWindow() {
        let center = NotificationCenter.default
        windowObservers.forEach(center.removeObserver)
        windowObservers.removeAll()
    }

    fileprivate func notifyGeometryChange() {
        guard let window, !bounds.isEmpty else { return }
        let fullInWindow = convert(bounds, to: nil)
        var visibleInWindow = fullInWindow

        // NSView.visibleRect may temporarily report the whole view when a new
        // SwiftUI item is inserted, before NSScrollView completes its layout.
        // Explicitly intersect with both hard clipping boundaries so an SDL
        // child window can never draw outside the app or over the sidebar.
        if let contentView = window.contentView {
            let windowContentRect = contentView.convert(contentView.bounds, to: nil)
            visibleInWindow = visibleInWindow.intersection(windowContentRect)
        }
        if let clipView = enclosingScrollView?.contentView {
            let scrollViewport = clipView.convert(clipView.bounds, to: nil)
            visibleInWindow = visibleInWindow.intersection(scrollViewport)
        } else {
            let reportedVisible = convert(visibleRect.intersection(bounds), to: nil)
            visibleInWindow = visibleInWindow.intersection(reportedVisible)
        }
        geometryDidChange?(
            window.convertToScreen(fullInWindow),
            window.convertToScreen(visibleInWindow)
        )
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
        // A sibling card may just have been removed. SwiftUI can move this
        // host through an ancestor without changing the host's own bounds,
        // so explicitly resynchronize the independent render window.
        nsView.notifyGeometryChange()
        DispatchQueue.main.async { [weak nsView] in
            nsView?.notifyGeometryChange()
        }
    }

    static func dismantleNSView(_ nsView: ScrcpyHostView, coordinator: Coordinator) {
        coordinator.stop()
        nsView.didAttachToWindow = nil
        nsView.geometryDidChange = nil
        nsView.sheetVisibilityDidChange = nil
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
        private var renderContainer: NSView?
        private var renderView: ScrcpyRenderView?
        private var session: OpaquePointer?
        private var renderSuspendedForSheet = false
        private var lastRefreshTime: TimeInterval = 0

        init(state: Binding<EmbeddedSessionState>) {
            stateBinding = state
        }

        func request(serial: String) {
            guard requestedSerial != serial else { return }
            requestedSerial = serial
            attemptedSerial = nil

            if activeSerial != nil {
                if let session {
                    scrcpy_embedded_session_stop(session)
                }
            } else {
                startRequestedSessionIfPossible()
            }
        }

        func attachIfNeeded(view: ScrcpyHostView, adbPath: URL, serverPath: URL) {
            guard !initialized, let window = view.window else { return }

            let surface = ScrcpyRenderView(frame: view.bounds)
            surface.wantsLayer = true
            surface.layer?.backgroundColor = NSColor.black.cgColor

            let container = NSView(frame: view.bounds)
            container.wantsLayer = true
            container.layer?.masksToBounds = true
            container.addSubview(surface)

            let fullRect = window.convertToScreen(view.convert(view.bounds, to: nil))
            let child = EmbeddedRenderWindow(
                contentRect: fullRect,
                styleMask: .borderless,
                backing: .buffered,
                defer: false
            )
            child.backgroundColor = .clear
            child.isOpaque = false
            child.hasShadow = false
            child.isMovable = false
            // Input is routed by the app-wide local event monitor according
            // to each surface's visible clip. Keeping the transparent overlay
            // mouse-transparent prevents its hidden area from blocking the
            // SwiftUI sidebar or horizontal scroller.
            child.ignoresMouseEvents = true
            child.contentView = container
            child.inputView = surface
            window.addChildWindow(child, ordered: .above)

            hostWindow = window
            renderWindow = child
            renderContainer = container
            renderView = surface
            view.geometryDidChange = { [weak self] full, visible in
                self?.updateRenderGeometry(full: full, visible: visible)
            }
            view.sheetVisibilityDidChange = { [weak self, weak view] isVisible in
                self?.setRenderSuspendedForSheet(isVisible)
                if !isVisible {
                    view?.notifyGeometryChange()
                }
            }

            let iconDirectory = Bundle.main.resourceURL?.path
                ?? FileManager.default.currentDirectoryPath + "/app/data"

            let created = adbPath.path.withCString { adbCString in
                serverPath.path.withCString { serverCString in
                    iconDirectory.withCString { iconCString in
                        scrcpy_embedded_session_create(
                            Unmanaged.passUnretained(child).toOpaque(),
                            Unmanaged.passUnretained(surface).toOpaque(),
                            adbCString,
                            serverCString,
                            iconCString
                        )
                    }
                }
            }

            guard let created else {
                window.removeChildWindow(child)
                child.orderOut(nil)
                renderWindow = nil
                renderContainer = nil
                renderView = nil
                stateBinding.wrappedValue = .failed
                return
            }

            session = created
            surface.session = created
            initialized = true
            timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
                MainActor.assumeIsolated {
                    self?.pump()
                }
            }
            view.notifyGeometryChange()
            DispatchQueue.main.async { [weak view] in
                // Re-evaluate after SwiftUI/NSScrollView finishes positioning
                // a newly inserted card.
                view?.notifyGeometryChange()
            }
            startRequestedSessionIfPossible()
        }

        private func updateRenderGeometry(full: NSRect, visible: NSRect) {
            guard let renderWindow, let renderContainer, let renderView else { return }
            guard !renderSuspendedForSheet, hostWindow?.attachedSheet == nil else {
                renderView.visibleInteractionRect = .zero
                renderWindow.orderOut(nil)
                return
            }
            let clipped = full.intersection(visible)
            guard !clipped.isNull, clipped.width > 1, clipped.height > 1 else {
                renderView.visibleInteractionRect = .zero
                renderWindow.orderOut(nil)
                return
            }

            // Keep the Cocoa/SDL window at its full size. Resizing it to the
            // visible intersection makes SDL letterbox into that narrower
            // size and visibly stretches the Android frame while scrolling.
            renderWindow.setFrame(full, display: true)
            renderContainer.frame = NSRect(origin: .zero, size: full.size)
            renderView.frame = renderContainer.bounds

            let localClip = NSRect(
                x: clipped.minX - full.minX,
                y: clipped.minY - full.minY,
                width: clipped.width,
                height: clipped.height
            )
            renderView.visibleInteractionRect = localClip

            let mask = CAShapeLayer()
            mask.frame = renderContainer.bounds
            mask.path = CGPath(rect: localClip, transform: nil)
            mask.fillColor = NSColor.black.cgColor
            renderContainer.layer?.mask = mask
            renderContainer.layer?.setNeedsDisplay()

            // SDL may install its Metal layer directly on the render NSView.
            // Apply the same mask there as well instead of relying only on
            // ancestor-layer clipping across a child NSWindow boundary.
            let surfaceMask = CAShapeLayer()
            surfaceMask.frame = renderView.bounds
            surfaceMask.path = CGPath(rect: localClip, transform: nil)
            surfaceMask.fillColor = NSColor.black.cgColor
            renderView.layer?.mask = surfaceMask
            let becameVisible = !renderWindow.isVisible
            if becameVisible {
                renderWindow.orderFront(nil)
            }
            if becameVisible {
                refreshRenderSurface()
            }
        }

        private func setRenderSuspendedForSheet(_ suspended: Bool) {
            renderSuspendedForSheet = suspended
            if suspended {
                renderView?.visibleInteractionRect = .zero
                renderWindow?.orderOut(nil)
            }
        }

        func stop() {
            requestedSerial = nil
            timer?.invalidate()
            timer = nil

            let sessionToDestroy = initialized ? session : nil
            if let sessionToDestroy {
                scrcpy_embedded_session_stop(sessionToDestroy)
            }

            initialized = false
            session = nil
            activeSerial = nil
            attemptedSerial = nil
            renderSuspendedForSheet = false
            lastRefreshTime = 0
            renderView?.session = nil
            renderView?.visibleInteractionRect = .zero
            if let renderWindow {
                hostWindow?.removeChildWindow(renderWindow)
                renderWindow.orderOut(nil)
            }

            let teardown = sessionToDestroy.map {
                SessionTeardown(
                    session: $0,
                    renderWindow: renderWindow,
                    renderContainer: renderContainer,
                    renderView: renderView
                )
            }
            renderWindow = nil
            renderContainer = nil
            renderView = nil
            hostWindow = nil

            // Joining the scrcpy worker on the main thread prevents SwiftUI
            // from completing the ForEach deletion and leaves a black card on
            // screen. Retain the AppKit render objects while teardown runs,
            // but do the blocking join on a worker queue.
            if let teardown {
                DispatchQueue.global(qos: .userInitiated).async {
                    teardown.destroy()
                }
            }
        }

        private func pump() {
            guard let session else { return }
            let rawStatus = scrcpy_embedded_session_pump(session)
            let newState = Self.map(rawStatus)
            stateBinding.wrappedValue = newState

            // A host-provided Metal view may lose its drawable after being
            // occluded, after display sleep, or while the app is inactive.
            // Android does not emit frames for an unchanged screen, so replay
            // the retained texture occasionally to make the view self-heal.
            let now = ProcessInfo.processInfo.systemUptime
            if newState == .running, now - lastRefreshTime >= 2 {
                refreshRenderSurface(now: now)
            }

            if newState == .idle || newState == .failed || newState == .disconnected {
                activeSerial = nil
                startRequestedSessionIfPossible()
            }
        }

        private func refreshRenderSurface(now: TimeInterval? = nil) {
            guard !renderSuspendedForSheet,
                  renderWindow?.isVisible == true,
                  let session else { return }
            if scrcpy_embedded_session_refresh(session) {
                lastRefreshTime = now ?? ProcessInfo.processInfo.systemUptime
            }
        }

        private func startRequestedSessionIfPossible() {
            guard initialized,
                  let serial = requestedSerial,
                  activeSerial == nil,
                  attemptedSerial != serial else { return }

            guard let session else { return }
            let current = Self.map(scrcpy_embedded_session_get_status(session))
            guard current == .idle || current == .failed || current == .disconnected else { return }

            attemptedSerial = serial
            let started = serial.withCString {
                scrcpy_embedded_session_start(session, $0)
            }
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

private final class SessionTeardown: @unchecked Sendable {
    private let session: OpaquePointer
    // SDL still owns native references during screen destruction. Keep the
    // hidden render hierarchy alive until the C session has fully exited.
    private let renderWindow: NSWindow?
    private let renderContainer: NSView?
    private let renderView: ScrcpyRenderView?

    init(
        session: OpaquePointer,
        renderWindow: NSWindow?,
        renderContainer: NSView?,
        renderView: ScrcpyRenderView?
    ) {
        self.session = session
        self.renderWindow = renderWindow
        self.renderContainer = renderContainer
        self.renderView = renderView
    }

    func destroy() {
        scrcpy_embedded_session_destroy(session)
        // Ensure AppKit objects retained above are ultimately released on the
        // main thread rather than by this background work item.
        DispatchQueue.main.async { [self] in
            withExtendedLifetime(renderWindow) {}
            withExtendedLifetime(renderContainer) {}
            withExtendedLifetime(renderView) {}
        }
    }
}
