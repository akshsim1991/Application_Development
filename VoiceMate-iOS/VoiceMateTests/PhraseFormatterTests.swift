import XCTest
@testable import VoiceMate

final class PhraseFormatterTests: XCTestCase {

    func testNumberWords() {
        XCTAssertEqual(PhraseFormatter.words(0), "zero")
        XCTAssertEqual(PhraseFormatter.words(13), "thirteen")
        XCTAssertEqual(PhraseFormatter.words(42), "forty two")
        XCTAssertEqual(PhraseFormatter.words(500), "five hundred")
        XCTAssertEqual(PhraseFormatter.words(1250), "one thousand two hundred fifty")
    }

    func testMetricDistancePhrasing() {
        XCTAssertEqual(PhraseFormatter.distance(500, imperial: false), "five hundred metres")
        XCTAssertEqual(PhraseFormatter.distance(487, imperial: false), "about five hundred metres")
        XCTAssertEqual(PhraseFormatter.distance(30, imperial: false), "about fifty metres")
        XCTAssertEqual(PhraseFormatter.distance(1500, imperial: false), "one point five kilometres")
        XCTAssertEqual(PhraseFormatter.distance(1000, imperial: false), "one kilometre")
        XCTAssertEqual(PhraseFormatter.distance(12_000, imperial: false), "twelve kilometres")
    }

    func testAbbreviationsExpand() {
        let chunks = PhraseFormatter.prepare("Continue on NH 48 for 12 km")
        XCTAssertEqual(chunks.first?.text, "Continue on National Highway 48 for 12 km")
        // "NH" inside another word must not be touched.
        XCTAssertEqual(PhraseFormatter.prepare("Turn left at Anhalt").first?.text, "Turn left at Anhalt")
    }

    func testUserPronunciationOverride() {
        let o = Pronunciation(written: "Koramangala", spoken: "Ko-ra-mang-ala")
        let chunks = PhraseFormatter.prepare("Turn left towards koramangala.", overrides: [o])
        XCTAssertEqual(chunks.first?.text, "Turn left towards Ko-ra-mang-ala")
    }

    func testChunkingAddsPausesButNotAfterLast() {
        let chunks = PhraseFormatter.prepare("Turn left. Then keep right.")
        XCTAssertEqual(chunks.map(\.text), ["Turn left", "Then keep right"])
        XCTAssertEqual(chunks.map(\.pauseFactor), [1, 0])
    }

    func testRoundaboutExitNumber() {
        XCTAssertEqual(PhraseFormatter.exitNumber(from: "At the roundabout, take the 2nd exit onto MG Road"), 2)
        XCTAssertEqual(PhraseFormatter.exitNumber(from: "At the roundabout take the third exit"), 3)
        XCTAssertNil(PhraseFormatter.exitNumber(from: "Turn left onto MG Road"))
        XCTAssertEqual(PhraseFormatter.ordinal(2), "second")
    }
}
