import sys
import os
import torch
import torchaudio
from transformers import Wav2Vec2ForCTC, AutoProcessor

# Model ID for Omnilingual ASR (MMS 300M)
# If this specific model doesn't work out of the box with transformers, 
# we might need to use 'facebook/mms-1b-all' or similar, but user asked for 300m.
# Model ID or Path
# We look for the local model first
# Model ID or Path
# We look for the local model first
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LOCAL_MODEL_PATH_RU = os.path.join(os.path.dirname(SCRIPT_DIR), "models", "wav2vec2-russian")
LOCAL_MODEL_PATH_300M = os.path.join(os.path.dirname(SCRIPT_DIR), "models", "omniASR-CTC-300M")
LOCAL_MODEL_PATH_1B = os.path.join(os.path.dirname(SCRIPT_DIR), "models", "mms-1b-all")

if os.path.exists(LOCAL_MODEL_PATH_RU) and os.path.exists(os.path.join(LOCAL_MODEL_PATH_RU, "config.json")):
    MODEL_ID = LOCAL_MODEL_PATH_RU
elif os.path.exists(LOCAL_MODEL_PATH_300M) and os.path.exists(os.path.join(LOCAL_MODEL_PATH_300M, "config.json")):
    MODEL_ID = LOCAL_MODEL_PATH_300M
elif os.path.exists(LOCAL_MODEL_PATH_1B) and os.path.exists(os.path.join(LOCAL_MODEL_PATH_1B, "config.json")):
    MODEL_ID = LOCAL_MODEL_PATH_1B
else:
    # Fallback to online Russian model
    MODEL_ID = "jonatasgrosman/wav2vec2-large-xlsr-53-russian" 

def main():
    if len(sys.argv) < 2:
        print("Usage: python run_omni_asr.py <wav_file>")
        sys.exit(1)

    wav_path = sys.argv[1]
    
    if not os.path.exists(wav_path):
        print(f"Error: File {wav_path} not found.")
        sys.exit(1)

    try:
        # Try Faster Whisper first if available
        try:
            from faster_whisper import WhisperModel
            print("Using Faster Whisper...", file=sys.stderr)
            
            # Use 'base' or 'small' model for speed/accuracy balance. 'large-v3' is best but slower.
            # 'base' is very fast.
            model_size = "base" 
            
            # Run on CPU with INT8 for compatibility and speed
            model = WhisperModel(model_size, device="cpu", compute_type="int8")
            
            segments, info = model.transcribe(wav_path, beam_size=5)
            
            print("Detected language '%s' with probability %f" % (info.language, info.language_probability), file=sys.stderr)
            
            transcription = ""
            for segment in segments:
                transcription += segment.text + " "
            
            transcription = transcription.strip()
            print(f"Transcription result: {transcription}", file=sys.stderr)
            sys.stdout.buffer.write(transcription.encode('utf-8'))
            return

        except ImportError:
            print("Faster Whisper not found, falling back to Wav2Vec2...", file=sys.stderr)
        except Exception as e_whisper:
            print(f"Faster Whisper error: {e_whisper}. Falling back...", file=sys.stderr)

        # Fallback to Wav2Vec2
        print(f"Loading model: {MODEL_ID}...", file=sys.stderr)
        processor = AutoProcessor.from_pretrained(MODEL_ID)
        model = Wav2Vec2ForCTC.from_pretrained(MODEL_ID)
        
        print("Loading audio...", file=sys.stderr)
        
        # Try to set backend if the function exists (older torchaudio)
        if hasattr(torchaudio, 'set_audio_backend'):
            try:
                torchaudio.set_audio_backend("soundfile")
            except:
                pass

        waveform = None
        sample_rate = None

        try:
            waveform, sample_rate = torchaudio.load(wav_path)
        except Exception as e_load:
             print(f"Error loading audio with torchaudio: {e_load}", file=sys.stderr)
             # Try explicit soundfile load if torchaudio fails (fallback)
             try:
                 import soundfile as sf
                 data, samplerate = sf.read(wav_path)
                 waveform = torch.FloatTensor(data).unsqueeze(0)
                 sample_rate = samplerate
                 print("Loaded audio using soundfile directly.", file=sys.stderr)
             except Exception as e_sf:
                 print(f"Error loading with soundfile fallback: {e_sf}", file=sys.stderr)
                 raise e_load # Re-raise original error if fallback fails
        
        # Resample if needed (MMS usually expects 16kHz)
        if sample_rate != 16000:
            print(f"Resampling from {sample_rate} to 16000...", file=sys.stderr)
            resampler = torchaudio.transforms.Resample(orig_freq=sample_rate, new_freq=16000)
            waveform = resampler(waveform)
            
        inputs = processor(waveform.squeeze(), sampling_rate=16000, return_tensors="pt")
        
        print("Transcribing...", file=sys.stderr)
        with torch.no_grad():
            outputs = model(**inputs).logits
            
        ids = torch.argmax(outputs, dim=-1)[0]
        transcription = processor.decode(ids)
        
        print(f"Transcription result: {transcription}", file=sys.stderr)
        
        # Output the transcription to stdout
        # Ensure we encode to utf-8 to avoid console encoding issues on Windows
        sys.stdout.buffer.write(transcription.encode('utf-8'))
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
