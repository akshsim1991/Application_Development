import SwiftUI

enum Theme {
    static let bg = Color(red: 0.05, green: 0.05, blue: 0.11)
    static let card = Color.white.opacity(0.07)
    static let accent = Color(red: 1.0, green: 0.62, blue: 0.10)
    static let good = Color(red: 0.20, green: 0.90, blue: 0.55)
    static let bad = Color(red: 1.0, green: 0.32, blue: 0.38)
    static let cyan = Color(red: 0.20, green: 0.82, blue: 1.0)
    static let violet = Color(red: 0.56, green: 0.42, blue: 1.0)
    static let pink = Color(red: 1.0, green: 0.32, blue: 0.64)
    static let dim = Color.white.opacity(0.6)

    static let brand = LinearGradient(colors: [accent, pink], startPoint: .topLeading, endPoint: .bottomTrailing)
    static let greenGrad = LinearGradient(colors: [good, cyan], startPoint: .topLeading, endPoint: .bottomTrailing)
    static let redGrad = LinearGradient(colors: [bad, accent], startPoint: .topLeading, endPoint: .bottomTrailing)
}

/// Static gradient backdrop (cheap to draw, no blur).
struct AppBackground: View {
    var body: some View {
        ZStack {
            LinearGradient(colors: [Color(red: 0.04, green: 0.04, blue: 0.10),
                                    Color(red: 0.08, green: 0.05, blue: 0.19),
                                    Color(red: 0.15, green: 0.06, blue: 0.22)],
                           startPoint: .top, endPoint: .bottom)
            RadialGradient(colors: [Theme.violet.opacity(0.28), .clear], center: .topTrailing, startRadius: 0, endRadius: 420)
            RadialGradient(colors: [Theme.pink.opacity(0.20), .clear], center: .bottomLeading, startRadius: 0, endRadius: 460)
        }
        .ignoresSafeArea()
    }
}

/// Translucent card with a thin light edge.
struct GlassBackground: View {
    var radius: CGFloat = 22
    var tint: Color = .clear
    var body: some View {
        RoundedRectangle(cornerRadius: radius, style: .continuous)
            .fill(Theme.card)
            .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous).fill(tint.opacity(0.10)))
            .overlay(
                RoundedRectangle(cornerRadius: radius, style: .continuous)
                    .strokeBorder(LinearGradient(colors: [.white.opacity(0.20), .white.opacity(0.04)],
                                                 startPoint: .topLeading, endPoint: .bottomTrailing), lineWidth: 1)
            )
    }
}

struct CardStyle: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(GlassBackground())
    }
}

extension View { func card() -> some View { modifier(CardStyle()) } }

/// Buttons shrink slightly while pressed, which makes taps feel instant.
struct PressStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.95 : 1)
            .animation(.spring(response: 0.18, dampingFraction: 0.7), value: configuration.isPressed)
    }
}

func tapHaptic() { UIImpactFeedbackGenerator(style: .light).impactOccurred() }

/// List section with the glass row background.
struct GSection<Content: View>: View {
    let title: String?
    let content: () -> Content

    init(_ title: String? = nil, @ViewBuilder content: @escaping () -> Content) {
        self.title = title
        self.content = content
    }

    var body: some View {
        if let title {
            Section(title) { content() }.listRowBackground(Theme.card)
        } else {
            Section { content() }.listRowBackground(Theme.card)
        }
    }
}

struct StatTile: View {
    let title: String
    let value: String
    var icon: String? = nil
    var color: Color = Theme.accent

    var body: some View {
        HStack(spacing: 12) {
            if let icon {
                Image(systemName: icon)
                    .font(.headline)
                    .foregroundStyle(color)
                    .frame(width: 38, height: 38)
                    .background(Circle().fill(color.opacity(0.18)))
            }
            VStack(alignment: .leading, spacing: 2) {
                Text(title.uppercased()).font(.caption2.weight(.semibold)).foregroundStyle(Theme.dim)
                Text(value).font(.system(size: 23, weight: .bold, design: .rounded)).monospacedDigit()
                    .minimumScaleFactor(0.6).lineLimit(1)
            }
            Spacer(minLength: 0)
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(GlassBackground(radius: 18, tint: color))
    }
}

func clockString(_ s: TimeInterval) -> String {
    let t = Int(s)
    return String(format: "%d:%02d:%02d", t / 3600, t % 3600 / 60, t % 60)
}

func cardinal(_ course: Double) -> String {
    guard course >= 0 else { return "—" }
    let names = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    return names[Int(((course + 22.5) / 45).rounded(.down)) % 8]
}

func hideKeyboard() {
    UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
}

/// Lets you dismiss the keyboard (number pads have no return key) by tapping Done or dragging the list.
struct KeyboardDone: ViewModifier {
    func body(content: Content) -> some View {
        content
            .scrollDismissesKeyboard(.interactively)
            .toolbar {
                ToolbarItemGroup(placement: .keyboard) {
                    Spacer()
                    Button("Done") { hideKeyboard() }
                }
            }
    }
}

extension View { func keyboardDone() -> some View { modifier(KeyboardDone()) } }
