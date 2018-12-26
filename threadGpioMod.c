/**
 *  UNISAL 2018 - Sistemas Operacionais Embarcados - Linux Embarcado
 *  Atividade - Controle de GPIO via Sysfs
 *  Lucas Tamborrino
 * 
 */

/*TODO - Modificações
 *  -Ao pressionar o botão durante 2 segundos: Finaliza thread_led_ctrl e cria nova thread que executa mesmo controle porem 10 vezes mais rapida;
 * -Antes de mudar o controle de led, uma thread é disparada sinalizando a troca de thread, um terceiro led pisca 3 vezes
 
 */

// Inclusao de bibliotecas necessarias
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include <time.h>
#include <signal.h>
#include "gpioFileSys.h"


#define LED1	22
#define LED2	24
#define BTN1	25

#define LED3    23
#define DEBOUNCE    0.1 //s

// Variaveis de Referencia para Operacao
bool estado_botao = true; // botao eh ativado em false/0 -> pull-up!
bool muda_estado_pisca = false; // inicialmente nao mudamos estado de pisca_led
int led_control = 0;
pthread_mutex_t lock;

// Mutex para sinalizar finalização da thread
pthread_mutex_t endThread;
bool finalizaThread = false;

pthread_cond_t signal_cond;


pthread_t thread_id_hb;
pthread_t thread_id_led;
pthread_t thread_id_btn;
pthread_t thread_id_led_2;
pthread_t thread_id_sinal;

enum LED_ESTADOS {
  VEL0 = 0,
  VEL1,
  VEL2,
  VEL3,
  VEL4,
  VEL5,
  NUM_ESTADOS
};

int VEL_LED[NUM_ESTADOS] = {0, 250, 500, 750, 1000, 1};
int VEL_LED_2[NUM_ESTADOS] = {0, 25, 50, 75, 100, 1};

int estado_led = VEL0;

volatile bool terminateSignal = 0;

// Funcoes de Suporte para Thread de Aplicação
void *thread_heart_beat(void *arg);
void *thread_led_ctrl(void *arg);
void *thread_btn_read(void *arg);
void *thread_led_ctrl_2(void *arg);
void *thread_sinal(void *arg);

//*********************************************************************************************************

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num) 
{ 
    /* Reset handler to catch SIGINT next time. 
       Refer http://en.cppreference.com/w/c/program/signal */
    printf("\n Terminate \n"); 
    terminateSignal = 1;

} 

//*********************************************************************************************************

int main(int argc, char *argv[]) {
  // Unexporta os pinos
  GPIOUnexport(LED1);
  GPIOUnexport(LED2);
  GPIOUnexport(LED3);
  GPIOUnexport(BTN1);
  // Exporta agora, certo
  GPIOExport(LED1);
  GPIOExport(LED2);
  GPIOExport(LED3);
  GPIOExport(BTN1);
  // Define as direcoes de cada um
  GPIODirection(LED1, OUT);
  GPIODirection(LED2, OUT);
  GPIODirection(LED3, OUT);
  GPIODirection(BTN1, IN);
  
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
  // Trava momentaneamente nosso programa aqui
  while(true) {      
      if(terminateSignal)
      {
        // Quando a gente ta manipulando IOs... Eh bom "unexport" quando acaba
        GPIOUnexport(LED1);
        GPIOUnexport(LED2);
        GPIOUnexport(BTN1);
        return 0;
      }  
    }   
  return 0;
  
}

//*********************************************************************************************************

void *thread_heart_beat(void *arg) {
  while(true) {
    usleep(0.5 * 1000 * 1000);
    GPIOWrite(LED1, HIGH);
    usleep(0.5 * 1000 * 1000);
    GPIOWrite(LED1, LOW);
  }

  return NULL;
}

//*********************************************************************************************************

void *thread_led_ctrl(void *arg) {
    
    bool finalizaFlag = false;
    while(true) {
        usleep(0.100 * 1000 * 1000);
        // Soh precisa da trava/destrava pra leitura de estado pra mudar o comportamento
        pthread_mutex_lock(&lock);
        if (muda_estado_pisca) {
            printf("1 - Muda Estado do Led\n");
        if (estado_led == VEL5) {
            estado_led = VEL0;
        } else {
            estado_led++;
        }
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
        case VEL4:
            usleep(VEL_LED[estado_led] * 1000);
            GPIOWrite(LED2, HIGH);
            usleep(VEL_LED[estado_led] * 1000);
            GPIOWrite(LED2, LOW);
        break;
        case VEL5:
            GPIOWrite(LED2, HIGH);
        break;
        }
        
    
        //Verifica comando para finalizar thread
        pthread_mutex_lock(&endThread);
        if(finalizaThread)
        {
            finalizaFlag = true;
            finalizaThread = false;
            GPIOWrite(LED2, LOW);
            pthread_cond_wait(&signal_cond, &endThread);
            
        }
        pthread_mutex_unlock(&endThread);
        if(finalizaFlag)
        {
            if (pthread_create(&thread_id_led_2, NULL, thread_led_ctrl_2, NULL) != 0) {
                fprintf(stderr, "Falha na criacao da thread led control 2 \n");
            }
            printf("Finalizando thread thread_led_ctrl\n");
            pthread_exit(NULL);
        }
    }

    return NULL;
}

//*********************************************************************************************************

void *thread_led_ctrl_2(void *arg) {
    
    bool finalizaFlag = false;
  while(true) {
      usleep(0.100 * 1000 * 1000);
    // Soh precisa da trava/destrava pra leitura de estado pra mudar o comportamento
    pthread_mutex_lock(&lock);
    if (muda_estado_pisca) {
        printf("2 - Muda Estado do Led\n");
      if (estado_led == VEL5) {
        estado_led = VEL0;
      } else {
      	estado_led++;
      }
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
      case VEL4:
        usleep(VEL_LED_2[estado_led] * 1000);
        GPIOWrite(LED2, HIGH);
        usleep(VEL_LED_2[estado_led] * 1000);
        GPIOWrite(LED2, LOW);
      break;
      case VEL5:
        GPIOWrite(LED2, HIGH);
      break;
    }
    
    
    //Verifica comando para finalizar thread
    pthread_mutex_lock(&endThread);
    if(finalizaThread)
    {
        finalizaFlag = true;
        finalizaThread = false;
        GPIOWrite(LED2, LOW);
        pthread_cond_wait(&signal_cond, &endThread);
        
    }
    pthread_mutex_unlock(&endThread);
    if(finalizaFlag)
    {
        if (pthread_create(&thread_id_led, NULL, thread_led_ctrl, NULL) != 0) {
            fprintf(stderr, "Falha na criacao da thread de led control 1  \n");
            
        }
        printf("Finalizando thread thread_led_ctrl_2\n");
        pthread_exit(NULL);
    }
    
  }

  return NULL;
}
//*********************************************************************************************************

void *thread_sinal(void *arg)
{
    int i;
    for(i = 0; i<3; i++)
    {
        usleep(0.1 * 1000 * 1000);
        GPIOWrite(LED3, HIGH);
        usleep(0.1 * 1000 * 1000);
        GPIOWrite(LED3, LOW);
    }
    pthread_cond_signal(&signal_cond);
    pthread_exit(NULL);
}

//*********************************************************************************************************

void *thread_btn_read(void *arg) {
    clock_t actualTime, oldTime;
    double tempo_percorrido;
    bool old_estado_botao = false;
    
    
  while(true) {
    usleep(0.100 * 1000 * 1000);
    // ve se da pra mexer na variavel de estado do botao
    estado_botao = (GPIORead(BTN1) == 0 ? false : true);
    
    switch(!estado_botao)
    {
        //Pega tempo de botão pressionado
        case false:
            //Borda de descida
            if(old_estado_botao == true)
            {
                
                //actualTime = time(NULL);
                actualTime = clock();
                //tempo_percorrido = difftime(actualTime, oldTime);
                tempo_percorrido = (double)(actualTime - oldTime)/CLOCKS_PER_SEC;
                printf("Tempo percorrido: %f\n", tempo_percorrido);
            }
            old_estado_botao = false;
        break;
        case true:
            //borda de subida
            if(old_estado_botao == false)
            {
                printf("Botao pressionado!\n");
                //oldTime = time(NULL);
                oldTime = clock();
                
            }
            old_estado_botao = true;
        break;
        default:
        break;
    }
    
    
    if(tempo_percorrido > DEBOUNCE)
    {
        if(tempo_percorrido < 2)
        {
            pthread_mutex_lock(&lock);
            muda_estado_pisca = true;
            pthread_mutex_unlock(&lock); 
            
        }else if(tempo_percorrido > 2 && tempo_percorrido < 5){
            printf("Finaliza Thread Atual\n");
            pthread_mutex_lock(&endThread);
            finalizaThread = true;
            pthread_mutex_unlock(&endThread);
            if (pthread_create(&thread_id_sinal, NULL, thread_sinal, NULL) != 0) {
                fprintf(stderr, "Falha na criacao da thread de Sinalização  \n");
            }   
            pthread_join(thread_id_sinal, 0);
        }
        
    }
    tempo_percorrido = 0;
    
  }
  return NULL;
}
