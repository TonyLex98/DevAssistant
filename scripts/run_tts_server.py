import sys
import os
import io
import re

# Force UTF-8 encoding for stdin/stdout
sys.stdin = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8')
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

import torch
import sounddevice as sd

# Number to Russian words conversion
def num_to_russian(match):
    """Convert number to Russian words"""
    num = int(match.group())
    if num == 0:
        return "ноль"
    
    ones = ["", "один", "два", "три", "четыре", "пять", "шесть", "семь", "восемь", "девять"]
    teens = ["десять", "одиннадцать", "двенадцать", "тринадцать", "четырнадцать", 
             "пятнадцать", "шестнадцать", "семнадцать", "восемнадцать", "девятнадцать"]
    tens = ["", "", "двадцать", "тридцать", "сорок", "пятьдесят", 
            "шестьдесят", "семьдесят", "восемьдесят", "девяносто"]
    hundreds = ["", "сто", "двести", "триста", "четыреста", "пятьсот",
                "шестьсот", "семьсот", "восемьсот", "девятьсот"]
    
    if num < 0:
        return "минус " + num_to_russian(re.match(r'\d+', str(abs(num))))
    
    if num < 10:
        return ones[num]
    elif num < 20:
        return teens[num - 10]
    elif num < 100:
        return tens[num // 10] + (" " + ones[num % 10] if num % 10 else "")
    elif num < 1000:
        return hundreds[num // 100] + (" " + num_to_russian(re.match(r'\d+', str(num % 100))) if num % 100 else "")
    elif num < 1000000:
        thousands = num // 1000
        remainder = num % 1000
        if thousands == 1:
            t_word = "тысяча"
        elif thousands < 5:
            t_word = num_to_russian(re.match(r'\d+', str(thousands))).replace("два", "две") + " тысячи"
        else:
            t_word = num_to_russian(re.match(r'\d+', str(thousands))) + " тысяч"
        return t_word + (" " + num_to_russian(re.match(r'\d+', str(remainder))) if remainder else "")
    else:
        return str(num)  # Too large, keep as is

# Common English words with correct Russian pronunciation
WORD_DICT = {
    # Tech terms
    'python': 'пайтон', 'java': 'джава', 'javascript': 'джаваскрипт',
    'function': 'фанкшн', 'class': 'класс', 'method': 'метод',
    'file': 'файл', 'code': 'код', 'error': 'эррор', 'warning': 'ворнинг',
    'string': 'стринг', 'array': 'эррей', 'object': 'объект',
    'template': 'темплейт', 'model': 'модель', 'server': 'сервер',
    'client': 'клиент', 'database': 'датабейс', 'query': 'квери',
    'hello': 'хелло', 'world': 'ворлд', 'test': 'тест',
    'version': 'версия', 'update': 'апдейт', 'install': 'инсталл',
    'download': 'даунлоад', 'upload': 'аплоад', 'delete': 'делит',
    'create': 'криейт', 'edit': 'эдит', 'save': 'сейв',
    'open': 'оупен', 'close': 'клоуз', 'start': 'старт', 'stop': 'стоп',
    'run': 'ран', 'build': 'билд', 'debug': 'дебаг',
    'input': 'инпут', 'output': 'аутпут', 'result': 'резалт',
    'true': 'тру', 'false': 'фолс', 'null': 'налл', 'none': 'нан',
    'ok': 'окей', 'yes': 'йес', 'no': 'ноу',
    'api': 'эйпиай', 'url': 'юрл', 'http': 'эйчтитипи', 'https': 'эйчтитипиэс',
    'json': 'джейсон', 'xml': 'иксэмэль', 'html': 'эйчтиэмэль',
    'css': 'сиэсэс', 'sql': 'эскюэль',
    'gpu': 'джипиу', 'cpu': 'сипию', 'ram': 'рам', 'ssd': 'эсэсди',
    'ai': 'эйай', 'ml': 'эмэль', 'llm': 'эльэльэм',
    'token': 'токен', 'prompt': 'промпт', 'response': 'респонс',
    'whisper': 'виспер', 'silero': 'силеро', 'ollama': 'оллама',
}

# Phonetic digraphs/trigraphs (order matters - longer first)
PHONETIC_RULES = [
    ('tion', 'шн'), ('sion', 'жн'), ('ight', 'айт'),
    ('ough', 'оу'), ('ould', 'уд'), ('ould', 'уд'),
    ('th', 'з'), ('sh', 'ш'), ('ch', 'ч'), ('ph', 'ф'),
    ('wh', 'в'), ('ck', 'к'), ('ng', 'нг'), ('nk', 'нк'),
    ('qu', 'кв'), ('wr', 'р'), ('kn', 'н'), ('gh', ''),
    ('ee', 'и'), ('ea', 'и'), ('oo', 'у'), ('ou', 'ау'),
    ('ow', 'оу'), ('oi', 'ой'), ('oy', 'ой'),
    ('ai', 'ей'), ('ay', 'ей'), ('au', 'о'),
    ('ie', 'ай'), ('ei', 'ей'),
]

# Single letter mapping (fallback)
LETTER_MAP = {
    'a': 'а', 'b': 'б', 'c': 'к', 'd': 'д', 'e': 'е', 'f': 'ф', 'g': 'г',
    'h': 'х', 'i': 'и', 'j': 'дж', 'k': 'к', 'l': 'л', 'm': 'м', 'n': 'н',
    'o': 'о', 'p': 'п', 'q': 'к', 'r': 'р', 's': 'с', 't': 'т', 'u': 'а',
    'v': 'в', 'w': 'в', 'x': 'кс', 'y': 'и', 'z': 'з',
}

def transliterate_word(word):
    """Transliterate a single English word using phonetic rules"""
    lower = word.lower()
    
    # Check dictionary first
    if lower in WORD_DICT:
        result = WORD_DICT[lower]
        # Preserve capitalization
        if word[0].isupper():
            result = result[0].upper() + result[1:]
        return result
    
    # Apply phonetic rules
    result = lower
    for pattern, replacement in PHONETIC_RULES:
        result = result.replace(pattern, replacement)
    
    # Apply letter-by-letter for remaining
    final = []
    for char in result:
        final.append(LETTER_MAP.get(char, char))
    result = ''.join(final)
    
    # Preserve capitalization
    if word[0].isupper():
        result = result[0].upper() + result[1:]
    
    return result

def transliterate(text):
    """Transliterate English words in text to Russian phonetics"""
    # Find English words and transliterate them
    def replace_word(match):
        word = match.group()
        return transliterate_word(word)
    
    # Match sequences of English letters
    return re.sub(r'[a-zA-Z]+', replace_word, text)

def preprocess_for_tts(text):
    """Preprocess text for Russian TTS: convert numbers and transliterate English"""
    # Convert numbers to words
    text = re.sub(r'\d+', num_to_russian, text)
    # Transliterate remaining English letters
    text = transliterate(text)
    return text


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
            
            # Preprocess text for Russian TTS
            processed_text = preprocess_for_tts(line)
            print(f"Обработано: {processed_text[:50]}...", file=sys.stderr)
            
            # Generate audio
            audio = model.apply_tts(
                text=processed_text,
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
