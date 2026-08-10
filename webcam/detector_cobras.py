"""
╔══════════════════════════════════════════════════════════════╗
║          🐍  DETECTOR DE COBRAS - WEBCAM EM TEMPO REAL       ║
║          Modelo: YOLOv8 | Dataset: Roboflow Snake Detection  ║
║          Pressione 'Q' para sair | 'S' para salvar screenshot║
╚══════════════════════════════════════════════════════════════╝
"""

import os
import sys
import time
import subprocess
import argparse
from pathlib import Path
from datetime import datetime

# Força X11 em sistemas com Wayland (evita erro do Qt/Wayland)
os.environ["QT_QPA_PLATFORM"] = "xcb"

import cv2
import numpy as np

# ─────────────────────────────────────────────
# CAMINHOS DOS MODELOS (prioridade: melhor → backup)
# ─────────────────────────────────────────────
BASE_DIR = Path(__file__).parent
MODEL_PATHS = [
    BASE_DIR / "resultados_treino" / "yolov8_cobras" / "weights" / "best.pt", # Modelo treinado mais recente
    BASE_DIR / "runs" / "run1" / "weights" / "best.pt",                       # Melhor do run1 (backup)
    BASE_DIR / "models" / "best.pt",                                          # Cópia em models/ (backup)
]

SCREENSHOTS_DIR = BASE_DIR / "screenshots"

# ─────────────────────────────────────────────
# CONFIGURAÇÕES DE DETECÇÃO
# ─────────────────────────────────────────────
CONF_THRESHOLD   = 0.50   # Confiança mínima para detectar (0.0-1.0)
IOU_THRESHOLD    = 0.45   # Limiar IOU para NMS
IMG_SIZE         = 640    # Tamanho de entrada do modelo (640 ou 1280)
CAMERA_INDEX     = 0      # Índice da webcam (0 = padrão)
ALARM_COOLDOWN   = 1.5    # Segundos entre alarmes sonoros

# Cores e estilo da HUD
COLOR_DETECTED   = (0, 0, 220)       # Vermelho vivo (BGR) - cobra detectada
COLOR_SAFE       = (0, 200, 60)      # Verde - sem cobra
COLOR_BOX        = (0, 30, 200)      # Cor da bounding box
COLOR_TEXT_BG    = (10, 10, 10)      # Fundo do texto
COLOR_WHITE      = (255, 255, 255)
COLOR_YELLOW     = (0, 200, 255)
COLOR_CYAN       = (255, 220, 0)


def find_model() -> Path | None:
    """Procura o modelo treinado nos caminhos configurados."""
    for path in MODEL_PATHS:
        if path.exists():
            return path
    return None


def play_alarm():
    """Toca um alarme sonoro rápido (requer sox instalado)."""
    try:
        subprocess.Popen(
            ["play", "-nq",
             "synth", "0.08", "sine", "1400",
             "pad", "0", "0.05",
             "repeat", "5"],
            stderr=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
        )
    except FileNotFoundError:
        # sox não instalado — tenta beep do sistema
        try:
            subprocess.Popen(["beep"], stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        except Exception:
            pass


def save_screenshot(frame: np.ndarray):
    """Salva o frame atual como imagem PNG com timestamp."""
    SCREENSHOTS_DIR.mkdir(exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:19]
    filepath = SCREENSHOTS_DIR / f"cobra_detectada_{ts}.png"
    cv2.imwrite(str(filepath), frame)
    print(f"  📸 Screenshot salvo: {filepath.name}")
    return filepath


def draw_hud(frame: np.ndarray, detected: bool, num_detections: int,
             fps: float, conf_threshold: float) -> np.ndarray:
    """Desenha a interface HUD sobre o frame."""
    h, w = frame.shape[:2]

    # ── Barra superior de status ──────────────────────────────────
    bar_h = 54
    overlay = frame.copy()
    cv2.rectangle(overlay, (0, 0), (w, bar_h), (15, 15, 15), -1)
    cv2.addWeighted(overlay, 0.75, frame, 0.25, 0, frame)

    # Título
    cv2.putText(frame, "🐍 DETECTOR DE COBRAS", (12, 20),
                cv2.FONT_HERSHEY_DUPLEX, 0.55, COLOR_CYAN, 1, cv2.LINE_AA)

    # Status de detecção
    if detected:
        status_text = f"⚠  COBRA DETECTADA!  ({num_detections})"
        status_color = COLOR_DETECTED
    else:
        status_text = "✔  AMBIENTE SEGURO"
        status_color = COLOR_SAFE

    cv2.putText(frame, status_text, (12, 44),
                cv2.FONT_HERSHEY_DUPLEX, 0.65, status_color, 1, cv2.LINE_AA)

    # FPS (canto superior direito)
    fps_text = f"FPS: {fps:4.1f}"
    fps_size = cv2.getTextSize(fps_text, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)[0]
    cv2.putText(frame, fps_text, (w - fps_size[0] - 10, 20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, COLOR_YELLOW, 1, cv2.LINE_AA)

    # Confiança (canto superior direito, abaixo do FPS)
    conf_text = f"Conf: {conf_threshold:.0%}"
    conf_size = cv2.getTextSize(conf_text, cv2.FONT_HERSHEY_SIMPLEX, 0.48, 1)[0]
    cv2.putText(frame, conf_text, (w - conf_size[0] - 10, 40),
                cv2.FONT_HERSHEY_SIMPLEX, 0.48, COLOR_YELLOW, 1, cv2.LINE_AA)

    # ── Barra inferior de controles ────────────────────────────────
    ctrl_y = h - 10
    cv2.putText(frame, "[ Q ] Sair   [ S ] Screenshot   [ + / - ] Confiança",
                (12, ctrl_y), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (180, 180, 180), 1, cv2.LINE_AA)

    # ── Alerta pulsante quando cobra detectada ─────────────────────
    if detected:
        pulse = int((time.time() * 4) % 2)  # pisca ~4x/s
        if pulse:
            border_thickness = 6
            cv2.rectangle(frame, (0, 0), (w - 1, h - 1), COLOR_DETECTED, border_thickness)

    return frame


def draw_custom_boxes(frame: np.ndarray, results) -> tuple[np.ndarray, int]:
    """Desenha bounding boxes customizadas sobre o frame."""
    num_boxes = 0

    if not results or len(results) == 0:
        return frame, 0

    boxes = results[0].boxes
    if boxes is None or len(boxes) == 0:
        return frame, 0

    num_boxes = len(boxes)

    for box in boxes:
        x1, y1, x2, y2 = map(int, box.xyxy[0].tolist())
        conf = float(box.conf[0])
        cls  = int(box.cls[0])

        # Bounding box com bordas duplas para visual premium
        cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 0, 0), 4)        # sombra preta
        cv2.rectangle(frame, (x1, y1), (x2, y2), COLOR_BOX, 2)         # borda colorida

        # Label com fundo
        label = f"Cobra  {conf:.0%}"
        (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_DUPLEX, 0.55, 1)
        lx, ly = x1, y1 - 8
        if ly - th - 4 < 0:
            ly = y2 + th + 8  # move para baixo se sair da tela

        cv2.rectangle(frame, (lx - 2, ly - th - 6), (lx + tw + 4, ly + 2),
                      COLOR_BOX, -1)
        cv2.putText(frame, label, (lx, ly - 2),
                    cv2.FONT_HERSHEY_DUPLEX, 0.55, COLOR_WHITE, 1, cv2.LINE_AA)

        # Cantos decorativos (estilo militar/tático)
        corner_len = 14
        thickness  = 2
        cr = COLOR_YELLOW
        # topo-esquerdo
        cv2.line(frame, (x1, y1), (x1 + corner_len, y1), cr, thickness)
        cv2.line(frame, (x1, y1), (x1, y1 + corner_len), cr, thickness)
        # topo-direito
        cv2.line(frame, (x2, y1), (x2 - corner_len, y1), cr, thickness)
        cv2.line(frame, (x2, y1), (x2, y1 + corner_len), cr, thickness)
        # baixo-esquerdo
        cv2.line(frame, (x1, y2), (x1 + corner_len, y2), cr, thickness)
        cv2.line(frame, (x1, y2), (x1, y2 - corner_len), cr, thickness)
        # baixo-direito
        cv2.line(frame, (x2, y2), (x2 - corner_len, y2), cr, thickness)
        cv2.line(frame, (x2, y2), (x2, y2 - corner_len), cr, thickness)

    return frame, num_boxes


def main():
    parser = argparse.ArgumentParser(
        description="🐍 Detector de Cobras em Tempo Real — Webcam + YOLOv8"
    )
    parser.add_argument("--conf",   type=float, default=CONF_THRESHOLD, help="Confiança mínima (0.0–1.0)")
    parser.add_argument("--iou",    type=float, default=IOU_THRESHOLD,  help="Limiar IOU para NMS")
    parser.add_argument("--imgsz",  type=int,   default=IMG_SIZE,        help="Tamanho de entrada do modelo")
    parser.add_argument("--cam",    type=int,   default=CAMERA_INDEX,    help="Índice da câmera (default: 0)")
    parser.add_argument("--model",  type=str,   default=None,            help="Caminho manual para o .pt")
    parser.add_argument("--no-alarm", action="store_true",               help="Desativa o alarme sonoro")
    args = parser.parse_args()

    conf_threshold = args.conf

    # ── Carrega o modelo ──────────────────────────────────────────
    print("\n" + "═" * 60)
    print("  🐍  DETECTOR DE COBRAS — YOLOV8 — TEMPO REAL")
    print("═" * 60)

    try:
        from ultralytics import YOLO
    except ImportError:
        print("\n❌  Ultralytics não encontrado. Instale com:")
        print("    pip install ultralytics\n")
        sys.exit(1)

    model_path = Path(args.model) if args.model else find_model()
    if model_path is None or not model_path.exists():
        print(f"\n❌  Modelo não encontrado. Caminhos tentados:")
        for p in MODEL_PATHS:
            print(f"    • {p}")
        print("\n  Passe o caminho manualmente com: --model /caminho/para/best.pt")
        sys.exit(1)

    print(f"\n  📦  Modelo  : {model_path}")
    print(f"  🎯  Confiança: {conf_threshold:.0%}")
    print(f"  📐  ImgSize : {args.imgsz}px")
    print(f"  🎥  Câmera  : /dev/video{args.cam}")

    model = YOLO(str(model_path))
    print("\n  ✅  Modelo carregado com sucesso!\n")

    # ── Abre a câmera ─────────────────────────────────────────────
    cap = cv2.VideoCapture(args.cam)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    if not cap.isOpened():
        print(f"❌  Não foi possível abrir a câmera (índice {args.cam}).")
        print("   Tente: --cam 1  ou  --cam 2")
        sys.exit(1)

    real_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    real_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"  📷  Resolução da câmera: {real_w}×{real_h}")
    print(f"\n  Controles:")
    print(f"    Q          → Sair")
    print(f"    S          → Salvar screenshot")
    print(f"    + / =      → Aumentar confiança (+5%)")
    print(f"    -          → Diminuir confiança (-5%)")
    print(f"\n  Aguardando frames...\n")

    last_alarm_time  = 0
    prev_frame_time  = time.time()
    fps              = 0.0
    frame_count      = 0
    total_detections = 0
    session_start    = time.time()

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("⚠  Erro ao ler frame. Tentando novamente...")
                time.sleep(0.05)
                continue

            frame_count += 1

            # ── Inferência ───────────────────────────────────────
            results = model.predict(
                frame,
                conf    = conf_threshold,
                iou     = args.iou,
                imgsz   = args.imgsz,
                verbose = False,
                stream  = False,
            )

            # ── Bounding boxes customizadas ──────────────────────
            frame, num_detections = draw_custom_boxes(frame, results)
            detected = num_detections > 0

            if detected:
                total_detections += 1

            # ── Alarme sonoro ────────────────────────────────────
            if detected and not args.no_alarm:
                now = time.time()
                if now - last_alarm_time > ALARM_COOLDOWN:
                    play_alarm()
                    last_alarm_time = now

            # ── Cálculo de FPS (média móvel) ─────────────────────
            now = time.time()
            fps = 0.9 * fps + 0.1 * (1.0 / max(now - prev_frame_time, 1e-6))
            prev_frame_time = now

            # ── HUD ──────────────────────────────────────────────
            frame = draw_hud(frame, detected, num_detections, fps, conf_threshold)

            # ── Exibe o frame ────────────────────────────────────
            cv2.imshow("Detector de Cobras 🐍", frame)

            # ── Teclas ───────────────────────────────────────────
            key = cv2.waitKey(1) & 0xFF

            if key == ord("q") or key == 27:           # Q ou ESC → sair
                break
            elif key == ord("s"):                       # S → screenshot
                save_screenshot(frame)
            elif key in (ord("+"), ord("=")):           # + → aumentar conf
                conf_threshold = min(1.0, conf_threshold + 0.05)
                print(f"  🎯 Confiança ajustada para: {conf_threshold:.0%}")
            elif key == ord("-"):                       # - → diminuir conf
                conf_threshold = max(0.05, conf_threshold - 0.05)
                print(f"  🎯 Confiança ajustada para: {conf_threshold:.0%}")

    except KeyboardInterrupt:
        print("\n\n  ⚠  Interrompido pelo usuário (Ctrl+C).")

    finally:
        cap.release()
        cv2.destroyAllWindows()

        elapsed = time.time() - session_start
        print("\n" + "═" * 60)
        print("  📊  RESUMO DA SESSÃO")
        print("═" * 60)
        print(f"  ⏱  Tempo de sessão  : {elapsed:.1f}s")
        print(f"  🖼  Frames analisados: {frame_count}")
        print(f"  🐍  Frames c/ cobra  : {total_detections}")
        print(f"  📷  FPS médio final  : {fps:.1f}")
        print("═" * 60 + "\n")


if __name__ == "__main__":
    main()
