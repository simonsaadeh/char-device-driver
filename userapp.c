/*
 * userapp.c
 *
 *  Created on: 2019
 *      Author: Simon Saadeh
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/ioctl.h>

#define MAGIC_VAL 'R'
#define GetNumData _IOR(MAGIC_VAL, 1, int32_t*)
#define GetNumReader _IOR(MAGIC_VAL, 2, unsigned short*)
#define GetBufSize _IOR(MAGIC_VAL, 3, int32_t*)
#define SetBufSize _IOW(MAGIC_VAL, 4, unsigned long)
#define IOC_MAXNR 5
int main(void)
{
	int quite = 1;
	char  write_buf[256], read_buf[256];
	int mode = NULL, mode2 = NULL, mode3 = NULL;
	int fd = NULL;
	int nombredechar_a_lire = NULL;
	int ret=NULL;
	int32_t NumData, BufSize, NewBufSize;
	unsigned short NumLecteurs;
	
while(quite){
	printf("Veuillez choisir une des options suivantes: \n");
	printf("1 = Read ONLY Mode. \n");
	printf("2 = Write ONLY Mode. \n");
	printf("3 = Write/Read Mode. \n");
	printf("4 = Configurer des parametres dans le Buffer. (IOCTL) \n");
	printf("5 = Quitter le user panel \n");

	scanf("%i", &mode);
	
		switch(mode){
			case 1:
					fd = open("/dev/etsele_cdev", O_RDONLY);
					if(fd== -1){
						printf("file %s either does not exist or has been locked by another process\n","/dev/etsele_cdev");
						exit(-1);
					}

					printf("\n\nChoisir: \n");
					printf("1 = Lecture Mode Bloquant. \n");
					printf("2 = Lecture Mode Non-Bloquant. \n");
					scanf("%i", &mode3);
					if(mode3==2){
					int flags = fcntl(fd, F_GETFL, 0);
					fcntl(fd, F_SETFL, flags | O_NONBLOCK);
					}
					
					printf("Le nombre de char a lire \n");
					scanf("%i", &nombredechar_a_lire);
					ret = read(fd, read_buf, nombredechar_a_lire);
					if(ret>0)
						printf("Device: %.*s \n",nombredechar_a_lire,read_buf);
					else{
						printf("Erreur: Il n'y a plus de valeurs dans le buffer. \n");
					}
					break;
			case 2:
					fd = open("/dev/etsele_cdev", O_WRONLY);
					if(fd== -1){
						printf("file %s either does not exist or has been locked by another process\n","/dev/etsele_cdev");
						exit(-1);
					}

					printf("\n\nChoisir: \n");
					printf("1 = Ecriture Mode Bloquant. \n");
					printf("2 = Ecriture Mode Non-Bloquant. \n");
					scanf("%i", &mode3);
					if(mode3==2){
					int flags = fcntl(fd, F_GETFL, 0);
					fcntl(fd, F_SETFL, flags | O_NONBLOCK);
					}
					
					printf("enter data: ");
					scanf(" %[^\n]s", write_buf);
					write(fd, write_buf, strlen(write_buf));
					break;
			case 3:
					fd = open("/dev/etsele_cdev", O_RDWR);
					if(fd== -1){
						printf("file %s either does not exist or has been locked by another process\n","/dev/etsele_cdev");
						exit(-1);
					}

					printf("\n\nChoisir: \n");
					printf("1 = Lecture/Ecriture Mode Bloquant. \n");
					printf("2 = Lecture/Ecriture Mode Non-Bloquant. \n");
					scanf("%i", &mode3);
					if(mode3==2){
					int flags = fcntl(fd, F_GETFL, 0);
					fcntl(fd, F_SETFL, flags | O_NONBLOCK);
					}
					
					
					printf("enter data: ");
					scanf(" %[^\n]s", write_buf);
					write(fd, write_buf, strlen(write_buf));
					printf("Le nombre de char a lire \n");
					scanf("%i", &nombredechar_a_lire);
					ret = read(fd, read_buf, nombredechar_a_lire);
					if(ret>0)
						printf("Device: %.*s \n",nombredechar_a_lire,read_buf);
					else{
						printf("Erreur: Il n'y a plus de valeurs dans le buffer. \n");
					}
					break;
			case 4: 
					fd = open("/dev/etsele_cdev", O_RDONLY);
					if(fd== -1){
						printf("file %s either does not exist or has been locked by another process\n","/dev/etsele_cdev");
						exit(-1);
					}
					printf("\n\nVeuillez choisir une option:\n");
					printf("1- Afficher le nombre de données actuellement présentes dans le Buffer.\n");	
					printf("2- Afficher le nombre actuel de lecteur(s).\n");		
					printf("3- Afficher la taille actuelle du Buffer. \n");		
					printf("4- Changer la taille du Buffer. (Admin) \n");
					scanf("%i", &mode2);
					switch(mode2){
						case 1:
							ioctl(fd, GetNumData, (int32_t*) &NumData);
							printf("Nombre de données dans le Buffer: %d\n", NumData);
							break;
						case 2:
							ioctl(fd, GetNumReader, (int32_t*) &NumLecteurs);
							printf("Nombre de lecteurs: %d\n", NumLecteurs);
							break;
						case 3:
							ioctl(fd, GetBufSize, (int32_t*) &BufSize);
							printf("Taille du buffer: %d\n", BufSize);
							break;
						case 4:
							printf("Écrire la nouvelle taille: \n");
							scanf("%i", &NewBufSize);
							ioctl(fd, SetBufSize, (int32_t*) &NewBufSize);
							break;
						default:
							printf("command not recognized\n");
							break;
		
		
}
					break;	
			case 5:
					quite = 0; //on quite le userapp
					printf("Closing user panel\n");
					break;				
			default:
					printf("command not recognized\n");
					break;
	}
	memset(write_buf,0, 256);
	memset(read_buf,0, 256);
	close(fd);
}
	return 0;
}


