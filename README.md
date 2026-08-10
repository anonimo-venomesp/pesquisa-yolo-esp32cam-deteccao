# VenomESP - Detecção em Tempo Real de Animais Peçonhentos

Sistema embarcado para localizar cobras e escorpiões instantaneamente utilizando visão computacional (YOLOv8n) e hardware de baixo custo (ESP32-CAM). O projeto foca em atender áreas de risco através de uma solução acessível, rápida e precisa.

## Arquitetura do Projeto

O sistema funciona integrando o microcontrolador ESP32-CAM, que fotografa o ambiente continuamente, com um servidor FastAPI. O servidor recebe as imagens e executa a rede neural YOLOv8n treinada para identificar as ameaças. Quando um animal é localizado, a plataforma envia alertas imediatos via Telegram.

## Resultados e Métricas

Os testes atestam a viabilidade de rodar modelos leves de visão computacional vinculados a hardwares limitados. A rede processa os dados com alta acurácia, conforme registrado nos ensaios oficiais descritos abaixo.

**Cobras:**
- Precisão: 91.0%
- Revocação: 85.0%
- mAP@50: 91.9%
- FPS Teórico: ~370

**Escorpiões:**
- Precisão: 93.4%
- Revocação: 90.5%
- mAP@50: 92.2%
- FPS Teórico: ~294

## Organização do Repositório

O código fonte está dividido nos seguintes módulos principais.

### 1. API (Backend)
O diretório `api` hospeda o servidor escrito em FastAPI. Ele gerencia as requisições HTTP, processa as imagens que chegam do hardware e aciona as mensagens do bot.
- Instale os requisitos através do arquivo `requirements.txt`.
- Configure o token editando o arquivo `.env.example`.
- Inicie o servidor rodando o arquivo `main.py`.

### 2. Firmware do Microcontrolador
A pasta `esp32cam_firmware` contém o código em C++ estruturado para gravar na placa AI Thinker ESP32-CAM usando o Arduino IDE. O script conecta o dispositivo na rede Wi-Fi e define a frequência das capturas.

### 3. Modelos Treinados
O diretório `pesos` guarda os arquivos `.pt` originais. Os pesos das cobras (`cobras_best.pt`) e dos escorpiões (`escorpioes_best.pt`) pesam aproximadamente 6MB cada e já estão configurados para uso imediato no servidor ou na webcam.

### 4. Metodologia de Treinamento
Para garantir total transparência científica, a pasta `treinamento` oferece os códigos originais (arquivos `.ipynb`) que utilizamos para ensinar a rede neural. O treinamento das cobras baseia-se num banco do Kaggle, enquanto os escorpiões foram processados via Roboflow. O código demonstra o passo a passo de configuração e o treino exato de 100 épocas (cobras) e 50 épocas (escorpiões).

### 5. Testes Locais
O diretório `webcam` possui códigos em Python para avaliar os pesos usando a câmera do seu computador. É ideal para validar o funcionamento da rede neural sem necessitar da instalação do microcontrolador na rede local.

## Guia de Instalação Rápida

Comece preparando o servidor central:

```bash
cd api
pip install -r requirements.txt
cp .env.example .env
python main.py
```

Para realizar o teste pela câmera local, garanta que os pesos estão mapeados corretamente no script:

```bash
cd webcam
pip install opencv-python ultralytics
python detector_cobras.py --model ../pesos/cobras_best.pt
```
