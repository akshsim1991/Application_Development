import Foundation

/// "Ask anything" voice answers from Claude. Needs your own Anthropic API key (Settings).
enum Ask {
    static func answer(_ question: String) async -> String? {
        let key = Prefs.anthropicKey
        guard !key.isEmpty, let url = URL(string: "https://api.anthropic.com/v1/messages") else { return nil }
        let system = "You are RideVoice, a voice assistant for a motorcycle rider using a helmet headset. "
            + "Answer in at most two short spoken sentences. Plain text only: no markdown, lists, or emojis. "
            + "If a question needs reading a screen or long instructions, say to ask again when stopped."
        let body: [String: Any] = [
            "model": "claude-haiku-5-5",
            "max_tokens": 220,
            "thinking": ["type": "disabled"],
            "output_config": ["effort": "low"],
            "system": system,
            "messages": [["role": "user", "content": question]]
        ]
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.timeoutInterval = 20
        req.setValue("application/json", forHTTPHeaderField: "content-type")
        req.setValue(key, forHTTPHeaderField: "x-api-key")
        req.setValue("2023-06-01", forHTTPHeaderField: "anthropic-version")
        req.httpBody = try? JSONSerialization.data(withJSONObject: body)
        guard let (data, resp) = try? await URLSession.shared.data(for: req),
              (resp as? HTTPURLResponse)?.statusCode == 200,
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let content = json["content"] as? [[String: Any]] else { return nil }
        let text = content.compactMap { $0["type"] as? String == "text" ? $0["text"] as? String : nil }
            .joined(separator: " ").trimmingCharacters(in: .whitespacesAndNewlines)
        return text.isEmpty ? nil : text
    }
}
