import os
from transformers import Wav2Vec2ForCTC, AutoProcessor

# Russian model (Wav2Vec2-Large-XLSR-53-Russian)
MODEL_ID = "jonatasgrosman/wav2vec2-large-xlsr-53-russian"
TARGET_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "models", "wav2vec2-russian")

def download_model():
    print(f"Downloading model {MODEL_ID} to {TARGET_DIR}...")
    
    if not os.path.exists(TARGET_DIR):
        os.makedirs(TARGET_DIR)
        
    try:
        # Try loading processor first
        print("Loading processor...")
        processor = AutoProcessor.from_pretrained(MODEL_ID)
        processor.save_pretrained(TARGET_DIR)
        
        print("Loading model...")
        model = Wav2Vec2ForCTC.from_pretrained(MODEL_ID)
        model.save_pretrained(TARGET_DIR)
        
        print("Model downloaded successfully.")
    except Exception as e:
        print(f"Error downloading {MODEL_ID}: {e}")
        print("Attempting fallback to facebook/mms-1b-all (ASR model)...")
        # Fallback
        try:
             fallback_id = "facebook/mms-1b-all"
             fallback_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "models", "mms-1b-all")
             if not os.path.exists(fallback_dir):
                 os.makedirs(fallback_dir)
             
             processor = AutoProcessor.from_pretrained(fallback_id)
             processor.save_pretrained(fallback_dir)
             model = Wav2Vec2ForCTC.from_pretrained(fallback_id)
             model.save_pretrained(fallback_dir)
             print(f"Fallback model {fallback_id} downloaded.")
        except Exception as e2:
             print(f"Fallback failed: {e2}")

if __name__ == "__main__":
    download_model()
