# 🤖 DogRoverMQTT - BitDogLab 🤖

O DogRoverMQTT é a 7º atividade da Fase 2 do EmbarcaTech, o DogRover em si é uma ideia desenvolvida durante a 4º atividade da residência, que utilizava de um WEB Server par aestebelecer a comunicação via Wi-Fi. Agora, esse projeto é reimaginado para operar utilizando o protocolo de comunicação MQTT (Message Queuing Telemetry Transport), enviando mensagens para tópicos específicos utilizando um Broker MQTT que, nesse caso, opera em um Smartphone. 

Ainda sobre o DogRoverMQTT, tal qual o projeto anterior, segue sendo um projeto de temática de Robótica Móvel, sendo uma interface para simulação que controla e montiora um rover espacial de exploração em um outro planeta. Através do protocolo MQTT estabelecido, ele interpreta vários dados recebidos, gerando uma interface gráfica com uso dos periféricos presentes na placa, simulando a atuação do rover espacial em um planeta extraterrestre.

Desenvolvido por: Taylan Mayckon

---
## 📽️ Link do Video de Demonstração:
[YouTube]()
---

## 📌 **Funcionalidades Implementadas**

- FreeRTOS para geração de diferentes Tasks, fazendo com que o sistema execute várias tarefas de forma concorrente.
- Envio sem fio de informações: O sistema se conecta à um Broker MQTT, onde envia e recebe dados de tópicos específicos sem a necessidade de conexão física entre dispositivos.
- Mapa interativo: A matriz de LEDs 5x5 presente na BitDogLab ilustra um mapa que indica a posição do Rover, obstáculos e pontos de coleta
- Logs de informações: Informações são continuamente exibidas no Display OLED da BitDogLab e no servidor WEB, sendo eles a bateria atual do Rover, o modo de operação (WEB ou BitDogLab) e quantidade de coletas

---

## 🛠 **Recursos Utilizados**

- **FreeRTOS:** é um sistema operacional de código aberto e tempo real (RTOS) projetado para microcontroladores e dispositivos embarcados. Ele permite a criação de diferentes tarefas e faz o gerenciamento das mesmas para serem executadas de forma paralela.
- **Semáforos:** Para evitar situações de concorrência ou similares, onde foi utilizado Mutex para gerir corretamente o uso do ADC.
- **Protocólo MQTT:** É um protocólo de comunicação sem fio, onde o padrão de troca de mensagens é publish/subscriber.
- **Display OLED:** foi utilizado o ssd1306, que possui 128x64 pixels, para informações visuais dos dados lidos.
- **Matriz de LEDs Endereçáveis:** A BitDogLab possui uma matriz de 25 LEDs WS2812, que foi operada com o auxílio de uma máquina de estados para gerar o mapa interativo do sistema.
- **LED RGB:** Utilizado para sinalizar locais bloqueados e nível de bateria, através de sua intensidade.
- **Buzzers:** Emite sons para gerar alertas sonoros para as coletas e locais bloqueados.

---

## 📂 **Estrutura do Código**
```
📁 DogRoverMQTT/
│
├── 📁 lib/                                 # Bibliotecas utilizadas no projeto
│   ├── 📄 FreeRTOSConfig.h                 # Arquivos de configuração para o FreeRTOS
│   ├── 📄 font.h                           # Fonte utilizada no Display I2C
│   ├── 📄 led_matrix.c                     # Funções para manipulação da matriz de LEDs endereçáveis
│   ├── 📄 led_matrix.h                     # Cabeçalho para o led_matrix.c
│   ├── 📄 ssd1306.c                        # Funções que controlam o Display I2C
│   ├── 📄 ssd1306.h                        # Cabeçalho para o ssd1306.c
│   └── 📄 ws2812.pio                       # Máquina de estados para operar a matriz de LEDs endereçáveis
│
├── 📄 .gitignore                           # Arquivos e diretórios a serem ignorados pelo Git.
├── 📄 CMakeLists.txt                       # Configurações para compilar o código corretamente
├── 📄 lwipopts.h                           # Opções de configuração personalizadas para a pilha de rede LwIP (Lightweight IP)
├── 📄 lwipopts_examples_common.h           # Opções de configuração comuns da LwIP, fornecidas pelos exemplos do SDK do Raspberry Pi Pico
├── 📄 mbedtls_config.h                     # Arquivo de configuração para a biblioteca mbedTLS, usada para segurança (TLS/SSL)
├── 📄 mbedtls_config_examples_common.h     # Opções de configuração comuns da mbedTLS, fornecidas pelos exemplos do SDK do Raspberry Pi Pico
├── 📄 DogRoverMQTT.c                       # Código principal do projeto
└── 📄 README.md                            # Documentação do projeto.
```

---
