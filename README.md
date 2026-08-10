# VenomESP - Detecção de Animais Peçonhentos

Sistema de detecção de cobras e escorpiões em tempo real utilizando YOLOv8 e ESP32-CAM.

## Estrutura do Projeto

```
latinoware-venomesp/
├── api/                            # Backend FastAPI
│   ├── main.py                     # Servidor de detecção
│   ├── requirements.txt            # Dependências Python
│   └── .env.example                # Variáveis de ambiente (exemplo)
├── esp32cam_firmware/              # Firmware ESP32-CAM
│   └── esp32cam_firmware.ino       # Código Arduino
├── pesos/                          # Pesos dos Modelos Treinados
│   ├── cobras_best.pt              # Pesos do modelo de cobras (YOLOv8n)
│   └── escorpioes_best.pt          # Pesos do modelo de escorpiões (YOLOv8n)
├── treinamento/                    # Códigos de Treinamento dos Modelos
│   ├── notebook_treinamento_cobras.ipynb     # Treino de Cobras (Kaggle)
│   └── notebook_treinamento_escorpioes.ipynb # Treino de Escorpiões (Roboflow)
├── webcam/                         # Detectores via webcam
│   ├── detector_cobras.py          # Detector de cobras (YOLOv8)
│   └── detector_escorpioes.py      # Detector de escorpiões (YOLOv8)
└── README.md                       # Este arquivo
```

## Requisitos

### API

```bash
cd api
pip install -r requirements.txt
cp .env.example .env  # Configure o token do Telegram
python main.py
```

### ESP32-CAM

1. Instale o Arduino IDE com suporte a ESP32
2. Abra `esp32cam_firmware/esp32cam_firmware.ino`
3. Selecione a placa "AI Thinker ESP32-CAM"
4. Compile e faça o upload

### Webcam (Teste Local)

```bash
cd webcam
pip install opencv-python ultralytics

# Detector de cobras
python detector_cobras.py --model /caminho/para/best.pt

# Detector de escorpiões
python detector_escorpioes.py
```

## Modelos

Os pesos treinados (arquivos `.pt`) estão disponíveis na pasta `pesos/`. Eles são necessários para a execução dos scripts de inferência.

### Métricas de Performance

| Modelo | mAP@50 | Precision | Recall | FPS Teórico |
|--------|--------|-----------|--------|-------------|
| Cobra | 91.9% | 91.0% | 85.0% | ~370 |
| Escorpião | 92.2% | 93.4% | 90.5% | ~294 |

## Endpoints da API

| Método | Rota | Descrição |
|--------|------|-----------|
| POST | `/detectar` | Envia imagem para detecção |
| GET | `/config` | Retorna username do bot Telegram |
| GET | `/status-dispositivo/{id}` | Verifica registro do dispositivo |

## Licença

Pesquisa acadêmica - LATINOWARE 2026
