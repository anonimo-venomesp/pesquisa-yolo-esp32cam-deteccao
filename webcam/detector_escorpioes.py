import os
import time
import subprocess
# Força o OpenCV a usar X11 em vez de Wayland (evita erro "Could not find the Qt platform plugin 'wayland'")
os.environ["QT_QPA_PLATFORM"] = "xcb"

import cv2
from ultralytics import YOLO

def play_alarm():
    # Gera um som de sirene rápido tipo "titititititititit"
    try:
        subprocess.Popen([
            'play', '-nq', 
            'synth', '0.05', 'sine', '1200', 
            'pad', '0', '0.05', 
            'repeat', '9'
        ], stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
    except:
        pass

def main():
    try:
        model = YOLO('best.pt')
        print("Modelo carregado com sucesso.")
    except Exception as e:
        print(f"Erro ao carregar o modelo: {e}")
        return

    cap = cv2.VideoCapture(0)
    
    # Tenta forçar a webcam a capturar em HD (1280x720) 
    # Isso aumenta o tamanho da imagem e dá muito mais detalhes reais para a IA achar os pequenos.
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)

    if not cap.isOpened():
        print("Erro: Não foi possível abrir a webcam.")
        return

    print("\nIniciando a câmera... Pressione a tecla 'q' para sair.")
    
    last_alarm_time = 0
    cooldown_seconds = 1  # Tempo de espera entre alarmes (para não tocar 30x por segundo)

    while True:
        ret, frame = cap.read()
        
        if not ret:
            print("Erro ao ler o frame da câmera. Tentando novamente...")
            continue

        # Faz a predição usando o YOLO
        # A confiança subiu de 0.65 para 0.78 para tentar barrar a estampa da sua camisa
        # sem prejudicar tanto a detecção dos escorpiões pequenos
        results = model.predict(frame, conf=0.78, iou=0.45, imgsz=1280, verbose=False)

        if results and len(results) > 0:
            annotated_frame = results[0].plot()
            
            # Se encontrou alguma caixa (detectou escorpião)
            if len(results[0].boxes) > 0:
                current_time = time.time()
                # Toca o som apenas se o tempo de cooldown já passou
                if current_time - last_alarm_time > cooldown_seconds:
                    play_alarm()
                    last_alarm_time = current_time
        else:
            annotated_frame = frame

        cv2.imshow('Detector de Escorpioes - Tempo Real', annotated_frame)

        # Se a tecla 'q' for pressionada, interrompe o loop
        if cv2.waitKey(1) & 0xFF == ord('q'):
            print("Encerrando programa...")
            break

    # Libera os recursos (webcam e janelas)
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
