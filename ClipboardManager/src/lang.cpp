// Interface languages. English is the source text; a missing translation falls back to English, string by string.
#include "app.h"
#include <unordered_map>

struct Tr { const wchar_t* en; const wchar_t* hi; const wchar_t* es; };

static const Tr TABLE[] = {
    {L"Everything", L"सब कुछ", L"Todo"},
    {L"Text", L"टेक्स्ट", L"Texto"},
    {L"Pictures", L"चित्र", L"Imágenes"},
    {L"Files", L"फ़ाइलें", L"Archivos"},
    {L"Pinned", L"पिन किए गए", L"Fijados"},
    {L"Snippets", L"स्निपेट", L"Fragmentos"},
    {L"Copied today", L"आज कॉपी किया", L"Copiado hoy"},
    {L"Recently deleted", L"हाल में हटाए गए", L"Eliminados recientemente"},
    {L"Links", L"लिंक", L"Enlaces"},
    {L"Email addresses", L"ईमेल पते", L"Correos"},
    {L"Phone numbers", L"फ़ोन नंबर", L"Teléfonos"},
    {L"Colours", L"रंग", L"Colores"},
    {L"File paths", L"फ़ाइल पथ", L"Rutas"},
    {L"Groups", L"समूह", L"Grupos"},
    {L"Kind of text", L"टेक्स्ट का प्रकार", L"Tipo de texto"},
    {L"No groups yet", L"अभी कोई समूह नहीं", L"Aún no hay grupos"},
    {L"Search everything you copied  (Ctrl+F)", L"कॉपी की हुई हर चीज़ खोजें  (Ctrl+F)", L"Buscar todo lo copiado  (Ctrl+F)"},
    {L"Paste", L"पेस्ट", L"Pegar"},
    {L"Copy", L"कॉपी", L"Copiar"},
    {L"Pin", L"पिन", L"Fijar"},
    {L"Delete", L"हटाएँ", L"Eliminar"},
    {L"Folder", L"फ़ोल्डर", L"Carpeta"},
    {L"More", L"और", L"Más"},
    {L"Paste into the program you were using (Enter)", L"जिस प्रोग्राम में थे उसमें पेस्ट करें (Enter)", L"Pegar en el programa que usaba (Intro)"},
    {L"Put it on the clipboard (Ctrl+C)", L"क्लिपबोर्ड पर रखें (Ctrl+C)", L"Poner en el portapapeles (Ctrl+C)"},
    {L"Pinned items stay at hand and are never deleted automatically (Ctrl+P)", L"पिन किए गए आइटम हाथ में रहते हैं और अपने-आप कभी नहीं हटते (Ctrl+P)", L"Los fijados siempre están a mano y nunca se borran solos (Ctrl+P)"},
    {L"Delete the selected items (Del); Ctrl+Z brings them back", L"चुने हुए आइटम हटाएँ (Del); Ctrl+Z से वापस आते हैं", L"Eliminar lo seleccionado (Supr); Ctrl+Z lo recupera"},
    {L"Open the Clipboard contents folder", L"क्लिपबोर्ड सामग्री फ़ोल्डर खोलें", L"Abrir la carpeta del contenido"},
    {L"Save as, pause recording, snippets, backup, settings, About", L"इस रूप में सहेजें, रिकॉर्डिंग रोकें, स्निपेट, बैकअप, सेटिंग, परिचय", L"Guardar como, pausar, fragmentos, copia, ajustes, Acerca de"},
    {L"Select an item to see all of it.", L"पूरा देखने के लिए कोई आइटम चुनें।", L"Seleccione un elemento para verlo completo."},
    {L"Nothing matches.", L"कुछ भी मेल नहीं खाता।", L"Nada coincide."},
    {L"Nothing copied yet", L"अभी कुछ कॉपी नहीं किया", L"Aún no se ha copiado nada"},
    {L"Copy something — text, a picture or files — and it appears here.", L"कुछ कॉपी करें — टेक्स्ट, चित्र या फ़ाइलें — और वह यहाँ दिखेगा।", L"Copie algo — texto, una imagen o archivos — y aparecerá aquí."},
    {L"Try other words, or choose Everything in the filter.", L"दूसरे शब्द आज़माएँ, या फ़िल्टर में \"सब कुछ\" चुनें।", L"Pruebe otras palabras o elija Todo en el filtro."},
    {L"Paused", L"रुका हुआ", L"En pausa"},
    {L"click to resume", L"फिर शुरू करने के लिए क्लिक करें", L"clic para reanudar"},
    {L"Recording", L"रिकॉर्डिंग", L"Grabando"},
    {L"click to pause", L"रोकने के लिए क्लिक करें", L"clic para pausar"},
    {L"item", L"आइटम", L"elemento"},
    {L"items", L"आइटम", L"elementos"},
    {L"shown", L"दिखाए गए", L"mostrados"},
    {L"Enter pastes into", L"Enter पेस्ट करता है:", L"Intro pega en"},
    {L"Enter copies", L"Enter कॉपी करता है", L"Intro copia"},
    {L"PICTURE", L"चित्र", L"IMAGEN"}, {L"FILES", L"फ़ाइलें", L"ARCHIVOS"}, {L"SNIPPET", L"स्निपेट", L"FRAGMENTO"},
    {L"LINK", L"लिंक", L"ENLACE"}, {L"EMAIL", L"ईमेल", L"CORREO"}, {L"PHONE", L"फ़ोन", L"TEL."}, {L"COLOUR", L"रंग", L"COLOR"},
    {L"PATH", L"पथ", L"RUTA"}, {L"TEXT", L"टेक्स्ट", L"TEXTO"},
    {L"Today", L"आज", L"Hoy"}, {L"Yesterday", L"कल", L"Ayer"},
    {L"Picture", L"चित्र", L"Imagen"},
    {L"1 copied file", L"1 कॉपी की गई फ़ाइल", L"1 archivo copiado"},
    {L"copied files", L"कॉपी की गई फ़ाइलें", L"archivos copiados"},
    {L"(spaces or empty lines only)", L"(केवल खाली जगह या खाली पंक्तियाँ)", L"(solo espacios o líneas vacías)"},
    {L"Copied", L"कॉपी किया", L"Copiado"},
    {L"from", L"से", L"desde"},
    {L"pixels", L"पिक्सेल", L"píxeles"},
    {L"line", L"पंक्ति", L"línea"}, {L"lines", L"पंक्तियाँ", L"líneas"},
    {L"character", L"अक्षर", L"carácter"}, {L"characters", L"अक्षर", L"caracteres"},
    {L"items selected.", L"आइटम चुने गए।", L"elementos seleccionados."},
    {L"Copied. Press Ctrl+V to paste it anywhere.", L"कॉपी हो गया। कहीं भी पेस्ट करने के लिए Ctrl+V दबाएँ।", L"Copiado. Pulse Ctrl+V para pegarlo donde quiera."},
    {L"&Paste into", L"&पेस्ट करें:", L"&Pegar en"},
    {L"&Copy", L"&कॉपी", L"&Copiar"},
    {L"Paste as plain &text", L"सादे &टेक्स्ट के रूप में पेस्ट", L"Pegar como &texto sin formato"},
    {L"Paste &as", L"इस रूप में पेस्ट &करें", L"Pegar &como"},
    {L"&Delete", L"&हटाएँ", L"&Eliminar"},
    {L"Select &all", L"&सभी चुनें", L"Seleccionar &todo"},
    {L"&Hide the window", L"विंडो &छिपाएँ", L"&Ocultar la ventana"},
    {L"&Group", L"&समूह", L"&Grupo"},
    {L"New group…", L"नया समूह…", L"Nuevo grupo…"},
    {L"No group", L"कोई समूह नहीं", L"Sin grupo"},
    {L"&Restore", L"&वापस लाएँ", L"&Restaurar"},
    {L"&Open Feather Clipboard Manager", L"क्लिपबोर्ड मैनेजर &खोलें", L"&Abrir Feather Clipboard Manager"},
    {L"&Copy a recent item", L"हाल का आइटम &कॉपी करें", L"&Copiar un elemento reciente"},
    {L"&Pause recording", L"रिकॉर्डिंग &रोकें", L"&Pausar la grabación"},
    {L"&Settings", L"&सेटिंग", L"A&justes"},
    {L"S&ettings", L"&सेटिंग", L"A&justes"},
    {L"&About Feather Clipboard Manager", L"क्लिपबोर्ड मैनेजर के &बारे में", L"&Acerca de Feather Clipboard Manager"},
    {L"A&bout Feather Clipboard Manager", L"क्लिपबोर्ड मैनेजर के &बारे में", L"&Acerca de Feather Clipboard Manager"},
    {L"E&xit", L"&बाहर निकलें", L"&Salir"},
    {L"Start with &Windows", L"&Windows के साथ शुरू करें", L"Iniciar con &Windows"},
    {L"&Hide to the tray", L"ट्रे में &छिपाएँ", L"&Ocultar en la bandeja"},
    {L"Settings", L"सेटिंग", L"Ajustes"},
    {L"General", L"सामान्य", L"General"},
    {L"What to keep", L"क्या रखना है", L"Qué conservar"},
    {L"Privacy", L"गोपनीयता", L"Privacidad"},
    {L"Shortcuts", L"शॉर्टकट", L"Atajos"},
    {L"Appearance", L"रूप-रंग", L"Apariencia"},
    {L"Backup", L"बैकअप", L"Copia de seguridad"},
    {L"OK", L"ठीक है", L"Aceptar"},
    {L"Cancel", L"रद्द करें", L"Cancelar"},
    {L"Start with Windows", L"Windows के साथ शुरू करें", L"Iniciar con Windows"},
    {L"...in the tray, without the window", L"...ट्रे में, विंडो के बिना", L"...en la bandeja, sin la ventana"},
    {L"Closing the window keeps it recording in the tray", L"विंडो बंद करने पर भी ट्रे में रिकॉर्डिंग चलती रहती है", L"Al cerrar la ventana sigue grabando en la bandeja"},
    {L"Enter or a double-click pastes into the program you were using", L"Enter या डबल-क्लिक से उसी प्रोग्राम में पेस्ट होता है जिसमें आप थे", L"Intro o doble clic pega en el programa que usaba"},
    {L"Language:", L"भाषा:", L"Idioma:"},
    {L"Theme:", L"थीम:", L"Tema:"},
    {L"Same as Windows", L"Windows जैसा", L"Igual que Windows"},
    {L"Light", L"हल्का", L"Claro"},
    {L"Dark", L"गहरा", L"Oscuro"},
    {L"Open folder", L"फ़ोल्डर खोलें", L"Abrir carpeta"},
    {L"None", L"कोई नहीं", L"Ninguno"},
    {L"Text is always kept. Also keep:", L"टेक्स्ट हमेशा रखा जाता है। इन्हें भी रखें:", L"El texto siempre se conserva. Conservar también:"},
    {L"Edit the picture (crop, draw, resize)…", L"चित्र संपादित करें…", L"Editar la imagen…"},
    {L"Crop", L"काटें", L"Recortar"}, {L"Draw", L"बनाएँ", L"Dibujar"}, {L"Highlight", L"हाइलाइट", L"Resaltar"}, {L"Rotate", L"घुमाएँ", L"Girar"},
    {L"Undo", L"पूर्ववत", L"Deshacer"}, {L"Close", L"बंद करें", L"Cerrar"},
    {L"About Feather Clipboard Manager", L"क्लिपबोर्ड मैनेजर के बारे में", L"Acerca de Feather Clipboard Manager"},
    {L"Developed for faster experience.", L"तेज़ अनुभव के लिए विकसित।", L"Desarrollado para una experiencia más rápida."},
    {L"Statistics", L"आँकड़े", L"Estadísticas"},
    {L"Name", L"नाम", L"Nombre"},
    {L"New snippet", L"नया स्निपेट", L"Nuevo fragmento"},
    {L"Edit text", L"टेक्स्ट संपादित करें", L"Editar texto"},
    {L"Recording again.", L"रिकॉर्डिंग फिर शुरू।", L"Grabando de nuevo."},
    {L"Recording is paused: what you copy now is not kept.", L"रिकॉर्डिंग रुकी है: अब जो कॉपी करेंगे वह नहीं रखा जाएगा।", L"La grabación está en pausa: lo que copie ahora no se conserva."},
};

static std::unordered_map<std::wstring, std::wstring> g_map;
static int g_lang = 0;

void SetLanguage(int l) {
    g_lang = l;
    g_map.clear();
    if (l == LANG_EN) return;
    for (auto& t : TABLE) {
        const wchar_t* tr = l == LANG_HI ? t.hi : t.es;
        if (tr && *tr) g_map[t.en] = tr;
    }
}

const wchar_t* T(const wchar_t* en) {
    if (g_lang == LANG_EN || !en) return en;
    auto it = g_map.find(en);
    return it == g_map.end() ? en : it->second.c_str();
}

const wchar_t* LanguageName(int l) {
    switch (l) {
    case LANG_HI: return L"हिन्दी (Hindi)";
    case LANG_ES: return L"Español (Spanish)";
    default: return L"English";
    }
}
