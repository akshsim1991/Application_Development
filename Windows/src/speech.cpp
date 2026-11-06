#include "speech.hpp"
#include <windows.h>
#include <sapi.h>
#include <cstdlib>
#include <cwctype>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>

// mingw's libraries do not carry the SAPI identifiers, so they are spelled out here.
static const CLSID kClsidSpVoice = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
static const IID kIidISpVoice = {0x6C44DF74, 0x72B9, 0x4992, {0xA1, 0xEC, 0xEF, 0x99, 0x6E, 0x04, 0x22, 0xD4}};

namespace speech {
namespace {
// The build check's stand-in voice (FEATHER_FAKE_VOICE=1): reports each word every 120 ms, as SAPI would, without sound.
struct Fake {
    std::mutex m;
    std::deque<std::pair<size_t, size_t>> words;   // position, length; (npos, 0) = finished
    std::atomic<bool> run{false}, paused{false};
    std::thread t;
    unsigned generation = 0;
} g_fake;
bool fakeVoice() { return getenv("FEATHER_FAKE_VOICE") != nullptr; }
void fakeStop() {
    g_fake.run = false;
    if (g_fake.t.joinable()) g_fake.t.join();
    std::lock_guard<std::mutex> l(g_fake.m);
    g_fake.words.clear();
}
void fakeStart(const std::wstring& text, HWND notify, UINT msg) {
    fakeStop();
    g_fake.run = true;
    g_fake.paused = false;
    g_fake.t = std::thread([text, notify, msg]() {
        size_t i = 0;
        while (g_fake.run && i < text.size()) {
            while (i < text.size() && std::iswspace(text[i])) ++i;
            size_t j = i;
            while (j < text.size() && !std::iswspace(text[j])) ++j;
            if (j > i) { { std::lock_guard<std::mutex> l(g_fake.m); g_fake.words.push_back({i, j - i}); } PostMessageW(notify, msg, 0, 0); }
            i = j;
            for (int k = 0; k < 12 && g_fake.run; ++k) { Sleep(10); while (g_fake.paused && g_fake.run) Sleep(20); }
        }
        if (g_fake.run) { { std::lock_guard<std::mutex> l(g_fake.m); g_fake.words.push_back({static_cast<size_t>(-1), 0}); } PostMessageW(notify, msg, 0, 0); }
        g_fake.run = false;
    });
}
ISpVoice* g_voice = nullptr;
bool g_paused = false;
ULONG g_stream = 0;   // the stream being spoken: events of earlier (purged) ones are ignored
bool g_comReady = false;

bool ensureCom() {
    if (g_comReady) return true;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    return g_comReady;
}
}

bool start(const std::wstring& text, int rate, std::wstring& error) { return start(text, rate, error, nullptr, 0); }

bool start(const std::wstring& text, int rate, std::wstring& error, HWND notify, UINT msg) {
    stop();
    if (fakeVoice() && notify) { fakeStart(text, notify, msg); g_paused = false; return true; }
    if (!ensureCom()) { error = L"Reading aloud could not start (the Windows speech system could not be initialised)."; return false; }
    if (!g_voice) {
        HRESULT hr = CoCreateInstance(kClsidSpVoice, nullptr, CLSCTX_ALL, kIidISpVoice, reinterpret_cast<void**>(&g_voice));
        if (FAILED(hr) || !g_voice) {
            g_voice = nullptr;
            error = L"Reading aloud uses the speech voices of Windows, which are not available on this PC.\n\n"
                    L"On Windows 10 or 11 you can add a voice in Settings > Time & language > Speech.";
            return false;
        }
    }
    g_voice->SetRate(rate < -10 ? -10 : rate > 10 ? 10 : rate);
    if (notify) {
        const ULONGLONG want = SPFEI(SPEI_WORD_BOUNDARY) | SPFEI(SPEI_END_INPUT_STREAM);
        g_voice->SetInterest(want, want);
        g_voice->SetNotifyWindowMessage(notify, msg, 0, 0);
    } else g_voice->SetInterest(0, 0);
    g_paused = false;
    HRESULT hr = g_voice->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, &g_stream);
    if (FAILED(hr)) {
        error = L"The voice could not start. Check that a voice is installed and switched on in Windows (Settings > Time & language > Speech), "
                L"and that your sound output is connected.";
        return false;
    }
    return true;
}

void events(const std::function<void(int kind, size_t pos, size_t len)>& f) {
    if (fakeVoice()) {
        std::deque<std::pair<size_t, size_t>> got;
        { std::lock_guard<std::mutex> l(g_fake.m); got.swap(g_fake.words); }
        for (const auto& w : got) { if (w.first == static_cast<size_t>(-1)) f(1, 0, 0); else f(0, w.first, w.second); }
        return;
    }
    if (!g_voice) return;
    SPEVENT e{};
    ULONG got = 0;
    while (g_voice->GetEvents(1, &e, &got) == S_OK && got == 1) {
        if (e.ulStreamNum == g_stream) {
            if (e.eEventId == SPEI_WORD_BOUNDARY) f(0, static_cast<size_t>(e.lParam), static_cast<size_t>(e.wParam));
            else if (e.eEventId == SPEI_END_INPUT_STREAM) f(1, 0, 0);
        }
        if (e.elParamType == SPET_LPARAM_IS_STRING || e.elParamType == SPET_LPARAM_IS_POINTER) CoTaskMemFree(reinterpret_cast<void*>(e.lParam));
        else if (e.elParamType == SPET_LPARAM_IS_OBJECT && e.lParam) reinterpret_cast<IUnknown*>(e.lParam)->Release();
        e = SPEVENT{};
    }
}

void stop() {
    if (fakeVoice()) fakeStop();
    if (g_voice) {
        g_voice->Resume();
        g_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    }
    g_paused = false;
}

bool speaking() {
    if (fakeVoice()) return g_fake.run || g_paused;
    if (!g_voice) return false;
    SPVOICESTATUS st{};
    if (FAILED(g_voice->GetStatus(&st, nullptr))) return false;
    return st.dwRunningState != SPRS_DONE || g_paused;
}

bool paused() { return g_paused; }

void pauseResume() {
    if (fakeVoice()) { g_paused = !g_paused; g_fake.paused = g_paused; return; }
    if (!g_voice) return;
    if (g_paused) { g_voice->Resume(); g_paused = false; }
    else { g_voice->Pause(); g_paused = true; }
}

} // namespace speech
