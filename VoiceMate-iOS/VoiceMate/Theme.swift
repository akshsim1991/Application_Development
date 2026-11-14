import SwiftUI

enum Theme {
    static let bg = Color(red: 0.04, green: 0.04, blue: 0.06)
    static let card = Color(red: 0.10, green: 0.10, blue: 0.13)
    static let accent = Color(red: 1.0, green: 0.70, blue: 0.0)
    static let good = Color(red: 0.20, green: 0.85, blue: 0.45)
    static let bad = Color(red: 1.0, green: 0.30, blue: 0.30)
    static let dim = Color.white.opacity(0.55)
}

struct CardStyle: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 22, style: .continuous).fill(Theme.card))
    }
}

extension View { func card() -> some View { modifier(CardStyle()) } }

struct StatTile: View {
    let title: String
    let value: String
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title.uppercased()).font(.caption2.weight(.semibold)).foregroundStyle(Theme.dim)
            Text(value).font(.system(size: 26, weight: .bold, design: .rounded)).monospacedDigit()
                .minimumScaleFactor(0.6).lineLimit(1)
        }
        .card()
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
