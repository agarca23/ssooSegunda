/*POWERLIFTING*/
/*Pablo Pérez López*/
/*Pablo González de la Iglesia*/
/*Andrés García Álvarez*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>

#define NUMJUECES 2
#define NUMATLETAS 10

int numeroDeAtletas;
int numeroDeJueces;
int colaJuez[10];/*La cola guarda una referencia en cada posicion al coche que la ocupa*/
int colaFuente[10];
int atletasIntroducidos=0;
int mejoresPuntuaciones[3]={0,0,0};
int mejoresAtletas[3]={100,100,100};
int fuenteOcupada;


void estadisticas(int a);
void nuevoCompetidorATarima1(int a);
void nuevoCompetidorATarima2(int a);
void inicializarAtleta(int posicionPuntero, int numeroAtleta, int necesita_beber, int ha_competido, int tarimaAsignada);
void resetearAtleta(int posicionPuntero);
void writeLogMessage(char *id,char *msg);
void *accionesJuez(void* manejadora);
int calculoAleatorio(int max, int min);
void *accionesFuente(void *manejadora);
void *accionesAtleta(void* manejadora);
void finalizarCompeticion(int a);


struct atletas{
	pthread_t hiloAtleta;
	int numeroAtleta;
	int necesita_beber;
	int ha_competido;
	int tarimaAsignada;
};

struct atletas *punteroAtletas;



/*Mutex para los jueces y sus colas*/
pthread_mutex_t controladorEntrada;/*Controla que dos atletas no entren al mismo tiempo*/
pthread_mutex_t controladorColaJueces;/*controla que no entren a la cola dos atletas y  evita que tengan la misma posicion en la cola*/
pthread_mutex_t controladorColaFuente;
pthread_mutex_t controladorFuente;
pthread_mutex_t controladorPodium;
pthread_mutex_t controladorEscritura;/*controlara que no mas de dos atletas o jueces intenten escribir en el fichero*/
pthread_cond_t fuenteCond;

FILE *logFile;
char* logFileName ="registroTiempos.log";


int main(int argc, char *argv[]){

	if(argc==1){
		numeroDeAtletas=10;
		numeroDeJueces=2;
	}else if(argc==3){
		numeroDeAtletas=atoi(argv[1]);
		numeroDeJueces=atoi(argv[2]);
	}else{
		printf("Argumentos incorrectos\n");

	}
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGINT,finalizarCompeticion)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGPIPE,estadisticas)==SIG_ERR){
		exit(-1);
	}

	/*Declaramos los hilos juez y fuente*/
	pthread_t juez1;
	pthread_t juez2;
	pthread_t fuente;


	/*Inicializamos los semaforos*/
	pthread_mutex_init(&controladorEntrada, NULL);
	pthread_mutex_init(&controladorColaJueces,NULL);
	pthread_mutex_init(&controladorColaFuente,NULL);
	pthread_mutex_init(&controladorFuente,NULL);
	pthread_mutex_init(&controladorPodium,NULL);
	pthread_mutex_init(&controladorEscritura,NULL);
	if(pthread_cond_init(&fuenteCond,NULL)!=0){
		exit(-1);
	}

	/*Reservamos memoria para los atletas*/
	punteroAtletas = (struct atletas*)malloc(sizeof(struct atletas)*NUMATLETAS);
   	int i,j;
   	for(i=0;i<NUMATLETAS;i++){
    	punteroAtletas[i].numeroAtleta=0;
		punteroAtletas[i].necesita_beber=0;
		punteroAtletas[i].ha_competido=0;
		punteroAtletas[i].tarimaAsignada=0;
  	}

   	/*Inicializamos la cola, como no tenemos 100 atletas será nuestro valor de control*/

	for(i=0;i<NUMATLETAS;i++){
		colaJuez[i]=100;
		colaFuente[i]=100;
	}

   	/*Lanzamos losjueces*/
	i=1;
	pthread_create(&juez1, NULL, accionesJuez,(void*)&i);
	j=2;	
	pthread_create(&juez2, NULL, accionesJuez,(void*)&j);

	/*Lanzamos la fuente*/
	pthread_create(&fuente,NULL, accionesFuente,(void*)&i);

		/*inicializar el archivo, lo creo si no existe y sino lo sobreescribo*/
	logFile = fopen(logFileName,"w");
	if(logFile==NULL){
		char *err=strerror(errno);
		printf("%s", err);
		fclose(logFile);
	}
	writeLogMessage("","bienvenido a powerLiftingC\n");

	while(1){
		pause();
	}


}

void nuevoCompetidorATarima1(int a){
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}

	pthread_mutex_lock(&controladorEntrada);
	if(atletasIntroducidos<NUMATLETAS){
		atletasIntroducidos++;
		int i,j,k;

		for(i=0;i<NUMATLETAS;i++){
			if(punteroAtletas[i].numeroAtleta==0){
				inicializarAtleta(i,atletasIntroducidos,0,0,1);
		
				pthread_create(&punteroAtletas[i].hiloAtleta,NULL,accionesAtleta,(void*)&punteroAtletas[i].numeroAtleta);
				
				break;
			}
		}
	}
	pthread_mutex_unlock(&controladorEntrada);

	
}

void nuevoCompetidorATarima2(int a){
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	pthread_mutex_lock(&controladorEntrada);
	if(atletasIntroducidos<NUMATLETAS){
		atletasIntroducidos++;
		int i,j;

		for(i=0;i<NUMATLETAS;i++){
			if(punteroAtletas[i].numeroAtleta==0){
				inicializarAtleta(i,atletasIntroducidos,0,0,2);

				pthread_create(&punteroAtletas[i].hiloAtleta,NULL,accionesAtleta,(void*)&punteroAtletas[i].numeroAtleta);


				break;
			}
		}
	}
	pthread_mutex_unlock(&controladorEntrada);

	
}

void inicializarAtleta(int posicionPuntero, int numeroAtleta, int necesita_beber, int ha_competido, int tarimaAsignada){
	
	punteroAtletas[posicionPuntero].numeroAtleta = numeroAtleta;
	punteroAtletas[posicionPuntero].necesita_beber = necesita_beber;
	punteroAtletas[posicionPuntero].ha_competido = ha_competido;
	punteroAtletas[posicionPuntero].tarimaAsignada = tarimaAsignada;
	
}

void resetearAtleta(int posicionPuntero){
	punteroAtletas[posicionPuntero].numeroAtleta = 0;
	punteroAtletas[posicionPuntero].necesita_beber = 0;
	punteroAtletas[posicionPuntero].ha_competido = 0;
	punteroAtletas[posicionPuntero].tarimaAsignada = 0;
}

void *accionesAtleta(void* manejadora){
	int atletaActual=*(int*)manejadora-1;
	int comportamiento;
	int tiempoEspera;
	int subeTarima = 0;
	int enEspera=0; //Este es el tiempo que el atleta lleva en espera en una tarima
	char id[10];
	char msg[100];

	pthread_mutex_lock(&controladorEscritura);
	sprintf(msg, "Ha llegado a la tarima %d",punteroAtletas[atletaActual].tarimaAsignada);
	sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
	writeLogMessage(id,msg);
	pthread_mutex_unlock(&controladorEscritura);

	//Si el atleta llega a la tarima, espera 4 segundos para realizar su levantamiento.
	sleep(4);

	/*Añadimos el atleta a la cola*/
	pthread_mutex_lock(&controladorColaJueces);
	int j;
	for(j=0;j<10;j++){
		if(colaJuez[j]==100){
			colaJuez[j]=atletaActual;

			break;
		}
	}
	pthread_mutex_unlock(&controladorColaJueces);

	
	
	while(punteroAtletas[atletaActual].ha_competido == 0){

		comportamiento = calculoAleatorio(19,0);
		if(comportamiento<3){
			int i,j;
			pthread_mutex_lock(&controladorColaJueces);
			/*Dejamos hueco libre y avanzamos la cola*/
			for(i=0;i<10;i++){
				if(colaJuez[i]==atletaActual){

					for(j=i+1;j<10;j++){
						colaJuez[j-1]=colaJuez[j];
					}
					colaJuez[9]=100;
					break;
				}
			}
			pthread_mutex_unlock(&controladorColaJueces);
			pthread_mutex_lock(&controladorEscritura);
			sprintf(msg, "Ha tenido un problema y no va a poder subir a la tarima");
			sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
			writeLogMessage(id,msg);
			pthread_mutex_unlock(&controladorEscritura);

			pthread_exit(NULL);
		}else{


			sleep(3);
			enEspera = enEspera+3;
		}	
	}

	/*Comprueba si debe asistir a la fuente*/
	if(punteroAtletas[atletaActual].necesita_beber==1){

		pthread_mutex_lock(&controladorEntrada);
		resetearAtleta(atletaActual);
		pthread_mutex_unlock(&controladorEntrada);

		pthread_mutex_lock(&controladorColaFuente);
		int j;
		for(j=0;j<10;j++){
			if(colaFuente[j]==100){
				//printf("%d\n", atletaActual);
				colaFuente[j]=atletaActual;
				//printf("%d\n", colaFuente[j]);
				//printf("Añado atleta fuente posicion%d\n",j );
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg, "se va a la fuente");
				sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				break;
			}
		}
		pthread_mutex_unlock(&controladorColaFuente);



	}

	if(punteroAtletas[atletaActual].numeroAtleta!=0){
		pthread_mutex_lock(&controladorEntrada);
		resetearAtleta(atletaActual);
		pthread_mutex_unlock(&controladorEntrada);
	}
	pthread_mutex_lock(&controladorEscritura);
	sprintf(msg, "ha completado su participacion");
	sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
	writeLogMessage(id,msg);
	pthread_mutex_unlock(&controladorEscritura);


	pthread_exit(NULL);

}

void *accionesJuez(void* manejadora){

	int idJuez= *(int*)manejadora;
	int i=1;
	int tiempoEnTarima;
	int puntuacionEjercicio;
	int atletasAtendidos=0;
	int atletaActual=100;
	int j, k;
	int probabilidadMovimiento;
	int probabilidadAgua;
	char id[10];
	char msg[100];

	while(i==1){
		atletaActual=100;
		pthread_mutex_lock(&controladorColaJueces);
		/*Buscamos en la cola el primer atleta que pertenezca a la tarima*/
		for(j=0;j<10;j++){
			if(colaJuez[j]!=100){
				atletaActual=colaJuez[j];
				if(punteroAtletas[atletaActual].tarimaAsignada==idJuez){
					/*avanzamos la cola*/
					for(k=j;k<10;k++){
						if(k<9){
							colaJuez[k]=colaJuez[k+1];
						}
					}
					colaJuez[9]=100;
					break;
				}
			}
		}
		pthread_mutex_unlock(&controladorColaJueces);

		if(atletaActual!=100&&punteroAtletas[atletaActual].tarimaAsignada==idJuez){
			probabilidadMovimiento=calculoAleatorio(10,1);
			pthread_mutex_lock(&controladorEscritura);
			sprintf(msg,"entra en la tarima %d",idJuez);
			sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
			writeLogMessage(id,msg);
			pthread_mutex_unlock(&controladorEscritura);
			/*Movimiento valido*/
			if(probabilidadMovimiento<9){
				tiempoEnTarima=calculoAleatorio(6,2);
				sleep(tiempoEnTarima);
				puntuacionEjercicio=calculoAleatorio(300,60);
				/*comprobamos si la posicion pertenece al podium*/
				pthread_mutex_lock(&controladorPodium);
				for(j=0;j<3;j++){
					if(puntuacionEjercicio>mejoresPuntuaciones[j]){
						/*guaradmos la puntuacion y el id en el podium*/
						if(j==2){
							mejoresPuntuaciones[j]=puntuacionEjercicio;
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}else if(j==1){
							mejoresPuntuaciones[j+1]=mejoresPuntuaciones[j];
							mejoresPuntuaciones[j]=puntuacionEjercicio;

							mejoresAtletas[j+1]=mejoresAtletas[j];
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}else{
							mejoresPuntuaciones[j+2]=mejoresPuntuaciones[j+1];
							mejoresPuntuaciones[j+1]=mejoresPuntuaciones[j];
							mejoresPuntuaciones[j]=puntuacionEjercicio;

							mejoresAtletas[j+2]=mejoresAtletas[j+1];
							mejoresAtletas[j+1]=mejoresAtletas[j];
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}
					}
				}
				pthread_mutex_unlock(&controladorPodium);

				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"tiene una puntuacion de %d",puntuacionEjercicio);
				sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				probabilidadAgua=calculoAleatorio(10,1);

				if(probabilidadAgua==9){
					punteroAtletas[atletaActual].necesita_beber=1;
					
				} 
				atletasAtendidos++;
			}
			/*Descalificado normativa*/
			if(probabilidadMovimiento==9){
				tiempoEnTarima=calculoAleatorio(4,1);
				sleep(tiempoEnTarima);
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"ha sido descalificado por no cumplir la normativa");
				sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				atletasAtendidos++;
			}
			/*Levantamiento fallido*/
			if(probabilidadMovimiento==10){
				tiempoEnTarima=calculoAleatorio(10,6);
				sleep(tiempoEnTarima);
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"no ha realizado un movimiento válido");
				sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				atletasAtendidos++;

			}
			punteroAtletas[atletaActual].ha_competido=1;
			if(atletasAtendidos%4==0){
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"comienza el descanso");
				sprintf(id,"juez_%d",idJuez);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				sleep(10);
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"finaliza el descanso");
				sprintf(id,"juez_%d",idJuez);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
	

			}
		}
	}

}

void *accionesFuente(void* manejadora){
	
	int i;
	int atletaBebe = 100;
	int atletaBoton = 100;
	char id[10];
	char msg[100];


	while(1){
		pthread_mutex_lock(&controladorColaFuente);

		if (colaFuente[0] != 100 && colaFuente[1] != 100)
		{
			atletaBebe = colaFuente[0];
			atletaBoton = colaFuente[1];

			pthread_mutex_lock(&controladorFuente);
			fuenteOcupada = 1;
			pthread_mutex_unlock(&controladorFuente);
		}
		pthread_mutex_unlock(&controladorColaFuente);

		if(atletaBebe!=100){

			punteroAtletas[atletaBebe].necesita_beber=0;
			pthread_mutex_lock(&controladorEscritura);
			sprintf(msg, "ha bebido agua");
			sprintf(id,"atleta_%d",punteroAtletas[atletaBebe].numeroAtleta);
			writeLogMessage(id,msg);
			sprintf(msg, "ha pulsado el boton de la fuente");
			sprintf(id,"atleta_%d",punteroAtletas[atletaBoton].numeroAtleta);
			writeLogMessage(id,msg);
			pthread_mutex_unlock(&controladorEscritura);

			/*Avanzamos la cola*/
			pthread_mutex_lock(&controladorColaFuente);

			for(i=0;i<9;i++){

				colaFuente[i]=colaFuente[i+1];
			}

			colaFuente[9]=100;
			pthread_mutex_lock(&controladorFuente);
			fuenteOcupada=0;
			atletaBebe = 100;
			pthread_mutex_unlock(&controladorFuente);
			pthread_mutex_unlock(&controladorColaFuente);

		}

	}
		
	
}

void finalizarCompeticion(int a){
	char id[10];
	char msg[100];
	if(signal(SIGINT,finalizarCompeticion)==SIG_ERR){
		exit(-1);
	}
	pthread_mutex_lock(&controladorEscritura);
	sprintf(id,"el atleta_%d",mejoresAtletas[0]);
	sprintf(msg,"ha ganado con una puntuacion de %d", mejoresPuntuaciones[0]);
	writeLogMessage(id,msg);
	sprintf(id,"el atleta_%d",mejoresAtletas[1]);
	sprintf(msg,"ha quedado segundo con una puntuacion de %d", mejoresPuntuaciones[1]);
	writeLogMessage(id,msg);
	sprintf(id,"el atleta_%d",mejoresAtletas[2]);
	sprintf(msg,"ha quedado tercero con una puntuacion de %d", mejoresPuntuaciones[2]);
	writeLogMessage(id,msg);
	pthread_mutex_unlock(&controladorEscritura);

	exit(0);
}

void estadisticas(int a){
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGPIPE,estadisticas)==SIG_ERR){
		exit(-1);
	}
}

void writeLogMessage(char *id,char *msg){
	
	/*calculamos la hora actual*/
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow,19,"%d %m %y %H: %M: %S",tlocal);
	
	/*escribimos en el log*/
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
		
}




int calculoAleatorio(int max, int min){
	
	srand(time(NULL));
	int numeroAleatorio = rand()%((max+1)-min)+min;
	return numeroAleatorio;
}



