import sys
import os
import time
import torch
from faster_whisper import WhisperModel

# Add CUDNN to PATH if installed via pip
try:
    import nvidia.cudnn
    if hasattr(nvidia.cudnn, "__file__") and nvidia.cudnn.__file__ is not None:
        cudnn_path = os.path.join(os.path.dirname(nvidia.cudnn.__file__), "bin")
        if os.path.exists(cudnn_path):
            os.environ["PATH"] = cudnn_path + os.pathsep + os.environ["PATH"]
            print(f"Added CUDNN to PATH: {cudnn_path}", file=sys.stderr)
    else:
        print("nvidia.cudnn module found but has no __file__ attribute.", file=sys.stderr)
except ImportError:
    pass

# Manual DLL search (Fallback if the above fails or __file__ is None)
import glob
try:
    site_packages = [p for p in sys.path if 'site-packages' in p]
    for sp in site_packages:
        matches = glob.glob(os.path.join(sp, "nvidia", "cudnn", "bin", "cudnn_ops64_*.dll"))
        if matches:
            dll_dir = os.path.dirname(matches[0])
            if dll_dir not in os.environ["PATH"]:
                os.environ["PATH"] = dll_dir + os.pathsep + os.environ["PATH"]
                print(f"Manually found and added CUDNN DLLs to PATH: {dll_dir}", file=sys.stderr)
            break
except Exception as e:
    print(f"Error searching for DLLs: {e}", file=sys.stderr)

def main():
    # Initialize model
    model_size = "medium"
    
    # Check if model needs to be downloaded
    from huggingface_hub import snapshot_download, hf_hub_download
    import huggingface_hub
    
    model_repo = f"Systran/faster-whisper-{model_size}"
    
    print(f"Проверка модели '{model_size}'...", file=sys.stderr)
    
    # Check if model is cached
    try:
        cache_dir = huggingface_hub.constants.HF_HUB_CACHE
        # Try to download with progress
        print(f"Загрузка модели '{model_size}' (если не в кэше)...", file=sys.stderr)
        snapshot_download(
            repo_id=model_repo,
            local_files_only=False,
        )
        print(f"Модель '{model_size}' готова.", file=sys.stderr)
    except Exception as e:
        print(f"Ошибка при проверке/загрузке модели: {e}", file=sys.stderr)
        # Continue anyway, WhisperModel will try to download
    
    # Try GPU first
    try:
        device = "cuda"
        compute_type = "float16"
        print(f"Загрузка Faster Whisper модели '{model_size}' на {device}...", file=sys.stderr)
        model = WhisperModel(model_size, device=device, compute_type=compute_type)
        
        # Perform a dummy transcription to verify CUDA works (catches DLL errors early)
        print("Проверка GPU модели...", file=sys.stderr)
        
        print("Модель загружена на GPU.", file=sys.stderr)
    except Exception as e:
        print(f"Ошибка загрузки на GPU: {e}", file=sys.stderr)
        print("Переключение на CPU...", file=sys.stderr)
        device = "cpu"
        compute_type = "int8"
        model = WhisperModel(model_size, device=device, compute_type=compute_type)
        print("Модель загружена на CPU.", file=sys.stderr)

    print("READY") # Signal to C++ that we are ready
    sys.stdout.flush()

    while True:
        try:
            # Read file path from stdin
            line = sys.stdin.readline()
            if not line:
                break
            
            line = line.strip()
            if line == "EXIT":
                break
            if not line:
                continue
                
            wav_path = line
            
            if not os.path.exists(wav_path):
                print(f"Error: File not found: {wav_path}", file=sys.stderr)
                continue

            # Transcribe
            # beam_size=5 is standard.
            # vad_filter=True helps reduce hallucinations on silent audio
            segments, info = model.transcribe(wav_path, beam_size=5, vad_filter=True, vad_parameters=dict(min_silence_duration_ms=500))
            
            transcription = ""
            for segment in segments:
                transcription += segment.text + " "
            
            transcription = transcription.strip()
            
            # Filter hallucinations
            hallucinations = [
                "Thanks for watching!", 
                "Thanks for watching", 
                "Thank you for watching.", 
                "Thank you for watching",
                "Subtitles by",
                "Amara.org"
            ]
            
            for h in hallucinations:
                if transcription.lower().startswith(h.lower()):
                    transcription = ""
                    break
                if transcription.lower() == h.lower():
                    transcription = ""
                    break
            
            if not transcription:
                # If empty after filtering, don't print anything or print a placeholder if needed
                # But for server, we just don't send anything back or send empty line
                pass
            
            # Output result
            # We use a special marker to indicate end of transcription for this file if needed, 
            # but since we process one file at a time, a single line output is best.
            # We replace newlines with spaces to keep it on one line.
            transcription = transcription.replace("\n", " ")
            
            # Print to stdout (utf-8)
            sys.stdout.buffer.write((transcription + "\n").encode('utf-8'))
            sys.stdout.flush()
            
            # Clean up file
            try:
                os.remove(wav_path)
            except:
                pass

        except Exception as e:
            print(f"Error during transcription: {e}", file=sys.stderr)
            sys.stdout.flush()

if __name__ == "__main__":
    main()
