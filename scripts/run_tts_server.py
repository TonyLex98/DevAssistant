import sys
import os
import io

# Force UTF-8 encoding for stdin/stdout
sys.stdin = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8')
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

import torch
import sounddevice as sd

def main():
    print("Инициализация TTS...", file=sys.stderr)
    
    # Silero TTS - automatically downloads model if not present
    print("Загрузка модели Silero TTS (автоматическая загрузка если отсутствует)...", file=sys.stderr)
    
    try:
        # Load Silero model from torch hub
        device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        print(f"Устройство: {device}", file=sys.stderr)
        
        # This will automatically download the model if not cached
        model, example_text = torch.hub.load(
            repo_or_dir='snakers4/silero-models',
            model='silero_tts',
            language='ru',
            speaker='v4_ru'
        )
        model.to(device)
        
        # Available speakers for Russian v4: 
        # aidar, baya, kseniya, xenia, eugene, random
        speaker = 'xenia'  # Female voice, good quality
        sample_rate = 48000
        
        print(f"Модель Silero TTS загружена. Голос: {speaker}", file=sys.stderr)
        print("TTS_READY")
        sys.stdout.flush()
        
    except Exception as e:
        print(f"Ошибка загрузки Silero TTS: {e}", file=sys.stderr)
        print("Попытка использовать pyttsx3...", file=sys.stderr)
        
        # Fallback to pyttsx3
        try:
            import pyttsx3
            engine = pyttsx3.init()
            
            # Try to find Russian voice
            voices = engine.getProperty('voices')
            for voice in voices:
                if 'russian' in voice.name.lower() or 'ru' in voice.id.lower():
                    engine.setProperty('voice', voice.id)
                    break
            
            engine.setProperty('rate', 180)
            
            print("Используется pyttsx3 (системные голоса)", file=sys.stderr)
            print("TTS_READY")
            sys.stdout.flush()
            
            # pyttsx3 mode
            while True:
                try:
                    line = sys.stdin.readline()
                    if not line:
                        break
                    line = line.strip()
                    if line == "EXIT":
                        break
                    if not line:
                        continue
                    
                    engine.say(line)
                    engine.runAndWait()
                    print("TTS_DONE")
                    sys.stdout.flush()
                except Exception as e:
                    print(f"Ошибка: {e}", file=sys.stderr)
            return
            
        except Exception as e2:
            print(f"Ошибка pyttsx3: {e2}", file=sys.stderr)
            sys.exit(1)

    # Main Silero TTS loop
    while True:
        try:
            line = sys.stdin.readline()
            if not line:
                break
            
            line = line.strip()
            if line == "EXIT":
                break
            if not line:
                continue
            
            print(f"Синтез: {line[:50]}...", file=sys.stderr)
            
            # Generate audio
            audio = model.apply_tts(
                text=line,
                speaker=speaker,
                sample_rate=sample_rate
            )
            
            # Play audio
            sd.play(audio.cpu().numpy(), sample_rate)
            sd.wait()  # Wait until audio is finished
            
            print("TTS_DONE")
            sys.stdout.flush()
            
        except Exception as e:
            print(f"Ошибка синтеза: {e}", file=sys.stderr)
            sys.stdout.flush()

if __name__ == "__main__":
    main()
