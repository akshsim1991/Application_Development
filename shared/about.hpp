#pragma once
// The About text, the same in both apps (the version line and the platform words are added by each app).
#include <string>

namespace about {

inline std::wstring body(const std::wstring& platform /* L"Windows" or L"macOS" */, const std::wstring& where /* L"your PC" */) {
    return L"A fast, lightweight PDF reader and editor for " + platform +
           L". Read, search, print and present documents in tabs and windows; change and replace text, fill in and build "
           L"forms, sign, add comments, highlights, drawings, stamps, pictures and tables; add watermarks, page numbers, "
           L"bookmarks and links; reorganise, merge, split and extract pages; compress, redact, password-protect, recognise "
           L"text (OCR), compare and measure; export pages as pictures, text, Word or Excel. Everything happens on " + where +
           L", with no account, cloud service or tracking.\n\n"
           L"Created by: Akshaya Simha\n\n"
           L"DEVELOPMENT HISTORY\n"
           L"Feather PDF began its development journey using Visual Basic .NET (VB.NET). As the application evolved and its "
           L"requirements became more sophisticated, development transitioned to C# and eventually to C++. These transitions "
           L"reflect the continuous effort to improve the application's performance, capabilities, efficiency and overall "
           L"user experience.\n\n"
           L"DEVELOPMENT AND AI CONTRIBUTIONS\n"
           L"Feather PDF is an independently developed software application, with major portions of its code written by its "
           L"developer, Akshaya Simha. Artificial intelligence tools have also contributed to the development process by "
           L"assisting with coding, debugging, optimisation, feature implementation and technical problem-solving. The "
           L"application represents a combination of independent software development, continuous learning, "
           L"experimentation and AI-assisted engineering.\n\n"
           L"CREDITS AND ACKNOWLEDGEMENTS\n"
           L"Akshaya Simha sincerely thanks everyone who has contributed valuable suggestions, feedback and encouragement "
           L"throughout the development of Feather PDF. Your ideas and recommendations are greatly appreciated and play an "
           L"important role in the application's continuous improvement.\n\n"
           L"Courtesy:\nVinod Kumar C S\nAthith D\nMamatha H R\n\n"
           L"FEEDBACK AND SUGGESTIONS\n"
           L"If you have any feedback, suggestions or feature requests, please write to: akshsimha@gmail.com\n"
           L"Every suggestion is taken seriously and carefully considered. Wherever technically feasible, practical and "
           L"aligned with the application's development goals, suggested features may be incorporated into future versions "
           L"of Feather PDF. Thank you for supporting the continued development and improvement of Feather PDF.\n\n"
           L"Free to use and share, always. No subscription, no trial, no licence key.";
}

constexpr const wchar_t* kFeedbackAddress = L"akshsimha@gmail.com";

} // namespace about
