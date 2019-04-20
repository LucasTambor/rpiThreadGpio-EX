/**
 *  UNISAL 2019 - Sistemas Operacionais Embarcados - Linux Embarcado
 *  Atividade - Controle de GPIO em threads via Sysfs
 *  Lucas Tamborrino
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include "gpioFileSys.h"
#include <syslog.h>


#define LED1	23
#define LED2	24
#define BTN 	25
#define DEBOUNCE    0.1 //s

// Variaveis de Referencia para Operacao
bool muda_estado_pisca = false; // inicialmente nao mudamos estado de pisca_led
int led_control = 0;
pthread_mutex_t lock;


pthread_t thread_id_hb;
pthread_t thread_id_led;
pthread_t thread_id_btn;

enum LED_ESTADOS {
    VEL0 = 0,
    VEL1,
    VEL2,
    VEL3,
    VEL4,
    NUM_ESTADOS
};

int VEL_LED[NUM_ESTADOS] = {0, 1000, 200, 100, 1};

int estado_led = VEL0;

volatile bool terminateSignal = 0;

// Funcoes de Suporte para Thread de Aplicação
void *thread_heart_beat(void *arg);
void *thread_led_ctrl(void *arg);
void *thread_btn_read(void *arg);

//*********************************************************************************************************
// Função para lidar com sinalização de eventos
void sigintHandler(int sig_num) 
{ 
    syslog(LOG_NOTICE, "\n Terminate \n"); 
    terminateSignal = true;
} 

//*********************************************************************************************************

int main(int argc, char *argv[]) {
    // Unexporta os pinos
    GPIOUnexport(LED1);
    GPIOUnexport(LED2);
    GPIOUnexport(BTN);
    // Exporta agora, certo
    GPIOExport(LED1);
    GPIOExport(LED2);
    GPIOExport(BTN);
    // Define as direcoes de cada um
    GPIODirection(LED1, OUT);
    GPIODirection(LED2, OUT);
    GPIODirection(BTN, IN);
  
        // Inicializa threads e mutexes
    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "Falha na iniciliazacao do Mutex \n");
        return 1;
    }
  
    if (pthread_create(&thread_id_hb, NULL, thread_heart_beat, NULL) != 0) {
        fprintf(stderr, "Falha na criacao da thread de heart beat \n");
        return 1;
    } 

    if (pthread_create(&thread_id_led, NULL, thread_led_ctrl, NULL) != 0) {
        fprintf(stderr, "Falha na criacao da thread de led control 1 \n");
        return 1;
    }
    
    if (pthread_create(&thread_id_btn, NULL, thread_btn_read, NULL) != 0) {
        fprintf(stderr, "Falha na criacao da thread de botao \n");
        return 1;
    }
    
    signal(SIGINT, sigintHandler); 
    
    while(!terminateSignal) 
    {      


    }   
    // join nas threads
    pthread_join(thread_id_hb, NULL);
    pthread_join(thread_id_led, NULL);
    pthread_join(thread_id_btn, NULL);

    // Limpa mutex
    pthread_mutex_destroy(&lock);

    // Unexporta pinos ao final da execução
    GPIOUnexport(LED1);
    GPIOUnexport(LED2);
    GPIOUnexport(BTN);
   
    return 0;
    
}

//*********************************************************************************************************

void *thread_heart_beat(void *arg) {
  while(!terminateSignal) {
    usleep(0.5 * 1000 * 1000);
    GPIOWrite(LED1, HIGH);
    usleep(0.5 * 1000 * 1000);
    GPIOWrite(LED1, LOW);
  }

  return NULL;
}

//*********************************************************************************************************

void *thread_led_ctrl(void *arg) {
    
    while(!terminateSignal) {
        usleep(0.100 * 1000 * 1000);
        // Soh precisa da trava/destrava pra leitura de estado pra mudar o comportamento
        pthread_mutex_lock(&lock);
        if (muda_estado_pisca) {
            syslog(LOG_NOTICE, "Muda Estado do Led\n");
            if (estado_led == VEL4) {
                estado_led = VEL0;
            } else {
                estado_led++;
                
            }
            syslog(LOG_NOTICE, "Estado LED: %d\n", estado_led);
            muda_estado_pisca = false;
        }
        pthread_mutex_unlock(&lock);
        
        switch(estado_led) {
        case VEL0:
            GPIOWrite(LED2, LOW);
        break;
        case VEL1:
        case VEL2:
        case VEL3:
            usleep(VEL_LED[estado_led] * 1000);
            GPIOWrite(LED2, HIGH);
            usleep(VEL_LED[estado_led] * 1000);
            GPIOWrite(LED2, LOW);
        break;
        case VEL4:
            GPIOWrite(LED2, HIGH);
        break;
        }

    }
    return NULL;
}


//*********************************************************************************************************

void *thread_btn_read(void *arg) {
    bool old_estado_botao = false;
    bool estado_botao;
    
    
  while(!terminateSignal) {
    usleep(0.100 * 1000 * 1000);
    estado_botao = (GPIORead(BTN) == 0 ? true : false);
    
    

    // checa mudança de estado
    if(estado_botao != old_estado_botao)
    {   
        // Debouce do botão (~50ms)
        usleep(DEBOUNCE * 1000*1000);
        // borda de subida
        if(estado_botao)
        {
            syslog(LOG_NOTICE,"Botão Pressionado!\n");
            pthread_mutex_lock(&lock);
            muda_estado_pisca = true;
            pthread_mutex_unlock(&lock);
            
        }else{ //borda de descida

        }

        old_estado_botao = estado_botao;
    }
    
  }
  return NULL;
}
