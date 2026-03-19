import glob
import os
import sys


def _status(msg: str) -> None:
    print(f"STATUS: {msg}", file=sys.stderr, flush=True)


def _try_extend_cuda_path() -> None:
    try:
        import nvidia.cudnn  # type: ignore

        if getattr(nvidia.cudnn, "__file__", None):
            cudnn_path = os.path.join(os.path.dirname(nvidia.cudnn.__file__), "bin")
            if os.path.exists(cudnn_path):
                os.environ["PATH"] = cudnn_path + os.pathsep + os.environ["PATH"]
                _status(f"Added CUDNN path: {cudnn_path}")
    except Exception:
        pass

    try:
        for sp in [p for p in sys.path if "site-packages" in p]:
            matches = glob.glob(os.path.join(sp, "nvidia", "cudnn", "bin", "cudnn_ops64_*.dll"))
            if matches:
                dll_dir = os.path.dirname(matches[0])
                if dll_dir not in os.environ["PATH"]:
                    os.environ["PATH"] = dll_dir + os.pathsep + os.environ["PATH"]
                    _status(f"Discovered CUDNN DLLs: {dll_dir}")
                break
    except Exception:
        pass


def _load_model():
    try:
        from faster_whisper import WhisperModel  # type: ignore
    except Exception as e:
        raise RuntimeError(f"faster-whisper is not available: {e}")

    requested_model = os.getenv("FASTER_WHISPER_MODEL", "large-v3-turbo").strip()
    fallback_models = [requested_model, "medium"]

    # GPU first.
    for model_name in fallback_models:
        try:
            _status(f"Loading model '{model_name}' on CUDA...")
            return WhisperModel(model_name, device="cuda", compute_type="float16")
        except Exception as e:
            _status(f"CUDA load failed for '{model_name}': {e}")

    # CPU fallback.
    for model_name in fallback_models:
        try:
            _status(f"Loading model '{model_name}' on CPU...")
            return WhisperModel(model_name, device="cpu", compute_type="int8")
        except Exception as e:
            _status(f"CPU load failed for '{model_name}': {e}")

    raise RuntimeError("Unable to initialize any Faster-Whisper model.")


def _sanitize_text(text: str) -> str:
    text = text.replace("\n", " ").strip()
    while "  " in text:
        text = text.replace("  ", " ")
    banned = {
        "thanks for watching",
        "thank you for watching",
        "subtitles by",
        "amara.org",
    }
    if text.lower() in banned:
        return ""
    return text


def main() -> int:
    _try_extend_cuda_path()
    model = None
    try:
        model = _load_model()
    except Exception as e:
        _status(f"ASR disabled: {e}")
    print("READY", flush=True)

    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        if line == "EXIT":
            break

        wav_path = line
        if not os.path.exists(wav_path):
            print(f"ERROR: File not found: {wav_path}", file=sys.stderr, flush=True)
            continue

        try:
            if model is None:
                sys.stdout.buffer.write(b"\n")
                sys.stdout.flush()
                continue

            segments, _info = model.transcribe(
                wav_path,
                beam_size=1,
                best_of=1,
                vad_filter=True,
                vad_parameters=dict(min_silence_duration_ms=400),
                language=None,  # auto language detection
            )
            text = " ".join(seg.text for seg in segments)
            text = _sanitize_text(text)
            sys.stdout.buffer.write((text + "\n").encode("utf-8"))
            sys.stdout.flush()
        except Exception as e:
            print(f"ERROR: {e}", file=sys.stderr, flush=True)
        finally:
            try:
                os.remove(wav_path)
            except Exception:
                pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
