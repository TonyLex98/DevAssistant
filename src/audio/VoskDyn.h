
#pragma once
#include <QLibrary>
#include <QString>

// Opaque types from Vosk C API
struct VoskModel;
struct VoskRecognizer;

// Function pointer typedefs matching Vosk API
typedef void (*fn_vosk_set_log_level)(int);
typedef VoskModel* (*fn_vosk_model_new)(const char*);
typedef void (*fn_vosk_model_free)(VoskModel*);
typedef VoskRecognizer* (*fn_vosk_recognizer_new)(VoskModel*, float);
typedef VoskRecognizer* (*fn_vosk_recognizer_new_grm)(VoskModel*, float, const char*);
typedef int  (*fn_vosk_recognizer_accept_waveform)(VoskRecognizer*, const char*, int);
typedef const char* (*fn_vosk_recognizer_result)(VoskRecognizer*);
typedef const char* (*fn_vosk_recognizer_partial_result)(VoskRecognizer*);
typedef const char* (*fn_vosk_recognizer_final_result)(VoskRecognizer*);
typedef void (*fn_vosk_recognizer_set_words)(VoskRecognizer*, int);
typedef void (*fn_vosk_recognizer_set_partial_words)(VoskRecognizer*, int);
typedef void (*fn_vosk_recognizer_free)(VoskRecognizer*);

class VoskDyn {
public:
    bool load(const QString& dllPath) {
        lib.setFileName(dllPath);
        if (!lib.load()) return false;
        set_log_level = (fn_vosk_set_log_level) lib.resolve("vosk_set_log_level");
        model_new = (fn_vosk_model_new) lib.resolve("vosk_model_new");
        model_free = (fn_vosk_model_free) lib.resolve("vosk_model_free");
        recognizer_new = (fn_vosk_recognizer_new) lib.resolve("vosk_recognizer_new");
        recognizer_new_grm = (fn_vosk_recognizer_new_grm) lib.resolve("vosk_recognizer_new_grm");
        recognizer_accept_waveform = (fn_vosk_recognizer_accept_waveform) lib.resolve("vosk_recognizer_accept_waveform");
        recognizer_result = (fn_vosk_recognizer_result) lib.resolve("vosk_recognizer_result");
        recognizer_partial_result = (fn_vosk_recognizer_partial_result) lib.resolve("vosk_recognizer_partial_result");
        recognizer_final_result = (fn_vosk_recognizer_final_result) lib.resolve("vosk_recognizer_final_result");
        recognizer_set_words = (fn_vosk_recognizer_set_words) lib.resolve("vosk_recognizer_set_words");
        recognizer_set_partial_words = (fn_vosk_recognizer_set_partial_words) lib.resolve("vosk_recognizer_set_partial_words");
        recognizer_free = (fn_vosk_recognizer_free) lib.resolve("vosk_recognizer_free");
        return set_log_level && model_new && model_free && recognizer_new &&
               recognizer_accept_waveform && recognizer_result && recognizer_partial_result &&
               recognizer_final_result && recognizer_free;
    }
    bool isLoaded() const { return lib.isLoaded(); }
    QString errorString() const { return lib.errorString(); }

    QLibrary lib;
    // resolved symbols
    fn_vosk_set_log_level set_log_level = nullptr;
    fn_vosk_model_new model_new = nullptr;
    fn_vosk_model_free model_free = nullptr;
    fn_vosk_recognizer_new recognizer_new = nullptr;
    fn_vosk_recognizer_new_grm recognizer_new_grm = nullptr;
    fn_vosk_recognizer_accept_waveform recognizer_accept_waveform = nullptr;
    fn_vosk_recognizer_result recognizer_result = nullptr;
    fn_vosk_recognizer_partial_result recognizer_partial_result = nullptr;
    fn_vosk_recognizer_final_result recognizer_final_result = nullptr;
    fn_vosk_recognizer_set_words recognizer_set_words = nullptr;
    fn_vosk_recognizer_set_partial_words recognizer_set_partial_words = nullptr;
    fn_vosk_recognizer_free recognizer_free = nullptr;
};
