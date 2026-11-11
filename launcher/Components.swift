import SwiftUI
import AppKit

// Shared building blocks from the design canvas's Foundations board and launcher.css. Every unit
// builds its screens from these, so a row, a sheet or a banner looks and speaks the same everywhere.
// Shared file: add new blocks in your own `// MARK: - <unit>` region at the end; don't change the
// signatures below without updating every caller (the contract lists them).

// MARK: - Page structure (moved from RootView, HomeView, ServersView)

struct Eyebrow: View {
    var text: String
    var size: CGFloat = 13
    var tracking: CGFloat = 0.32
    var color: Color?
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text.uppercased()).font(.stencil(size, heavy: false)).tracking(size * tracking).foregroundStyle(color ?? palette.accent)
    }
}

struct PageHeader: View {
    var eyebrow: String? = nil
    var title: String
    var subtitle: String? = nil
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            if let eyebrow { Eyebrow(text: eyebrow) }
            Text(title).font(.system(size: 30, weight: .bold)).tracking(-0.3).foregroundStyle(palette.heading).accessibilityAddTraits(.isHeader)
            if let subtitle { Text(subtitle).font(.system(size: 13)).foregroundStyle(palette.secondary) }
        }
    }
}

/// Content panel: opaque and quiet, never glass. Padding equals the corner radius, so nothing
/// inside reaches into a corner.
struct ContentPanel<Content: View>: View {
    var radius: CGFloat = 22
    var padding = EdgeInsets(top: 22, leading: 22, bottom: 22, trailing: 22)
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    var body: some View {
        VStack(alignment: .leading, spacing: 0) { content }
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .topLeading)
            .background(palette.panel, in: RoundedRectangle(cornerRadius: radius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
            .shapeAudit(.container, .rounded(radius), "panel")
    }
}

/// A panel's title with an optional link-style action on the right (“Show all”, “Release Notes”).
struct PanelHeading: View {
    var title: String
    var action: String?
    var perform: (() -> Void)?
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.dark ? Palette.hex(0xefe9db) : palette.heading).accessibilityAddTraits(.isHeader)
            Spacer()
            if let action, let perform { Button(action, action: perform).quietAction().padding(.horizontal, -6) }
        }
    }
}

/// Pins a group of toolbar items to the trailing edge: ToolbarSpacer on macOS 26, a flexible space before.
struct TrailingToolbar<Items: View>: ViewModifier {
    @ViewBuilder var items: Items
    func body(content: Content) -> some View {
        #if COD2_LIQUID_GLASS
        if #available(macOS 26.0, *) {
            content.toolbar { ToolbarSpacer(.flexible); ToolbarItemGroup(placement: .automatic) { items } }
        } else { legacy(content) }
        #else
        legacy(content)
        #endif
    }
    private func legacy(_ content: Content) -> some View {
        content.toolbar { ToolbarItem(placement: .automatic) { Spacer() }; ToolbarItemGroup(placement: .automatic) { items } }
    }
}
extension View {
    func trailingToolbar<Items: View>(@ViewBuilder _ items: () -> Items) -> some View { modifier(TrailingToolbar(items: items)) }
}

/// Reads the detail column's origin in window coordinates so content can keep the design's grid.
struct WindowOriginReader: View {
    @Binding var origin: CGPoint
    var body: some View {
        GeometryReader { proxy in
            Color.clear.onAppear { origin = proxy.frame(in: .global).origin }
                .onChange(of: proxy.frame(in: .global).origin) { _, value in origin = value }
        }
    }
}

/// List rows (servers, demos, matches): 60-point art 10 points in, concentric with a 20-point corner.
/// Trailing content keeps the corner radius from the edge; a button there sits as far in as from the top.
enum RowMetrics {
    static let inset: CGFloat = 10, radius: CGFloat = 20, trailing: CGFloat = 20, height: CGFloat = 60 + 2 * 10
    static var artRadius: CGFloat { Concentric.radius(in: radius, inset: inset) }
    static func buttonTrailing(_ size: ControlSize) -> CGFloat { (height - ControlMetrics.height(size)) / 2 }
}

// MARK: - Forms: section, group, row (Settings, sheets, setup and popovers use the same row)

/// Where a form group sits: on the window's ground (an opaque panel, radius 20, rows 20 in) or inside
/// glass or a sheet (a well, radius 16, rows 16 in). Sheets and popovers set `.inset` for their content.
enum FormContext { case panel, inset }
private struct FormContextKey: EnvironmentKey { static let defaultValue = FormContext.panel }
extension EnvironmentValues {
    var formContext: FormContext { get { self[FormContextKey.self] } set { self[FormContextKey.self] = newValue } }
}

enum FormMetrics {
    static func radius(_ context: FormContext) -> CGFloat { context == .panel ? 20 : 16 }
    static func rowInset(_ context: FormContext) -> CGFloat { context == .panel ? 20 : 16 }
    /// Row heights: 52 with a line of detail, 44 without.
    static let rowHeight: CGFloat = 44, detailRowHeight: CGFloat = 52
}

/// A titled section: the 13-point secondary title (a header for VoiceOver), its content, and an
/// optional 12-point footer.
struct FormSection<Content: View, Footer: View>: View {
    var title: String?
    @ViewBuilder var content: Content
    @ViewBuilder var footer: Footer
    @Environment(\.palette) private var palette
    init(_ title: String?, @ViewBuilder content: () -> Content, @ViewBuilder footer: () -> Footer) {
        self.title = title; self.content = content(); self.footer = footer()
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if let title {
                Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary)
                    .padding(.horizontal, 6).padding(.bottom, 8).accessibilityAddTraits(.isHeader)
            }
            content
            footer.font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 6).padding(.top, 8)
        }
    }
}
extension FormSection where Footer == EmptyView {
    init(_ title: String?, @ViewBuilder content: () -> Content) { self.init(title, content: content, footer: { EmptyView() }) }
}

/// Rows on one surface, split by hairlines: an opaque panel on the window, a well inside glass or a sheet.
struct FormGroup<Content: View>: View {
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    @Environment(\.formContext) private var context
    var body: some View {
        let radius = FormMetrics.radius(context)
        let shape = RoundedRectangle(cornerRadius: radius, style: .continuous)
        // Every row draws a hairline above itself; shifting up one point hides the first under the clip.
        VStack(spacing: 0) { content }
            .padding(.top, -1)
            .background(context == .panel ? palette.panel : palette.well, in: shape)
            .clipShape(shape)
            .overlay(shape.strokeBorder(context == .panel ? palette.panelStroke : palette.wellStroke, lineWidth: 1))
            .shapeAudit(.container, .rounded(radius), "form group")
    }
}

/// One form row: an optional leading symbol tile, the title and one line of detail, and the
/// control or controls on the right. VoiceOver reads the row as a group; give the control the
/// row's title as its label (the switch initializer does).
struct FormRow<Trailing: View>: View {
    var title: String
    var detail: String?
    var symbol: String?
    var monospacedTitle = false
    /// Taller than the standard 44/52 when a control must keep the group's corner radius from the edges.
    var minHeight: CGFloat?
    @ViewBuilder var trailing: Trailing
    @Environment(\.palette) private var palette
    @Environment(\.formContext) private var context
    init(_ title: String, detail: String? = nil, symbol: String? = nil, monospacedTitle: Bool = false, minHeight: CGFloat? = nil,
         @ViewBuilder trailing: () -> Trailing) {
        self.title = title; self.detail = detail; self.symbol = symbol; self.monospacedTitle = monospacedTitle; self.minHeight = minHeight
        self.trailing = trailing()
    }
    var body: some View {
        HStack(spacing: 14) {
            if let symbol {
                Image(systemName: symbol).font(.system(size: 15, weight: .medium)).foregroundStyle(palette.accent)
                    .frame(width: 30, height: 30)
                    .background(palette.selection, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                    .accessibilityHidden(true)
            }
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(monospacedTitle ? .typewriter(14) : .system(size: 14)).foregroundStyle(palette.text)
                    .fixedSize(horizontal: false, vertical: true)
                if let detail {
                    Text(detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).lineSpacing(1.5).fixedSize(horizontal: false, vertical: true)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            trailing
        }
        .padding(.horizontal, FormMetrics.rowInset(context)).padding(.vertical, 9)
        .frame(minHeight: minHeight ?? (detail == nil ? FormMetrics.rowHeight : FormMetrics.detailRowHeight))
        .overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) }
        .accessibilityElement(children: .contain)
    }
}
extension FormRow where Trailing == SwitchToggle {
    /// A row whose control is a switch named by the row's title.
    init(_ title: String, detail: String? = nil, symbol: String? = nil, isOn: Binding<Bool>) {
        self.init(title, detail: detail, symbol: symbol) { SwitchToggle(title, isOn: isOn) }
    }
}

// MARK: - Existing settings rows (Config Studio and the moved Settings content use these; new work uses FormRow)

/// Settings panels' corner radius, which rows also keep as their side margin, so no control
/// reaches into a corner.
enum SettingsMetrics {
    static let radius: CGFloat = 20
}

struct SettingsSection<Content: View, Footer: View>: View {
    var title: String
    @ViewBuilder var content: Content
    @ViewBuilder var footer: Footer
    @Environment(\.palette) private var palette
    init(_ title: String, @ViewBuilder content: () -> Content, @ViewBuilder footer: () -> Footer) {
        self.title = title; self.content = content(); self.footer = footer()
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.secondary).padding(.leading, 6).accessibilityAddTraits(.isHeader)
            content
            footer.font(.system(size: 12)).foregroundStyle(palette.tertiary).padding(.leading, 6)
        }
    }
}
extension SettingsSection where Footer == EmptyView {
    init(_ title: String, @ViewBuilder content: () -> Content) { self.init(title, content: content, footer: { EmptyView() }) }
}

/// Grouped settings rows on an opaque panel, separated by hairlines.
struct SettingsGroup<Content: View>: View {
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    var body: some View {
        // Every row draws a hairline above itself; shifting up one point hides the first under the clip.
        VStack(spacing: 0) { content }
        .padding(.top, -1)
        .background(palette.panel, in: RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous).stroke(palette.panelStroke, lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: SettingsMetrics.radius, style: .continuous))
        .shapeAudit(.container, .rounded(SettingsMetrics.radius), "settings group")
    }
}

private struct Hairline: ViewModifier {
    @Environment(\.palette) private var palette
    func body(content: Content) -> some View { content.overlay(alignment: .top) { Rectangle().fill(palette.separator).frame(height: 1) } }
}

struct SettingsRow<Control: View>: View {
    var title: String
    var detail: String?
    var fixedLabel: CGFloat?
    var typewriter = false
    var minHeight: CGFloat = 52
    /// Long titles that name where something goes wrap rather than truncate in a narrow window.
    var titleLines = 1
    @ViewBuilder var control: Control
    @Environment(\.palette) private var palette
    init(_ title: String, detail: String? = nil, fixedLabel: CGFloat? = nil, typewriter: Bool = false, minHeight: CGFloat = 52,
         titleLines: Int = 1, @ViewBuilder control: () -> Control) {
        self.title = title; self.detail = detail; self.fixedLabel = fixedLabel; self.typewriter = typewriter
        self.minHeight = minHeight; self.titleLines = titleLines; self.control = control()
    }
    var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(title).font(typewriter ? .typewriter(14) : .system(size: 14)).foregroundStyle(palette.dark ? Palette.hex(0xe6dfcf) : palette.text).lineLimit(titleLines)
                        .fixedSize(horizontal: false, vertical: titleLines > 1)

                }
                if let detail { Text(detail).font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true) }
            }
            .frame(width: fixedLabel, alignment: .leading)
            .frame(maxWidth: fixedLabel == nil ? .infinity : nil, alignment: .leading)
            control
        }
        .padding(.horizontal, SettingsMetrics.radius).padding(.vertical, detail == nil ? 10 : 9)
        .frame(minHeight: minHeight)
        .accessibilityElement(children: .contain)
        .modifier(Hairline())
    }
}

struct SettingsNote: View {
    var text: String
    @Environment(\.palette) private var palette
    var body: some View {
        Text(text).font(.system(size: 12)).foregroundStyle(palette.tertiary).frame(maxWidth: .infinity, minHeight: 44, alignment: .leading).padding(.horizontal, SettingsMetrics.radius)
            .modifier(Hairline())
    }
}

// MARK: - Sheets and alerts

/// One button in a sheet's action bar. The bar puts `.leading` actions on the left (after the note),
/// then the rest on the right in order: Cancel first, the default action last.
struct SheetAction {
    enum Role { case cancel, standard, primary, destructive, leading }
    var title: String
    var symbol: String? = nil
    var role: Role = .standard
    var enabled = true
    var action: () -> Void
    static func cancel(_ title: String = "Cancel", _ action: @escaping () -> Void) -> SheetAction { SheetAction(title: title, role: .cancel, action: action) }
    static func primary(_ title: String, symbol: String? = nil, enabled: Bool = true, _ action: @escaping () -> Void) -> SheetAction {
        SheetAction(title: title, symbol: symbol, role: .primary, enabled: enabled, action: action)
    }
}

/// The leading mark of a sheet: an SF Symbol on a 44-point tile (accent, or negative for a failure),
/// or the app's mark for update sheets.
enum SheetIcon {
    case symbol(String, negative: Bool = false)
    case mark
}

/// Every app sheet's anatomy (AllSheets board): symbol tile, a title that names the thing, one sentence
/// of consequence, the content, then the action bar with an optional note on the left, Cancel, and the
/// default action last. Escape presses Cancel (or the only action); Return presses the default action.
/// Content inside uses the inset form context (wells, rows 16 in).
struct SheetScaffold<Content: View, Note: View>: View {
    var icon: SheetIcon
    var title: String
    var message: String?
    var width: CGFloat = 520
    var actions: [SheetAction]
    @ViewBuilder var content: Content
    @ViewBuilder var note: Note
    @Environment(\.palette) private var palette
    init(icon: SheetIcon, title: String, message: String? = nil, width: CGFloat = 520, actions: [SheetAction],
         @ViewBuilder content: () -> Content, @ViewBuilder note: () -> Note) {
        self.icon = icon; self.title = title; self.message = message; self.width = width; self.actions = actions
        self.content = content(); self.note = note()
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(alignment: .top, spacing: 14) {
                iconView
                VStack(alignment: .leading, spacing: 4) {
                    Text(title).font(.system(size: 17, weight: .semibold)).foregroundStyle(palette.heading)
                        .fixedSize(horizontal: false, vertical: true).accessibilityAddTraits(.isHeader)
                    if let message {
                        Text(message).font(.system(size: 13)).foregroundStyle(palette.secondary).lineSpacing(2)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
                Spacer(minLength: 0)
            }
            content
            actionBar
        }
        .padding(22)
        .frame(width: width, alignment: .leading)
        .environment(\.formContext, .inset)
    }
    @ViewBuilder private var iconView: some View {
        switch icon {
        case .symbol(let name, let negative):
            Image(systemName: name).font(.system(size: 20, weight: .medium)).foregroundStyle(negative ? palette.negative : palette.accent)
                .frame(width: 44, height: 44)
                .background(palette.selection, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .accessibilityHidden(true)
        case .mark:
            BrandMark().frame(width: 48, height: 48)
        }
    }
    private var actionBar: some View {
        let hasCancel = actions.contains { $0.role == .cancel }
        let lone = actions.count == 1
        // Without a Cancel, Escape presses the default (Done) action.
        let escape = hasCancel ? nil : (actions.first { $0.role == .primary } ?? (lone ? actions.first : nil))
        return HStack(spacing: 8) {
            note.font(.system(size: 12)).foregroundStyle(palette.tertiary).fixedSize(horizontal: false, vertical: true)
            ForEach(Array(actions.enumerated()).filter { $0.element.role == .leading }, id: \.offset) { _, action in button(action) }
            Spacer(minLength: 8)
            ForEach(Array(actions.enumerated()).filter { $0.element.role != .leading }, id: \.offset) { _, action in button(action) }
        }
        .padding(.top, 2)
        .background {
            if let escape { Button("", action: escape.action).keyboardShortcut(.cancelAction).opacity(0).accessibilityHidden(true) }
        }
    }
    @ViewBuilder private func button(_ action: SheetAction) -> some View {
        let label = Group {
            if let symbol = action.symbol { Label(action.title, systemImage: symbol) } else { Text(action.title) }
        }
        switch action.role {
        case .primary:
            Button(action: action.action) { label }.prominentAction().actionControlSize().disabled(!action.enabled)
                .keyboardShortcut(.defaultAction)
        case .cancel:
            Button(action: action.action) { label }.glassAction().actionControlSize().disabled(!action.enabled).keyboardShortcut(.cancelAction)
        case .destructive:
            Button(action: action.action) { label }.destructiveAction().actionControlSize().disabled(!action.enabled)
        case .standard, .leading:
            Button(action: action.action) { label }.glassAction().actionControlSize().disabled(!action.enabled)
        }
    }
}
extension SheetScaffold where Note == EmptyView {
    init(icon: SheetIcon, title: String, message: String? = nil, width: CGFloat = 520, actions: [SheetAction], @ViewBuilder content: () -> Content) {
        self.init(icon: icon, title: title, message: message, width: width, actions: actions, content: content, note: { EmptyView() })
    }
}

/// A single decision (AllSheets › Alerts): the title asks the question, one sentence says what
/// happens, the action button names the action (never “OK”) and Cancel always exists.
struct ConfirmationRequest: Identifiable {
    let id = UUID()
    var title: String
    var message: String
    var actionTitle: String
    /// Removing or discarding something: the button is marked destructive.
    var destructive = true
    /// Extra choices between the action and Cancel (“Quit Both”).
    var others: [(title: String, action: @MainActor () -> Void)] = []
    var action: @MainActor () -> Void
}

extension View {
    /// Presents `request` as a native alert attached to this view's window.
    func confirmationAlert(_ request: Binding<ConfirmationRequest?>) -> some View {
        alert(request.wrappedValue?.title ?? "", isPresented: Binding(get: { request.wrappedValue != nil }, set: { if !$0 { request.wrappedValue = nil } }),
              presenting: request.wrappedValue) { value in
            Button(value.actionTitle, role: value.destructive ? .destructive : nil) { value.action() }
                .keyboardShortcut(value.destructive ? nil : .defaultAction)
            ForEach(Array(value.others.enumerated()), id: \.offset) { _, other in Button(other.title) { other.action() } }
            Button("Cancel", role: .cancel) {}
        } message: { value in Text(value.message) }
    }
}

// MARK: - Empty, loading and failed

/// Says what would be here and what makes it appear: a stencil heading, one sentence, one button.
struct EmptyState: View {
    var heading: String
    var message: String
    var actionTitle: String?
    var prominent = true
    var action: (() -> Void)?
    @Environment(\.palette) private var palette
    init(_ heading: String, message: String, actionTitle: String? = nil, prominent: Bool = true, action: (() -> Void)? = nil) {
        self.heading = heading; self.message = message; self.actionTitle = actionTitle; self.prominent = prominent; self.action = action
    }
    var body: some View {
        VStack(spacing: 10) {
            Text(heading.uppercased()).font(.stencil(26)).tracking(3.1).foregroundStyle(palette.chipIcon).multilineTextAlignment(.center)
                .accessibilityAddTraits(.isHeader)
            Text(message).font(.system(size: 13)).foregroundStyle(palette.secondary).multilineTextAlignment(.center)
                .frame(maxWidth: 420).fixedSize(horizontal: false, vertical: true)
            if let actionTitle, let action {
                if prominent { Button(actionTitle, action: action).prominentAction().actionControlSize().padding(.top, 4) }
                else { Button(actionTitle, action: action).glassAction().actionControlSize().padding(.top, 4) }
            }
        }
        .frame(maxWidth: .infinity)
        .accessibilityElement(children: .contain)
    }
}

enum BannerTone { case neutral, tip, error }

/// Calm, inline, never modal: neutral information, a soft-accent tip, or an error beside the thing
/// that failed, with an optional trailing button that offers the fix.
struct Banner<Trailing: View>: View {
    var tone: BannerTone
    var symbol: String?
    var title: String?
    var message: String
    @ViewBuilder var trailing: Trailing
    @Environment(\.palette) private var palette
    init(_ tone: BannerTone = .neutral, symbol: String? = nil, title: String? = nil, message: String, @ViewBuilder trailing: () -> Trailing) {
        self.tone = tone; self.symbol = symbol; self.title = title; self.message = message; self.trailing = trailing()
    }
    var body: some View {
        let shape = RoundedRectangle(cornerRadius: 14, style: .continuous)
        HStack(alignment: title == nil ? .firstTextBaseline : .center, spacing: 10) {
            Image(systemName: symbol ?? (tone == .error ? "exclamationmark.triangle" : "info.circle"))
                .font(.system(size: 14, weight: .medium)).foregroundStyle(tone == .error ? palette.negative : tone == .tip ? palette.accent : palette.secondary)
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                if let title { Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(palette.text) }
                Text(message).font(.system(size: 13)).foregroundStyle(title == nil ? palette.text : palette.secondary).lineSpacing(1.5)
            }
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityElement(children: .combine)
            .accessibilityLabel((tone == .error ? "Problem: " : "") + [title, message].compactMap { $0 }.joined(separator: ". "))
            .accessibilityAddTraits(.isStaticText)
            trailing
        }
        .padding(.vertical, 12).padding(.horizontal, 14)
        .background(tone == .tip ? palette.selection : palette.well, in: shape)
        .overlay(shape.strokeBorder(tone == .tip ? palette.selectionStroke : tone == .error ? palette.negative.opacity(0.55) : palette.wellStroke, lineWidth: 1))
        .accessibilityElement(children: .contain)
    }
}
extension Banner where Trailing == EmptyView {
    init(_ tone: BannerTone = .neutral, symbol: String? = nil, title: String? = nil, message: String) {
        self.init(tone, symbol: symbol, title: title, message: message, trailing: { EmptyView() })
    }
}

/// For things that happened elsewhere (the game closed): glass, at the bottom of the window, with
/// its buttons in the bottom-trailing corner so they stay concentric however many lines wrap.
struct Toast<Actions: View>: View {
    var symbol: String
    var tone: BannerTone
    var message: String
    @ViewBuilder var actions: Actions
    @Environment(\.palette) private var palette
    /// The message's height with its padding (ship-home-library: see the minimum height below).
    @State private var textHeight: CGFloat = 0
    init(symbol: String = "info.circle", tone: BannerTone = .neutral, message: String, @ViewBuilder actions: () -> Actions) {
        self.symbol = symbol; self.tone = tone; self.message = message; self.actions = actions()
    }
    var body: some View {
        // 48 tall on one line: a capsule around 36-point buttons set 6 points in.
        GlassContainer {
            HStack(alignment: .bottom, spacing: 12) {
                HStack(alignment: .firstTextBaseline, spacing: 10) {
                    Image(systemName: symbol).foregroundStyle(tone == .error ? palette.negative : palette.accent).accessibilityHidden(true)
                    Text(message).font(.system(size: 13)).foregroundStyle(palette.text).textSelection(.enabled)
                        .fixedSize(horizontal: false, vertical: true).frame(maxWidth: .infinity, alignment: .leading)
                        .id(message)
                }
                .padding(.vertical, 9)
                .onGeometryChange(for: CGFloat.self, of: { $0.size.height }) { textHeight = $0 }
                HStack(spacing: 8) { actions }
            }
            .padding(.leading, 18).padding(.trailing, 6).padding(.vertical, 6)
            // ship-home-library: a wrapped message makes the toast at least 66 tall, so the buttons sit
            // clear of the top-trailing corner and stay concentric with the bottom-trailing one alone
            // (at 62 they were 20 from the top and 6 from the side of a 24-point corner).
            .frame(minHeight: textHeight > ControlMetrics.action + 0.5 ? 66 : 48, alignment: .bottom)
            .frame(maxWidth: 720)
            .glassSurface(RoundedRectangle(cornerRadius: 24, style: .continuous))
        }
    }
}

// MARK: - Status, badges, chips, keys, ping, progress

/// Status is a symbol and words, never colour alone (Foundations: ready, needs a change, won't work, not measured).
enum StatusKind {
    case ready, needsChange, wontWork, notMeasured
    var symbol: String {
        switch self { case .ready: "checkmark.circle"; case .needsChange: "exclamationmark.triangle"; case .wontWork: "xmark.circle"; case .notMeasured: "minus.circle" }
    }
    var words: String {
        switch self { case .ready: "Ready"; case .needsChange: "Needs a change"; case .wontWork: "Won’t work"; case .notMeasured: "Not measured" }
    }
    func color(_ palette: Palette) -> Color {
        switch self { case .ready: palette.positive; case .needsChange: palette.caution; case .wontWork: palette.negative; case .notMeasured: palette.secondary }
    }
}

struct StatusLabel: View {
    var kind: StatusKind
    var text: String?
    @Environment(\.palette) private var palette
    init(_ kind: StatusKind, _ text: String? = nil) { self.kind = kind; self.text = text }
    var body: some View {
        Label { Text(text ?? kind.words).lineLimit(1) } icon: { Image(systemName: kind.symbol).font(.system(size: 13, weight: .semibold)) }
            .labelStyle(StatusLabelStyle())
            .font(.system(size: 12, weight: .semibold)).foregroundStyle(kind.color(palette))
            .fixedSize()
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(text ?? kind.words)
            .accessibilityAddTraits(.isStaticText)
    }
}
private struct StatusLabelStyle: LabelStyle {
    func makeBody(configuration: Configuration) -> some View { HStack(spacing: 6) { configuration.icon; configuration.title } }
}

enum BadgeTone { case neutral, accent, negative }

/// 18 points tall, a 6-point corner and a hairline: game versions, mods, rule limits.
struct Badge: View {
    var text: String
    var tone: BadgeTone
    @Environment(\.palette) private var palette
    init(_ text: String, tone: BadgeTone = .neutral) { self.text = text; self.tone = tone }
    var body: some View {
        let color = tone == .accent ? palette.accent : tone == .negative ? palette.negative : (palette.dark ? Palette.hex(0xbdb5a1) : palette.secondary)
        Text(text).font(.system(size: 10.5, weight: .semibold)).lineLimit(1).fixedSize()
            .foregroundStyle(color)
            .padding(.horizontal, 7).frame(height: 18)
            .overlay(RoundedRectangle(cornerRadius: 6, style: .continuous).strokeBorder(tone == .accent ? palette.selectionStroke : tone == .negative ? palette.negative : palette.track, lineWidth: 1))
    }
}

/// A capsule that states a fact and, with an action, opens where it can be changed. Regular chips are
/// 26 points; large ones (40) sit under the Home hero on glass.
struct Chip<Content: View>: View {
    enum Size { case regular, large }
    var size: Size = .regular
    var glass = false
    var label: String
    var action: (() -> Void)?
    @ViewBuilder var content: Content
    @Environment(\.palette) private var palette
    init(_ label: String, size: Size = .regular, glass: Bool = false, action: (() -> Void)? = nil, @ViewBuilder content: () -> Content) {
        self.label = label; self.size = size; self.glass = glass; self.action = action; self.content = content()
    }
    var body: some View {
        if let action {
            Button(action: action) { face.contentShape(Capsule()) }.buttonStyle(.plain).accessibilityLabel(label)
        } else {
            face.accessibilityElement(children: .ignore).accessibilityLabel(label).accessibilityAddTraits(.isStaticText)
        }
    }
    @ViewBuilder private var face: some View {
        let row = HStack(spacing: size == .large ? 10 : 8) { content }
            .font(.system(size: size == .large ? 13 : 12, weight: size == .large ? .regular : .medium)).foregroundStyle(palette.chipText)
            .lineLimit(1)
            .padding(.horizontal, size == .large ? 18 : 12).frame(height: size == .large ? 40 : 26)
        if glass {
            row.glassSurface(Capsule(), interactive: action != nil)
        } else {
            row.background(palette.button, in: Capsule()).overlay(Capsule().strokeBorder(palette.buttonStroke, lineWidth: 1))
                .shapeAudit(action != nil ? .control : .container, .capsule, "chip")
        }
    }
}

/// One key of a shortcut, as printed on the key: “⌘”, “K”, “Space”. VoiceOver reads its name.
struct KeyCap: View {
    var key: String
    @Environment(\.palette) private var palette
    init(_ key: String) { self.key = key }
    var body: some View {
        Text(key).font(.system(size: 12, weight: .semibold)).foregroundStyle(palette.chipText)
            .padding(.horizontal, 6).frame(minWidth: 22, minHeight: 22)
            .background(palette.button, in: RoundedRectangle(cornerRadius: 6, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 6, style: .continuous).strokeBorder(palette.buttonStroke, lineWidth: 1))
            .accessibilityLabel(Self.spoken(key))
    }
    static func spoken(_ key: String) -> String {
        ["⌘": "Command", "⇧": "Shift", "⌥": "Option", "⌃": "Control", "↩": "Return", "⌫": "Delete", "⎋": "Escape", ",": "Comma", "?": "Question mark"][key] ?? key
    }
}

/// A whole shortcut as key caps, read as one phrase (“Command K”).
struct ShortcutKeys: View {
    var keys: [String]
    init(_ keys: [String]) { self.keys = keys }
    var body: some View {
        HStack(spacing: 3) { ForEach(Array(keys.enumerated()), id: \.offset) { _, key in KeyCap(key) } }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(keys.map(KeyCap.spoken).joined(separator: " "))
            .accessibilityAddTraits(.isStaticText)
    }
}

/// Four signal bars for a server's ping (3 points wide, 4–12 tall): green from three bars,
/// caution below. Decorative: the row says the number in milliseconds.
struct PingBars: View {
    var level: Int
    @Environment(\.palette) private var palette
    var body: some View {
        HStack(alignment: .bottom, spacing: 2) {
            ForEach(1...4, id: \.self) { bar in
                RoundedRectangle(cornerRadius: 1).fill(bar <= level ? (level >= 3 ? palette.positive : palette.caution) : palette.track)
                    .frame(width: 3, height: [4, 7, 10, 12][bar - 1])
            }
        }
        .frame(height: 12, alignment: .bottom)
        .accessibilityHidden(true)
    }
}

/// An 8-point bar for work with a known size, or a moving segment when there is no count yet
/// (still with Reduce Motion). VoiceOver reads it as the native progress indicator it represents.
struct ProgressBar: View {
    var value: Double?
    var label: String
    @Environment(\.palette) private var palette
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var phase = false
    init(_ label: String, value: Double?) { self.label = label; self.value = value }
    var body: some View {
        GeometryReader { proxy in
            let fill = LinearGradient(colors: [palette.prominentBottom, palette.prominentTop], startPoint: .leading, endPoint: .trailing)
            ZStack(alignment: .leading) {
                Capsule().fill(palette.track)
                if let value {
                    Capsule().fill(fill).frame(width: max(8, proxy.size.width * min(1, max(0, value))))
                } else {
                    Capsule().fill(fill).frame(width: proxy.size.width * 0.3)
                        .offset(x: reduceMotion ? proxy.size.width * 0.35 : (phase ? proxy.size.width * 0.7 : 0))
                        .animation(reduceMotion ? nil : .easeInOut(duration: 0.9).repeatForever(autoreverses: true), value: phase)
                        .onAppear { phase = true }
                }
            }
            .clipShape(Capsule())
        }
        .frame(height: 8)
        .accessibilityRepresentation {
            if let value { ProgressView(label, value: value) } else { ProgressView(label) }
        }
    }
}

// MARK: - Controls: toolbar pill, scope switch, search, symbol button, pop-up, fields, checkbox, switch

/// Glass capsule (44 tall) that groups circular symbol buttons and short controls over content.
struct ToolbarPill<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        GlassContainer(spacing: 2) {
            // Buttons are 36 tall, 4 points from every edge, so they're concentric with the pill.
            HStack(spacing: 2) { content }
                .actionControlSize()
                .padding(.horizontal, 4).frame(height: 44)
                .glassSurface(Capsule())
        }
    }
}

/// “All · Favorites · Recent”: the native segmented control (arrow keys, radio-group semantics),
/// as a capsule, labelled for VoiceOver.
struct ScopeSwitch<Value: Hashable>: View {
    var label: String
    @Binding var selection: Value
    var options: [(value: Value, title: String)]
    init(_ label: String, selection: Binding<Value>, options: [(value: Value, title: String)]) {
        self.label = label; _selection = selection; self.options = options
    }
    var body: some View {
        Picker(label, selection: $selection) {
            ForEach(Array(options.enumerated()), id: \.offset) { _, option in Text(option.title).tag(option.value) }
        }
        .pickerStyle(.segmented).labelsHidden().fixedSize().capsuleControls()
        .accessibilityLabel(label)
    }
}

/// The search capsule: a magnifier, the field, and a Clear button once something is typed. Pass a
/// focus binding so ⌘F (the menu's Find) can focus it.
struct SearchPill: View {
    var prompt: String
    @Binding var text: String
    var focus: FocusState<Bool>.Binding
    var width: CGFloat? = 240
    /// The VoiceOver name when it should be shorter than the prompt (“Search players”).
    var label: String?
    @Environment(\.palette) private var palette
    init(_ prompt: String, text: Binding<String>, focus: FocusState<Bool>.Binding, width: CGFloat? = 240, label: String? = nil) {
        self.prompt = prompt; _text = text; self.focus = focus; self.width = width; self.label = label
    }
    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass").font(.system(size: 13)).foregroundStyle(palette.tertiary).accessibilityHidden(true)
            TextField(prompt, text: $text, prompt: Text(prompt).foregroundStyle(palette.tertiary))
                .textFieldStyle(.plain).font(.system(size: 13)).foregroundStyle(palette.text)
                .focused(focus)
                .onExitCommand { text = ""; focus.wrappedValue = false }
                .accessibilityLabel(label ?? prompt)
            if !text.isEmpty {
                Button { text = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(palette.tertiary) }
                    .buttonStyle(.plain).accessibilityLabel("Clear search").help("Clear search")
            }
        }
        .padding(.horizontal, 14).frame(width: width, height: 36)
        .background(palette.field, in: Capsule())
        .overlay(Capsule().strokeBorder(focus.wrappedValue ? palette.accent.opacity(0.75) : palette.wellStroke, lineWidth: focus.wrappedValue ? 2 : 1))
        .shapeAudit(.field, .capsule, "search")
    }
}

/// A lone symbol is a circle, and only a button when it has a name for VoiceOver and a tooltip.
struct SymbolButton: View {
    var symbol: String
    var label: String
    var help: String?
    var action: () -> Void
    init(_ symbol: String, label: String, help: String? = nil, action: @escaping () -> Void) {
        self.symbol = symbol; self.label = label; self.help = help; self.action = action
    }
    var body: some View {
        Button(action: action) { Image(systemName: symbol).frame(width: 16, height: 16) }
            .glassAction(circle: true)
            .accessibilityLabel(label)
            .help(help ?? label)
    }
}

/// The native pop-up menu as a capsule, named for VoiceOver.
struct PopUpButton<Value: Hashable>: View {
    var label: String
    @Binding var selection: Value
    var options: [(value: Value, title: String)]
    init(_ label: String, selection: Binding<Value>, options: [(value: Value, title: String)]) {
        self.label = label; _selection = selection; self.options = options
    }
    var body: some View {
        Picker(label, selection: $selection) {
            ForEach(Array(options.enumerated()), id: \.offset) { _, option in Text(option.title).tag(option.value) }
        }
        .pickerStyle(.menu).labelsHidden().fixedSize().capsuleControls()
        .accessibilityLabel(label)
    }
}

/// Text fields: the capsule (36, for an address or a password on its own line) and the rounded field
/// (28, radius 7, inside rows). An invalid value draws the negative edge; the message sits beside it.
struct LauncherTextField: View {
    enum Shape { case capsule, rounded }
    var label: String
    var prompt: String?
    @Binding var text: String
    var shape: Shape = .rounded
    var secure = false
    var monospaced = false
    var invalid = false
    var width: CGFloat? = nil
    /// Takes the keyboard focus when it appears (the one field of a sheet).
    var autofocus = false
    @Environment(\.palette) private var palette
    @FocusState private var focused: Bool
    init(_ label: String, text: Binding<String>, prompt: String? = nil, shape: Shape = .rounded, secure: Bool = false, monospaced: Bool = false,
         invalid: Bool = false, width: CGFloat? = nil, autofocus: Bool = false) {
        self.label = label; _text = text; self.prompt = prompt; self.shape = shape; self.secure = secure
        self.monospaced = monospaced; self.invalid = invalid; self.width = width; self.autofocus = autofocus
    }
    var body: some View {
        let height: CGFloat = shape == .capsule ? 36 : 28
        let radius: CGFloat = shape == .capsule ? height / 2 : 7
        let outline = RoundedRectangle(cornerRadius: radius, style: .continuous)
        Group {
            if secure { SecureField(label, text: $text, prompt: prompt.map { Text($0).foregroundStyle(palette.tertiary) }) }
            else { TextField(label, text: $text, prompt: prompt.map { Text($0).foregroundStyle(palette.tertiary) }) }
        }
        .textFieldStyle(.plain)
        .font(monospaced ? .typewriter(shape == .capsule ? 15 : 13, bold: shape == .capsule) : .system(size: 13))
        .foregroundStyle(palette.text)
        .focused($focused)
        .padding(.horizontal, shape == .capsule ? 16 : 12)
        .frame(width: width, height: height)
        .background(palette.field, in: outline)
        .overlay(outline.strokeBorder(invalid ? palette.negative : focused ? palette.accent.opacity(0.75) : palette.wellStroke, lineWidth: focused || invalid ? 2 : 1))
        .shapeAudit(.field, shape == .capsule ? .capsule : .rounded(radius), "text field")
        .accessibilityLabel(label)
        .onAppear { if autofocus { focused = true } }
    }
}

/// The native checkbox, in the control colour.
struct Checkbox: View {
    var title: String
    @Binding var isOn: Bool
    @Environment(\.palette) private var palette
    init(_ title: String, isOn: Binding<Bool>) { self.title = title; _isOn = isOn }
    var body: some View { Toggle(title, isOn: $isOn).toggleStyle(.checkbox).tint(palette.control).font(.system(size: 13)) }
}

/// The native switch, in the control colour, named by its row (the label is hidden on screen).
struct SwitchToggle: View {
    var label: String
    @Binding var isOn: Bool
    @Environment(\.palette) private var palette
    init(_ label: String, isOn: Binding<Bool>) { self.label = label; _isOn = isOn }
    var body: some View { Toggle(label, isOn: $isOn).toggleStyle(.switch).labelsHidden().tint(palette.control) }
}

// MARK: - Map art

/// Every place that draws a map's loading screen goes through this frame (Foundations: “Map art is never
/// stretched”). Loading screens are already given their 4:3 shape when decoded (MapArtworkStore); this
/// fills the frame from that shape and keeps the centre. Rows use 80 × 60 and cards 64 × 48, exactly
/// 4:3 so nothing is cropped; `.fill` covers whatever frame the caller gives it (hero, details panel).
struct MapArtFrame: View {
    enum Size {
        /// Server and match rows: 80 × 60, radius 10.
        case row
        /// Home cards and compact rows: 64 × 48, radius 9.
        case card
        /// Fill the caller's frame from the centre.
        case fill
        var size: CGSize? {
            switch self { case .row: CGSize(width: 80, height: 60); case .card: CGSize(width: 64, height: 48); case .fill: nil }
        }
        var radius: CGFloat { switch self { case .row: 10; case .card: 9; case .fill: 0 } }
    }
    var map: String
    var image: NSImage?
    var size: Size
    /// Corner radius; pass the concentric value when the art sits inside a rounded container.
    var radius: CGFloat?
    /// The large fallback scene (Home hero, a player's page) instead of the thumbnail drawing.
    var hero = false
    init(map: String, image: NSImage?, size: Size, radius: CGFloat? = nil, hero: Bool = false) {
        self.map = map; self.image = image; self.size = size; self.radius = radius; self.hero = hero
    }
    var body: some View {
        let corner = radius ?? size.radius
        let art = MapArt(map: map, image: image, hero: hero)
            .frame(width: size.size?.width, height: size.size?.height)
        if corner > 0 {
            art.clipShape(RoundedRectangle(cornerRadius: corner, style: .continuous)).shapeAudit(.inset, .rounded(corner), "row art")
        } else {
            art.clipped()
        }
    }
}
