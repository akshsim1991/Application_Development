#include "speech.hpp"
#include <windows.h>
#include <sapi.h>

// mingw's libraries do not carry the SAPI identifiers, so they are spelled out here.
static const CLSID kClsidSpVoice = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
static const IID kIidISpVoice = {0x6C44DF74, 0x72B9, 0x4992, {0xA1, 0xEC, 0xEF, 0x99, 0x6E, 0x04, 0x22, 0xD4}};

namespace speech {
namespace {
ISpVoice* g_voice = nullptr;
bool g_paused = false;
bool g_comReady = false;

bool ensureCom() {
    if (g_comReady) return true;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    return g_comReady;
}
}

bool start(const std::wstring& text, int rate, std::wstring& error) {
    stop();
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
    g_paused = false;
    HRESULT hr = g_voice->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, nullptr);
    if (FAILED(hr)) {
        error = L"The voice could not start. Check that a voice is installed and switched on in Windows (Settings > Time & language > Speech), "
                L"and that your sound output is connected.";
        return false;
    }
    return true;
}

void stop() {
    if (g_voice) {
        g_voice->Resume();
        g_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    }
    g_paused = false;
}

bool speaking() {
    if (!g_voice) return false;
    SPVOICESTATUS st{};
    if (FAILED(g_voice->GetStatus(&st, nullptr))) return false;
    return st.dwRunningState != SPRS_DONE || g_paused;
}

bool paused() { return g_paused; }

void pauseResume() {
    if (!g_voice) return;
    if (g_paused) { g_voice->Resume(); g_paused = false; }
    else { g_voice->Pause(); g_paused = true; }
}

} // namespace speech
