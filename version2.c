/*POWERLIFTING*/

/*Autor principal: ANDRÉS GARCÍA ÁLVVAREZ*/
/*Pablo Pérez López*/
/*Pablo González de la Iglesia*/
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


int numeroDeAtletas;
int numeroDeJueces;

int *colaJuez;/*La cola guarda una referencia en cada posicion al atleta que la ocupa*/
int atletasIntroducidos=0;
int mejoresPuntuaciones[3]={0,0,0};
int mejoresAtletas[3]={-1,-1,-1};
int fuenteOcupada;
int fin;



void nuevoCompetidorATarima1(int a);
void nuevoCompetidorATarima2(int a);
void inicializarAtleta(int posicionPuntero, int numeroAtleta, int necesita_beber, int ha_competido, int tarimaAsignada);
void writeLogMessage(char *id,char *msg);
void *accionesJuez(void* manejadora);
int calculoAleatorio(int max, int min);
void *accionesFuente(void *manejadora);
void *accionesAtleta(void* manejadora);
void finalizarCompeticion(int a);
void resetearAtleta(int posicionPuntero);
void mostrarEstadisticas();


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
pthread_mutex_t controladorHaCompetido;
pthread_cond_t fuenteCond;
pthread_cond_t finCond;

FILE *logFile;
char* logFileName ="registroTiempos.log";


int main(int argc, char *argv[]){

	if(argc==1){
		numeroDeAtletas=10;
		printf("Se admiten %d atletas simultaneamente.\n", numeroDeAtletas);
		numeroDeJueces=2;
		printf("El numero de tarimas es de %d.\n", numeroDeJueces);
	}else if(argc==3){
		numeroDeAtletas=atoi(argv[1]);
		printf("Se admiten %d atletas simultaneamente.\n", numeroDeAtletas);
		numeroDeJueces=atoi(argv[2]);
		printf("El numero de tarimas es de %d.\n", numeroDeJueces);
		if(numeroDeAtletas<1||numeroDeJueces<1){
			printf("Tiene que haber al menos un atleta y una tarima.\n");
			exit(0);
		}
	}else{
		printf("Argumentos incorrectos\n");
		exit(0);

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
	if(signal(SIGPIPE,mostrarEstadisticas)==SIG_ERR){
		exit(-1);
	}



	/*Declaramos los hilos juez y reservamos memoria*/
	pthread_t *jueces;

	jueces=(pthread_t *)malloc(sizeof(pthread_t)*numeroDeJueces);


	/*Inicializamos los semaforos*/
	pthread_mutex_init(&controladorEntrada, NULL);
	pthread_mutex_init(&controladorColaJueces,NULL);
	pthread_mutex_init(&controladorColaFuente,NULL);
	pthread_mutex_init(&controladorFuente,NULL);
	pthread_mutex_init(&controladorPodium,NULL);
	pthread_mutex_init(&controladorEscritura,NULL);
	pthread_mutex_init(&controladorHaCompetido,NULL);
	pthread_cond_init(&fuenteCond,NULL);
	pthread_cond_init(&finCond,NULL);

	/*Reservamos memoria para los atletas y la cola de los jueces*/
	punteroAtletas = (struct atletas*)malloc(sizeof(struct atletas)*numeroDeAtletas);


   	int i,j;
   	for(i=0;i<numeroDeAtletas;i++){
    	punteroAtletas[i].numeroAtleta=0;
		punteroAtletas[i].necesita_beber=0;
		punteroAtletas[i].ha_competido=0;
		punteroAtletas[i].tarimaAsignada=0;
  	}

   	/*Reservamos memoria e inicializamos la cola, como no tenemos -1 atletas será nuestro valor de control*/

	colaJuez= (int *)malloc(sizeof(int)*numeroDeAtletas);

	for(i=0;i<numeroDeAtletas;i++){
		colaJuez[i]=-1;
	}

   	/*Lanzamos losjueces*/
	for(i=0 ;i<numeroDeJueces;i++){
		j=i+1;
		pthread_create(&jueces[i],NULL, accionesJuez,(void*)&j);
		sleep(1);
	}


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
	int atletaAniadido=0;
	char id[10];
	char msg[100];
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	pthread_mutex_lock(&controladorEntrada);

		
	int i;

	for(i=0;i<numeroDeAtletas;i++){
		if(punteroAtletas[i].numeroAtleta==0){
			atletasIntroducidos++;
			inicializarAtleta(i,atletasIntroducidos,0,0,1);
			pthread_create(&punteroAtletas[i].hiloAtleta,NULL,accionesAtleta,(void*)&i);
			atletaAniadido=1;

			break;
		}
	}
	pthread_mutex_unlock(&controladorEntrada);

	if(atletaAniadido==0){
		pthread_mutex_lock(&controladorEscritura);
		sprintf(msg, "No se ha podido aniadir el atleta");
		sprintf(id,"INFORMACION");
		writeLogMessage(id,msg);
		pthread_mutex_unlock(&controladorEscritura);
	}

	
}

void nuevoCompetidorATarima2(int a){
	int atletaAniadido=0;
	char id[10];
	char msg[100];
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	pthread_mutex_lock(&controladorEntrada);


	int i;

	for(i=0;i<numeroDeAtletas;i++){
		if(punteroAtletas[i].numeroAtleta==0){
			atletasIntroducidos++;
			inicializarAtleta(i,atletasIntroducidos,0,0,2);
			pthread_create(&punteroAtletas[i].hiloAtleta,NULL,accionesAtleta,(void*)&i);
			atletaAniadido=1;

			break;
		}
	}
	pthread_mutex_unlock(&controladorEntrada);

	if(atletaAniadido==0){
		pthread_mutex_lock(&controladorEscritura);
		sprintf(msg, "No se ha podido aniadir el atleta");
		sprintf(id,"INFORMACION");
		writeLogMessage(id,msg);
		pthread_mutex_unlock(&controladorEscritura);
	}



	
}

void inicializarAtleta(int posicionPuntero, int numeroAtleta, int necesita_beber, int ha_competido, int tarimaAsignada){
	
	punteroAtletas[posicionPuntero].numeroAtleta = numeroAtleta;
	punteroAtletas[posicionPuntero].necesita_beber = necesita_beber;
	punteroAtletas[posicionPuntero].ha_competido = ha_competido;
	punteroAtletas[posicionPuntero].tarimaAsignada = tarimaAsignada;
	
}

void *accionesAtleta(void* manejadora){
	int atletaActual=*(int*)manejadora;
	int idAtleta=punteroAtletas[atletaActual].numeroAtleta;
	int sigueEnLaCola;//se utiliza para saber si el juez ya ha cogido al atleta de la tarima
	int comportamiento;
	int tiempoEspera;
	int subeTarima = 0;
	int enEspera=0; //Este es el tiempo que el atleta lleva en espera en una tarima
	int tengoQueBeber=0;
	char id[10];
	char msg[100];



	pthread_mutex_lock(&controladorEscritura);
	sprintf(msg, "Ha llegado al sistema se dirige hacia la tarima %d",punteroAtletas[atletaActual].tarimaAsignada);
	sprintf(id,"NUEVO atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
	writeLogMessage(id,msg);
	pthread_mutex_unlock(&controladorEscritura);

	//Si el atleta llega a la tarima, espera 4 segundos para realizar su levantamiento.
	sleep(4);

	/*Añadimos el atleta a la cola*/
	pthread_mutex_lock(&controladorColaJueces);
	int j;
	for(j=0;j<numeroDeAtletas;j++){
		if(colaJuez[j]==-1){
			colaJuez[j]=atletaActual;

			break;
		}
	}
	pthread_mutex_unlock(&controladorColaJueces);

	
	
	while(punteroAtletas[atletaActual].ha_competido == 0||punteroAtletas[atletaActual].ha_competido == 1){

		comportamiento = calculoAleatorio(19,0);
		/*Comprobamos si el atleta sigue en la cola*/
		sigueEnLaCola=0;
		pthread_mutex_lock(&controladorColaJueces);
		for(j=0;j<numeroDeAtletas;j++){
			if(atletaActual==colaJuez[j]){
				sigueEnLaCola=1;
				break;
			}
		}

		if(comportamiento<3&&sigueEnLaCola==1){
			int i,j;
			/*Dejamos hueco libre y avanzamos la cola*/
			for(i=0;i<numeroDeAtletas;i++){
				if(colaJuez[i]==atletaActual){

					for(j=i+1;j<numeroDeAtletas;j++){
						colaJuez[j-1]=colaJuez[j];
					}

					colaJuez[numeroDeAtletas-1]=-1;
					break;
				}
			}
			pthread_mutex_unlock(&controladorColaJueces);

			pthread_mutex_lock(&controladorHaCompetido);
			punteroAtletas[atletaActual].ha_competido=2;
			pthread_mutex_unlock(&controladorHaCompetido);

			pthread_mutex_lock(&controladorEscritura);
			sprintf(msg, "Ha tenido un problema y no va a poder subir a la tarima");
			sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
			writeLogMessage(id,msg);
			pthread_mutex_unlock(&controladorEscritura);
		}else{

			pthread_mutex_unlock(&controladorColaJueces);
			sleep(3);
			enEspera = enEspera+3;
		}	
	}

	tengoQueBeber=punteroAtletas[atletaActual].necesita_beber;

	pthread_mutex_lock(&controladorEntrada);
	resetearAtleta(atletaActual);
	pthread_mutex_unlock(&controladorEntrada);
	/*Comprobamos si el atleta tiene que ir a la fuente*/
	if(tengoQueBeber==1){
			pthread_mutex_lock(&controladorEscritura);
			sprintf(msg, "Se va a la fuente");
			sprintf(id,"atleta_%d",idAtleta);
			writeLogMessage(id,msg);
			pthread_mutex_unlock(&controladorEscritura);

			pthread_cond_signal(&fuenteCond);
			pthread_mutex_lock(&controladorColaFuente);
			pthread_cond_wait(&fuenteCond, &controladorColaFuente);
			pthread_mutex_unlock(&controladorColaFuente);

			pthread_mutex_lock(&controladorEscritura);
			sprintf(msg, "Bebe agua");
			sprintf(id,"atleta_%d",idAtleta);
			writeLogMessage(id,msg);
			pthread_mutex_unlock(&controladorEscritura);
	}
	
	/*Mensaje fin de hilo*/
	pthread_mutex_lock(&controladorEscritura);
	sprintf(msg, "ha completado su participacion");
	sprintf(id,"atleta_%d",idAtleta);
	writeLogMessage(id,msg);
	pthread_mutex_unlock(&controladorEscritura);


	pthread_exit(NULL);

}

void *accionesJuez(void* manejadora){

	int idJuez= *(int*)manejadora;
	int i=1;
	int atletaAdecuado;
	int tiempoEnTarima;
	int puntuacionEjercicio;
	int atletasAtendidos=0;
	int atletaActual=-1;
	int j, k;
	int probabilidadMovimiento;
	int probabilidadAgua;
	char id[10];
	char msg[100];


	printf("Soy el juez %d iniciando.\n", idJuez);
	while(i==1){
		atletaActual=-1;
		pthread_mutex_lock(&controladorColaJueces);
		/*Buscamos en la cola el primer atleta que pertenezca a la tarima*/
		for(j=0;j<numeroDeAtletas;j++){
			atletaAdecuado=0;
			if(colaJuez[j]!=-1){
				atletaActual=colaJuez[j];
				if(punteroAtletas[atletaActual].tarimaAsignada==idJuez){
					atletaAdecuado=1;
					pthread_mutex_lock(&controladorHaCompetido);
					punteroAtletas[atletaActual].ha_competido=1;
					pthread_mutex_unlock(&controladorHaCompetido);
					/*avanzamos la cola*/
					for(k=j;k<numeroDeAtletas;k++){
						if(k<numeroDeAtletas-1){
							colaJuez[k]=colaJuez[k+1];
						}
					}
					colaJuez[numeroDeAtletas-1]=-1;
					break;
				}
			}
		}

		/*Si no ha encontrado un atleta de su tarima coge el primero*/
		if(atletaAdecuado==0){
			if(colaJuez[0]!=-1){
				atletaActual=colaJuez[0];
				pthread_mutex_lock(&controladorHaCompetido);
				punteroAtletas[atletaActual].ha_competido=1;
				pthread_mutex_unlock(&controladorHaCompetido);
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"va a entrar en la tarima %d porque no hay nadie para dicha tarima",idJuez);
				sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				for(k=0;k<numeroDeAtletas;k++){
					if(k<numeroDeAtletas-1){
						colaJuez[k]=colaJuez[k+1];
					}
					colaJuez[numeroDeAtletas-1]=-1;

				}	
			}
		}
		pthread_mutex_unlock(&controladorColaJueces);
		/*En caso de tener un atleta valido lo sube a la tarima*/
		if(atletaActual!=-1){
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

				atletasAtendidos++;
			}
			/*Descalificado normativa*/
			if(probabilidadMovimiento==9){
				tiempoEnTarima=calculoAleatorio(4,1);
				sleep(tiempoEnTarima);

				/*comprobamos si la posicion pertenece al podium*/
				pthread_mutex_lock(&controladorPodium);
				for(j=0;j<3;j++){
					if(puntuacionEjercicio>mejoresPuntuaciones[j]){
						/*guaradmos la puntuacion y el id en el podium*/
						if(j==2){
							mejoresPuntuaciones[j]=0;
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}else if(j==1){
							mejoresPuntuaciones[j+1]=mejoresPuntuaciones[j];
							mejoresPuntuaciones[j]=0;

							mejoresAtletas[j+1]=mejoresAtletas[j];
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}else{
							mejoresPuntuaciones[j+2]=mejoresPuntuaciones[j+1];
							mejoresPuntuaciones[j+1]=mejoresPuntuaciones[j];
							mejoresPuntuaciones[j]=0;

							mejoresAtletas[j+2]=mejoresAtletas[j+1];
							mejoresAtletas[j+1]=mejoresAtletas[j];
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}
					}
				}
				pthread_mutex_unlock(&controladorPodium);


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

				/*comprobamos si la posicion pertenece al podium*/
				pthread_mutex_lock(&controladorPodium);
				for(j=0;j<3;j++){
					if(puntuacionEjercicio>mejoresPuntuaciones[j]){
						/*guaradmos la puntuacion y el id en el podium*/
						if(j==2){
							mejoresPuntuaciones[j]=0;
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}else if(j==1){
							mejoresPuntuaciones[j+1]=mejoresPuntuaciones[j];
							mejoresPuntuaciones[j]=0;

							mejoresAtletas[j+1]=mejoresAtletas[j];
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}else{
							mejoresPuntuaciones[j+2]=mejoresPuntuaciones[j+1];
							mejoresPuntuaciones[j+1]=mejoresPuntuaciones[j];
							mejoresPuntuaciones[j]=0;

							mejoresAtletas[j+2]=mejoresAtletas[j+1];
							mejoresAtletas[j+1]=mejoresAtletas[j];
							mejoresAtletas[j]=punteroAtletas[atletaActual].numeroAtleta;
							break;
						}
					}
				}
				pthread_mutex_unlock(&controladorPodium);
				pthread_mutex_lock(&controladorEscritura);
				sprintf(msg,"no ha realizado un movimiento valido");
				sprintf(id,"atleta_%d",punteroAtletas[atletaActual].numeroAtleta);
				writeLogMessage(id,msg);
				pthread_mutex_unlock(&controladorEscritura);
				atletasAtendidos++;

			}

			probabilidadAgua=calculoAleatorio(10,1);
			//probabilidadAgua=9;/*Mandamos al taleta simpre a beber*/
			if(probabilidadAgua==9){
				punteroAtletas[atletaActual].necesita_beber=1;
					
			} 
			pthread_mutex_lock(&controladorHaCompetido);
			punteroAtletas[atletaActual].ha_competido=2;
			pthread_mutex_unlock(&controladorHaCompetido);
			pthread_cond_signal(&finCond);
			/*Comprobamos si el juez tiene que descansar*/
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



void finalizarCompeticion(int a){
	char id[10];
	char msg[100];
	if(signal(SIGINT,finalizarCompeticion)==SIG_ERR){
		exit(-1);
	}


	/*Esperamos a que todos los atletas acaben*/
	pthread_mutex_lock(&controladorColaJueces);
	while(colaJuez[0]!=-1){
		
			pthread_cond_wait(&finCond, &controladorColaJueces);
		
	}
	pthread_mutex_unlock(&controladorColaJueces);

	/*En funcion de el numero de atletas que han terminado ponemos el podium*/
	pthread_mutex_lock(&controladorPodium);
	pthread_mutex_lock(&controladorEscritura);

	if(mejoresAtletas[0]!=-1){

		sprintf(id,"el atleta_%d",mejoresAtletas[0]);
		sprintf(msg,"ha ganado con una puntuacion de %d", mejoresPuntuaciones[0]);
		writeLogMessage(id,msg);
	}
	if(mejoresAtletas[1]!=-1){
		sprintf(id,"el atleta_%d",mejoresAtletas[1]);
		sprintf(msg,"ha quedado segundo con una puntuacion de %d", mejoresPuntuaciones[1]);
		writeLogMessage(id,msg);
	}
	if(mejoresAtletas[2]!=-1){
		sprintf(id,"el atleta_%d",mejoresAtletas[2]);
		sprintf(msg,"ha quedado tercero con una puntuacion de %d", mejoresPuntuaciones[2]);
		writeLogMessage(id,msg);
	}


	pthread_mutex_unlock(&controladorEscritura);
	pthread_mutex_unlock(&controladorPodium);


	pthread_mutex_lock(&controladorEscritura);
	sprintf(msg,"Se notifica a la fuente por si algun atleta se ha quedado sin beber.");
	sprintf(id,"Informacion");
	writeLogMessage(id,msg);
	pthread_mutex_unlock(&controladorEscritura);
	pthread_cond_signal(&fuenteCond);

	sleep(1);


	exit(0);
}

void writeLogMessage(char *id,char *msg){
	
	/*calculamos la hora actual*/
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[30];
	strftime(stnow,30,"%d/%m/%y %H:%M:%S",tlocal);
	
	/*escribimos en el log*/
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
		
}

void resetearAtleta(int posicionPuntero){
		/*ponemos los valores como al inicio para poder utilizar la posicion del array corresoundiente*/
    	punteroAtletas[posicionPuntero].numeroAtleta=0;
		punteroAtletas[posicionPuntero].necesita_beber=0;
		punteroAtletas[posicionPuntero].ha_competido=0;
		punteroAtletas[posicionPuntero].tarimaAsignada=0;

}

void mostrarEstadisticas(){
	char id[10];
	char msg[100];
	int atletasEsperando=0.;
	int atletasCompitiendo=0;
	int atletasEnSistema=0;

	int i;
	/*Recorremos el array de atletas buscando si han competido o estan compitiendo*/
	pthread_mutex_lock(&controladorHaCompetido);
	for(i=0;i<numeroDeAtletas;i++){
		if(punteroAtletas[i].ha_competido==0&&punteroAtletas[i].numeroAtleta!=0){
			atletasEsperando++;
		}
		if(punteroAtletas[i].ha_competido==1&&punteroAtletas[i].numeroAtleta!=0){
			atletasCompitiendo++;
		}
	}
	pthread_mutex_unlock(&controladorHaCompetido);

	pthread_mutex_lock(&controladorEntrada);
	atletasEnSistema=atletasIntroducidos;
	pthread_mutex_unlock(&controladorEntrada);
	/*Mostramos las estadisticas*/
	pthread_mutex_lock(&controladorEscritura);
	sprintf(id,"ESTADISTICAS");
	sprintf(msg,"Hay %d atletas esperando para competir.", atletasEsperando);
	writeLogMessage(id,msg);
	sprintf(id,"ESTADISTICAS");
	sprintf(msg,"Hay %d atletas compitiendo en este momento.", atletasCompitiendo);
	writeLogMessage(id,msg);
	sprintf(id,"ESTADISTICAS");
	sprintf(msg,"Se han introducido %d atletas.", atletasEnSistema);
	writeLogMessage(id,msg);
	printf("\n\n\nHay %d atletas esperando para competir.\n", atletasEsperando);
	printf("Hay %d atletas compitiendo en este momento.\n", atletasCompitiendo);
	printf("Se han introducido %d atletas.\n", atletasEnSistema);
	printf("\n");
	pthread_mutex_unlock(&controladorEscritura);

}

int calculoAleatorio(int max, int min){
	
	srand(time(NULL));
	int numeroAleatorio = rand()%((max+1)-min)+min;
	return numeroAleatorio;
}