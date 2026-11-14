import SwiftUI
import AVFoundation

/// Pick a voice, a speaking style and fine-tune it, with real navigation sentences to preview.
struct VoiceGalleryView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var store = VoiceStore.shared
    @State private var voices: [VoiceInfo] = []
    @State private var filter = 0            // 0 = Indian languages, 1 = English, 2 = all
    @State private var sample = 0
    @State private var compareA: String?
    @State private var compareB: String?

    private var shown: [VoiceInfo] {
        switch filter {
        case 0: return voices.filter { $0.language.hasSuffix("-IN") }
        case 1: return voices.filter { $0.language.hasPrefix("en") }
        default: return voices
        }
    }

    private var sampleText: String { VoiceStore.samples[min(sample, VoiceStore.samples.count - 1)] }

    var body: some View {
        List {
            presetSection
            tuningSection
            sampleSection
            voiceSection
            compareSection
            moreSection
            GSection {
                NavigationLink { PronunciationView() } label: { Label("Pronunciation dictionary", systemImage: "character.book.closed.fill") }
            }
        }
        .scrollContentBackground(.hidden)
        .background(AppBackground())
        .navigationTitle("Voice gallery")
        .onAppear { voices = VoiceStore.catalogue() }
        .onDisappear { ride.stopPreview() }
    }

    // MARK: Sections

    private var presetSection: some View {
        GSection("Speaking style") {
            ForEach(VoiceStore.presets) { p in
                Button { tapHaptic(); store.apply(p) } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(p.name).font(.headline)
                            Text(p.blurb).font(.caption).foregroundStyle(Theme.dim)
                        }
                        Spacer()
                        if store.presetID == p.id { Image(systemName: "checkmark.circle.fill").foregroundStyle(Theme.good) }
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            Text("A style sets speed, pitch, pauses and how much I say. System voices can't express emotion, so these change the delivery, not the feeling.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }

    private var tuningSection: some View {
        GSection("Fine-tune") {
            VStack(alignment: .leading) {
                HStack { Text("Speaking speed"); Spacer(); Text(String(format: "%.2f", store.rate)).foregroundStyle(Theme.dim) }
                Slider(value: $store.rate, in: VoiceStore.rateRange)
            }
            VStack(alignment: .leading) {
                HStack { Text("Pitch"); Spacer(); Text(String(format: "%.2f", store.pitch)).foregroundStyle(Theme.dim) }
                Slider(value: $store.pitch, in: VoiceStore.pitchRange)
            }
            VStack(alignment: .leading) {
                HStack { Text("Pause between phrases"); Spacer(); Text(String(format: "%.2f s", store.pause)).foregroundStyle(Theme.dim) }
                Slider(value: $store.pause, in: VoiceStore.pauseRange)
            }
            Picker("How much I say", selection: $store.verbosity) {
                ForEach(Verbosity.allCases) { Text($0.title).tag($0) }
            }
            .pickerStyle(.segmented)
            Text("Concise gives the middle and final prompts only. Detailed adds an earlier heads-up. Ranges are limited so speech stays clear at helmet speeds; some voices ignore small pitch changes.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }

    private var sampleSection: some View {
        GSection("Preview sentence") {
            Picker("Sentence", selection: $sample) {
                ForEach(0..<VoiceStore.samples.count, id: \.self) { Text(VoiceStore.samples[$0]).tag($0) }
            }
            .pickerStyle(.menu)
            Button { Task { await ride.previewVoice(sampleText) } } label: { Label("Play with my current voice", systemImage: "play.circle.fill") }
        }
    }

    private var voiceSection: some View {
        GSection("Voices on this iPhone") {
            Picker("Show", selection: $filter) {
                Text("Indian").tag(0)
                Text("English").tag(1)
                Text("All").tag(2)
            }
            .pickerStyle(.segmented)
            if shown.isEmpty {
                Text("No voices of this kind are installed. See “Get more voices” below.").foregroundStyle(Theme.dim)
            }
            ForEach(shown) { v in row(v) }
            Text("If the voice you chose is ever missing (for example after an iOS update), I use the closest similar installed voice and tell you at the start of a ride. The voice never changes in the middle of a ride.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }

    private func row(_ v: VoiceInfo) -> some View {
        let selected = store.voiceID == v.id
        return VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(v.name).font(.headline)
                if selected { Image(systemName: "checkmark.circle.fill").foregroundStyle(Theme.good) }
                Spacer()
            }
            Text("\(v.accent) · \(v.quality) · \(v.gender)").font(.caption).foregroundStyle(Theme.dim)
            Text("\(v.source) · works offline · installed").font(.caption2).foregroundStyle(Theme.dim)
            HStack(spacing: 10) {
                Button { Task { await ride.previewVoice(sampleText, voiceID: v.id) } } label: {
                    Label("Play", systemImage: "speaker.wave.2.fill")
                }
                Button { compareA = v.id } label: { tag("A", on: compareA == v.id) }
                Button { compareB = v.id } label: { tag("B", on: compareB == v.id) }
                Spacer()
                Button { tapHaptic(); store.select(v) } label: { Text(selected ? "Selected" : "Use this voice") }
                    .disabled(selected)
            }
            .font(.footnote.weight(.semibold))
            .buttonStyle(.borderless)
        }
        .padding(.vertical, 4)
    }

    private func tag(_ t: String, on: Bool) -> some View {
        Text(t)
            .font(.caption.weight(.heavy))
            .frame(width: 26, height: 26)
            .background(Circle().fill(on ? Theme.accent : Color.white.opacity(0.12)))
            .foregroundStyle(on ? Color.black : Color.white)
    }

    private var compareSection: some View {
        GSection("A/B comparison") {
            Text("Tap A on one voice and B on another, then compare. Listen for how place names and numbers sound, and whether the rhythm feels natural.")
                .font(.caption).foregroundStyle(Theme.dim)
            Button {
                guard let a = compareA, let b = compareB else { return }
                Task {
                    await ride.previewVoice(sampleText, voiceID: a)
                    try? await Task.sleep(for: .milliseconds(600))
                    await ride.previewVoice(sampleText, voiceID: b)
                }
            } label: { Label("Play A, then B", systemImage: "arrow.left.arrow.right.circle.fill") }
            .disabled(compareA == nil || compareB == nil)
        }
    }

    private var moreSection: some View {
        GSection("Get more voices") {
            Text("iOS lets apps use the voices installed on your phone, but apps can't download them or list ones you haven't downloaded. For the most natural sound, install Enhanced or Premium voices yourself: iPhone Settings > Accessibility > Spoken Content > Voices > English (India), then pick an Enhanced or Premium voice and wait for the download. Come back here and it will appear.")
                .font(.caption).foregroundStyle(Theme.dim)
            Button("Open iPhone Settings") {
                if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
            }
            Button("Allow Personal Voice (only if you created one)") {
                AVSpeechSynthesizer.requestPersonalVoiceAuthorization { _ in
                    Task { @MainActor in voices = VoiceStore.catalogue() }
                }
            }
            Text("Apple's built-in voices are good but not indistinguishable from a human. Naturalness varies by voice and device; judge them with the previews above.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }
}

// MARK: - Pronunciation dictionary

struct PronunciationView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var store = VoiceStore.shared
    @State private var editing: Pronunciation?

    var body: some View {
        List {
            GSection("Your pronunciations") {
                if store.pronunciations.isEmpty {
                    Text("None yet. Add a place name I say wrongly.").foregroundStyle(Theme.dim)
                }
                ForEach(store.pronunciations) { p in
                    Button { editing = p } label: {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(p.written).font(.headline)
                            Text("spoken as “\(p.spoken)”").font(.caption).foregroundStyle(Theme.dim)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
                .onDelete { store.pronunciations.remove(atOffsets: $0) }
                Button { editing = Pronunciation(written: "", spoken: "") } label: { Label("Add a word", systemImage: "plus.circle.fill") }
            }
            GSection("Built in") {
                Text("NH → National Highway, SH → State Highway, ORR → Outer Ring Road, Rd → Road, Jn → Junction, Hwy → Highway, km/h → kilometres per hour.")
                    .font(.caption).foregroundStyle(Theme.dim)
            }
            GSection("How to respell") {
                Text("Write the name the way it should sound, with hyphens between syllables, for example “Ko-ra-mang-ala”. Tap Test in the editor to hear it with your current voice. Different voices may need slightly different spellings.")
                    .font(.caption).foregroundStyle(Theme.dim)
            }
        }
        .scrollContentBackground(.hidden)
        .background(AppBackground())
        .keyboardDone()
        .navigationTitle("Pronunciation")
        .sheet(item: $editing) { item in
            PronunciationEditor(item: item, isNew: !store.pronunciations.contains { $0.id == item.id }) { saved in
                if let i = store.pronunciations.firstIndex(where: { $0.id == saved.id }) { store.pronunciations[i] = saved }
                else { store.pronunciations.append(saved) }
            } onDelete: {
                store.pronunciations.removeAll { $0.id == item.id }
            }
        }
    }
}

struct PronunciationEditor: View {
    @EnvironmentObject var ride: RideController
    @Environment(\.dismiss) private var dismiss
    let original: Pronunciation
    let isNew: Bool
    let onSave: (Pronunciation) -> Void
    let onDelete: () -> Void
    @State private var written: String
    @State private var spoken: String

    init(item: Pronunciation, isNew: Bool, onSave: @escaping (Pronunciation) -> Void, onDelete: @escaping () -> Void) {
        original = item
        self.isNew = isNew
        self.onSave = onSave
        self.onDelete = onDelete
        _written = State(initialValue: item.written)
        _spoken = State(initialValue: item.spoken)
    }

    var body: some View {
        NavigationStack {
            Form {
                GSection("Word") {
                    TextField("As written (e.g. Koramangala)", text: $written).autocorrectionDisabled()
                    TextField("Spoken as (e.g. Ko-ra-mang-ala)", text: $spoken).autocorrectionDisabled()
                    Button("Test") { Task { await ride.previewVoice("In five hundred metres, turn left onto \(spoken).") } }
                        .disabled(spoken.trimmingCharacters(in: .whitespaces).isEmpty)
                }
                if !isNew { GSection { Button("Delete", role: .destructive) { onDelete(); dismiss() } } }
            }
            .keyboardDone()
            .navigationTitle(isNew ? "New pronunciation" : "Edit pronunciation")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        var p = original
                        p.written = written.trimmingCharacters(in: .whitespaces)
                        p.spoken = spoken.trimmingCharacters(in: .whitespaces)
                        onSave(p)
                        dismiss()
                    }
                    .disabled(written.trimmingCharacters(in: .whitespaces).isEmpty || spoken.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
        .presentationDetents([.medium])
        .preferredColorScheme(.dark)
    }
}
