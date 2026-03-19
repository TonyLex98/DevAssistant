import io
import os
import re
import sys
import tempfile
import time

# Force UTF-8 on stdio for reliable IPC with Qt.
sys.stdin = io.TextIOWrapper(sys.stdin.buffer, encoding="utf-8")
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")


def _log(msg: str) -> None:
    print(f"TTS_STATUS: {msg}", file=sys.stderr, flush=True)


def _guess_lang(text: str) -> str:
    # Fast heuristic fallback that works without extra dependencies.
    if re.search(r"[\u0400-\u04FF]", text):
        return "ru"
    if re.search(r"[\u4E00-\u9FFF]", text):
        return "zh"
    if re.search(r"[\u3040-\u30FF]", text):
        return "ja"
    if re.search(r"[\uAC00-\uD7AF]", text):
        return "ko"
    # Latin alphabets via explicit escapes to avoid locale-dependent mojibake.
    if re.search(r"[\u00E1\u00E9\u00ED\u00F3\u00FA\u00F1\u00FC\u00BF\u00A1]", text.lower()):
        return "es"
    if re.search(r"[\u00E0\u00E8\u00EC\u00F2\u00F9]", text.lower()):
        return "it"
    if re.search(r"[\u00E0\u00E2\u00E7\u00E9\u00E8\u00EA\u00EB\u00EE\u00EF\u00F4\u00FB\u00F9\u00FC\u00FF\u0153]", text.lower()):
        return "fr"
    if re.search(r"[\u00E4\u00F6\u00FC\u00DF]", text.lower()):
        return "de"
    return "en"

VOICE_BY_LANG = {
    "en": "en-US-AriaNeural",
    "ru": "ru-RU-SvetlanaNeural",
    "de": "de-DE-KatjaNeural",
    "fr": "fr-FR-DeniseNeural",
    "es": "es-ES-ElviraNeural",
    "it": "it-IT-ElsaNeural",
    "ja": "ja-JP-NanamiNeural",
    "ko": "ko-KR-SunHiNeural",
    "zh": "zh-CN-XiaoxiaoNeural",
}


class EdgeNeuralBackend:
    def __init__(self, forced_voice: str = "") -> None:
        import asyncio  # noqa: F401
        import edge_tts  # noqa: F401
        import pygame  # noqa: F401

        self._forced_voice = forced_voice.strip()
        self._edge_tts = edge_tts
        self._asyncio = __import__("asyncio")
        self._pygame = pygame
        self._pygame.mixer.init(frequency=24000)
        self.name = "edge-neural"

    def _voice_for_text(self, text: str) -> str:
        if self._forced_voice:
            return self._forced_voice
        lang = _guess_lang(text)
        return VOICE_BY_LANG.get(lang, VOICE_BY_LANG["en"])

    def speak(self, text: str) -> None:
        voice = self._voice_for_text(text)
        fd, tmp_path = tempfile.mkstemp(prefix="devass_tts_", suffix=".mp3")
        os.close(fd)
        try:
            async def _gen() -> None:
                communicate = self._edge_tts.Communicate(text=text, voice=voice)
                await communicate.save(tmp_path)

            self._asyncio.run(_gen())
            self._pygame.mixer.music.load(tmp_path)
            self._pygame.mixer.music.play()
            while self._pygame.mixer.music.get_busy():
                time.sleep(0.05)
        finally:
            try:
                os.remove(tmp_path)
            except OSError:
                pass


class PyttsxFallbackBackend:
    def __init__(self) -> None:
        import pyttsx3  # noqa: F401
        self._pyttsx3 = pyttsx3
        self._engine = pyttsx3.init()
        self._engine.setProperty("rate", 180)
        self.name = "pyttsx3-fallback"

    def speak(self, text: str) -> None:
        self._engine.say(text)
        self._engine.runAndWait()


class SapiComBackend:
    def __init__(self, forced_voice: str = "") -> None:
        import win32com.client  # type: ignore

        self._spk = win32com.client.Dispatch("SAPI.SpVoice")
        self._forced_voice = forced_voice.strip().lower()
        self.name = "sapi-com"

        try:
            voices = self._spk.GetVoices()
            selected = None
            if self._forced_voice:
                for i in range(voices.Count):
                    v = voices.Item(i)
                    if self._forced_voice in str(v.GetDescription()).lower():
                        selected = v
                        break
            if selected is not None:
                self._spk.Voice = selected
        except Exception:
            pass

    def speak(self, text: str) -> None:
        self._spk.Speak(text)


class NullBackend:
    def __init__(self) -> None:
        self.name = "null-backend"

    def speak(self, text: str) -> None:
        _log(f"null backend active, text skipped ({len(text)} chars)")


def create_backend() -> object:
    forced_voice = os.getenv("DEVASS_TTS_VOICE", "").strip()

    try:
        backend = EdgeNeuralBackend(forced_voice=forced_voice)
        _log(f"backend selected: {backend.name}")
        return backend
    except Exception as e:
        _log(f"edge-neural unavailable: {e}")

    try:
        backend = SapiComBackend(forced_voice=forced_voice)
        _log(f"backend selected: {backend.name}")
        return backend
    except Exception as e:
        _log(f"sapi fallback unavailable: {e}")

    try:
        backend = PyttsxFallbackBackend()
        _log(f"backend selected: {backend.name}")
        return backend
    except Exception as e:
        _log(f"pyttsx fallback unavailable: {e}")

    backend = NullBackend()
    _log(f"backend selected: {backend.name}")
    return backend


def main() -> int:
    _log("initializing TTS backend...")
    backend = create_backend()

    print("TTS_READY", flush=True)

    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        if line == "EXIT":
            break

        # Config message support (optional).
        if line.startswith("__VOICE__="):
            voice = line.split("=", 1)[1].strip()
            os.environ["DEVASS_TTS_VOICE"] = voice
            _log(f"voice updated: {voice}")
            continue

        text = re.sub(r"\s+", " ", line).strip()
        if not text:
            continue

        try:
            backend.speak(text)
            print("TTS_DONE", flush=True)
        except Exception as e:
            print(f"TTS_ERROR: {e}", file=sys.stderr, flush=True)
            backend = NullBackend()
            print("TTS_DONE", flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

