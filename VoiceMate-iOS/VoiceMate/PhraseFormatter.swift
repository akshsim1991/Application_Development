import Foundation

struct SpeechChunk: Equatable {
    var text: String
    /// Silence after this chunk, as a multiple of the user's "pause length" setting.
    var pauseFactor: Double
}

struct Pronunciation: Codable, Identifiable, Equatable {
    var id = UUID()
    /// What appears in the instruction (e.g. "Koramangala").
    var written: String
    /// How it should be spoken, respelled (e.g. "Ko-ra-mang-ala").
    var spoken: String
}

/// Turns instruction text into natural speech: pronunciation overrides, abbreviations, number words and
/// short phrases with pauses. Pure functions (no UI or audio) so it can be unit tested.
enum PhraseFormatter {

    // MARK: Pipeline

    static func prepare(_ text: String, overrides: [Pronunciation] = []) -> [SpeechChunk] {
        var t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return [] }
        for o in overrides where !o.written.trimmingCharacters(in: .whitespaces).isEmpty {
            t = replaceWord(in: t, word: o.written, with: o.spoken, caseInsensitive: true)
        }
        for (abbr, full) in abbreviations { t = replaceWord(in: t, word: abbr, with: full, caseInsensitive: false) }
        return chunk(t)
    }

    /// Case-sensitive for abbreviations (so "NH" never matches a normal word), optional for user words.
    static func replaceWord(in text: String, word: String, with replacement: String, caseInsensitive: Bool) -> String {
        let pattern = "(?<![\\p{L}\\p{N}])" + NSRegularExpression.escapedPattern(for: word) + "(?![\\p{L}\\p{N}])"
        guard let re = try? NSRegularExpression(pattern: pattern, options: caseInsensitive ? [.caseInsensitive] : []) else { return text }
        let template = NSRegularExpression.escapedTemplate(for: replacement)
        return re.stringByReplacingMatches(in: text, range: NSRange(text.startIndex..., in: text), withTemplate: template)
    }

    /// Common road abbreviations in Indian navigation text.
    static let abbreviations: [(String, String)] = [
        ("NH", "National Highway"), ("SH", "State Highway"), ("ORR", "Outer Ring Road"),
        ("Rd.", "Road"), ("Rd", "Road"), ("Jn.", "Junction"), ("Jn", "Junction"),
        ("Hwy", "Highway"), ("Expy", "Expressway"), ("Ave", "Avenue"),
        ("km/h", "kilometres per hour"), ("mph", "miles per hour")
    ]

    /// Splits into short phrases at sentence ends and (for long sentences) at commas.
    static func chunk(_ text: String) -> [SpeechChunk] {
        var out: [SpeechChunk] = []
        // Split only at real sentence ends (punctuation followed by space or the end), so "3.5" stays whole.
        let marked = text.replacingOccurrences(of: "\n", with: " ")
            .replacingOccurrences(of: "([.!?])(\\s+|$)", with: "$1\u{1F}", options: .regularExpression)
        let sentences = marked.components(separatedBy: "\u{1F}")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .map { $0.hasSuffix(".") ? String($0.dropLast()) : $0 }
            .filter { !$0.isEmpty }
        for s in sentences {
            if s.count > 90, s.contains(",") {
                let parts = s.components(separatedBy: ",").map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
                for (i, p) in parts.enumerated() { out.append(SpeechChunk(text: p, pauseFactor: i == parts.count - 1 ? 1 : 0.5)) }
            } else {
                out.append(SpeechChunk(text: s, pauseFactor: 1))
            }
        }
        if !out.isEmpty { out[out.count - 1].pauseFactor = 0 }
        return out
    }

    // MARK: Numbers and distances

    private static let ones = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
                               "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"]
    private static let tens = ["", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"]

    /// 1250 -> "one thousand two hundred fifty"
    static func words(_ n: Int) -> String {
        if n < 0 { return "minus " + words(-n) }
        if n < 20 { return ones[n] }
        if n < 100 { return tens[n / 10] + (n % 10 == 0 ? "" : " " + ones[n % 10]) }
        if n < 1000 { return ones[n / 100] + " hundred" + (n % 100 == 0 ? "" : " " + words(n % 100)) }
        if n < 100_000 { return words(n / 1000) + " thousand" + (n % 1000 == 0 ? "" : " " + words(n % 1000)) }
        return String(n)
    }

    static func ordinal(_ n: Int) -> String {
        let names = ["", "first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth", "ninth", "tenth"]
        return n >= 1 && n < names.count ? names[n] : "\(n)th"
    }

    /// "In about five hundred metres": rounded to a speakable figure, with "about" only when it was rounded.
    static func distance(_ m: Double, imperial: Bool) -> String {
        if imperial {
            let ft = m * 3.28084
            if ft < 1000 {
                let r = max(50, Int((ft / 50).rounded()) * 50)
                return (abs(Double(r) - ft) > 15 ? "about " : "") + words(r) + " feet"
            }
            return decimalUnit(m / 1609.34, one: "mile", many: "miles")
        }
        if m < 1000 {
            let r = max(50, Int((m / 50).rounded()) * 50)
            return (abs(Double(r) - m) > 10 ? "about " : "") + words(r) + " metres"
        }
        return decimalUnit(m / 1000, one: "kilometre", many: "kilometres")
    }

    private static func decimalUnit(_ v: Double, one: String, many: String) -> String {
        if v < 10 {
            let r = (v * 10).rounded() / 10
            let whole = Int(r.rounded(.down))
            let tenth = Int(((r - Double(whole)) * 10).rounded())
            let prefix = abs(r - v) > 0.05 ? "about " : ""
            if tenth == 0 { return prefix + words(whole) + " " + (whole == 1 ? one : many) }
            return prefix + words(whole) + " point " + words(tenth) + " " + many
        }
        let r = Int(v.rounded())
        return (abs(Double(r) - v) > 0.3 ? "about " : "") + words(r) + " " + many
    }

    // MARK: Roundabouts

    /// "take the 2nd exit" / "take the second exit" -> 2
    static func exitNumber(from text: String) -> Int? {
        let t = text.lowercased()
        if let re = try? NSRegularExpression(pattern: "(\\d+)(?:st|nd|rd|th)\\s+exit"),
           let m = re.firstMatch(in: t, range: NSRange(t.startIndex..., in: t)),
           let r = Range(m.range(at: 1), in: t), let n = Int(t[r]) { return n }
        let names = ["first": 1, "second": 2, "third": 3, "fourth": 4, "fifth": 5, "sixth": 6, "seventh": 7, "eighth": 8]
        for (w, n) in names where t.contains("\(w) exit") { return n }
        return nil
    }
}
