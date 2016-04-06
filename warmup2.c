#include<stdio.h>
#include<stdlib.h>
#include <pthread.h>
#include "cs402.h"
#include "my402list.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <math.h>


int B=10,P=3,num=4;//lambda=0,mu=0;
//float r=1.500000;

double lambda=1.000000;
double mu=(1/0.350000)*1000000;
double r=(1/1.500000)*1000000;
//double mu=0;

pthread_t arrival;
pthread_t token;
pthread_t s1;
pthread_t s2;
pthread_t signalThread;

pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER; 

pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

sigset_t set;

My402List bucket;
My402List q1;
My402List q2;
My402List q3;

typedef struct packetInformation{
int packetNo;
int tokens;
double packetSysEnterTime;
double q1_enterTime;
double q1_leaveTime;
double q2_enterTime;
double q2_leaveTime;
double serverEnterTime;
double serverLeaveTime;
double serverTime;
}packetInfo;

struct timeval tv;
double packetEnter=0;
double prevPacketArr=0;
double startTime=0;
double emulationStartTime=0;
double packetInterArrival=0;
double droppedPackets=0;
double droppedTokens=0;
int packetServed=0;
double tokenCount=0;

int packetCount=0;
int flag=0;
int serverEnd=0;
char line[1024];
FILE *file=NULL;
char *fileName=NULL;
FILE *filep=NULL;

double packetInterArrivalTime=0;
double packetServiceTime=0;
double avgQ1=0;
double avgQ2=0;
double avgS1=0;
double avgS2=0;
double avgPacketTimeInSys=0;

int cancelFlag=0;
int remcount=0;
int len=0;


void PrintTestList(My402List *pList, int num_items)
{
    My402ListElem *elem=NULL;

    if (My402ListLength(pList) != num_items) {
        fprintf(stderr, "List length is not %1d in PrintTestList().\n", num_items);
        exit(1);
    }
    for (elem=My402ListFirst(pList); elem != NULL; elem=My402ListNext(pList, elem)) {
	packetInfo *temp=elem->obj;
        double ival=temp->packetSysEnterTime;
	//double ival=(double)(elem->obj);
        fprintf(stdout, "%012.3f \n", ival);
    }
    fprintf(stdout, "\n");
}

	void moveFromQ1ToQ2()
	{
		int z=0;
		for(z=1;z<=P;z++)
		{
		My402ListElem *t1=My402ListFirst(&bucket);
		My402ListUnlink(&bucket,t1);
		}

		int q2_size=My402ListLength(&q2);
		
		My402ListElem *elem= NULL;
		elem=My402ListFirst(&q1);
		packetInfo *temp2= elem->obj;
		
		My402ListAppend(&q2,temp2);
		
		

		gettimeofday(&tv,NULL);
		double leaveq1Time=tv.tv_sec*1000000+tv.tv_usec;
		temp2->q1_leaveTime=leaveq1Time;
		printf("%012.3fms : p%d leaves Q1,time in Q1=%.3fms,token bucket now has %d tokens\n",(temp2->q1_leaveTime-emulationStartTime)/1000,temp2->packetNo,
(temp2->q1_leaveTime-temp2->q1_enterTime)/1000,My402ListLength(&bucket));
		avgQ1+=(temp2->q1_leaveTime-temp2->q1_enterTime)/1000;
		gettimeofday(&tv,NULL);
		double q2_enter=tv.tv_sec*1000000+tv.tv_usec;
		temp2->q2_enterTime=q2_enter;
		printf("%012.3fms : p%d enters Q2\n", (temp2->q2_enterTime-emulationStartTime)/1000,temp2->packetNo);
		My402ListUnlink(&q1,elem);

		if(My402ListLength(&q2)>0&&q2_size==0)
		pthread_cond_broadcast(&cv);
	}

	void readFile()
	{

	 fgets(line,sizeof line,file); 

		int counter=1;
		char *tempData;
		char dest[20];
		tempData=strtok(line," \t");
		while (tempData != NULL)
		{
			strcpy (dest, tempData);   

			tempData = strtok (NULL, " \t");
			if(counter==1)
			{lambda=atoi(dest);lambda=lambda*1000;}
			else if(counter==2)
			{P=atoi(dest);}
			else if(counter==3)
			{mu=atof(dest);mu=mu*1000;}
			counter++;
		}
	}

	packetInfo* createPacket(int packetnum,int tokensReq,double s)
	{

	    packetInfo *packet = (packetInfo *)malloc(sizeof(packetInfo)); 
	    packet->packetNo=packetnum;
	    packet->tokens=tokensReq;
	    packet->serverTime=s;
	    gettimeofday(&tv,NULL);
	    packet->packetSysEnterTime = tv.tv_sec*1000000+tv.tv_usec;
	    double packetEnters = packet->packetSysEnterTime;
	    packetCount++;
	//printf("Packet Count:%d\n",packetCount);
	    if(tokensReq>B)
	    {	packetServed++;
 		if(packetnum==1)
		{printf("%012.3fms : p%d arrives, needs %d tokens, inter-arrival time=%.3fms, dropped\n",(packetEnters-emulationStartTime)/1000,packet->packetNo,packet->tokens,
			(packetEnters-emulationStartTime)/1000);
		packetInterArrivalTime+=(packetEnters-emulationStartTime)/1000;
		}
		else
		{printf("%012.3fms : p%d arrives, needs %d tokens, inter-arrival time=%.3fms, dropped\n",(packetEnters-emulationStartTime)/1000,packet->packetNo,packet->tokens,
			(packetEnters-prevPacketArr)/1000);
		packetInterArrivalTime+=(packetEnters-prevPacketArr)/1000;
		}

		droppedPackets++;
		prevPacketArr=packetEnters;
		
		return NULL;
	    }
	
	    else{
			if(packetnum==1)
				{printf("%012.3fms : p%d arrives, needs %d tokens, inter-arrival time=%.3fms\n",(packetEnters-emulationStartTime)/1000,packet->packetNo,packet->tokens,
				(packetEnters-emulationStartTime)/1000);
				packetInterArrivalTime+=(packetEnters-emulationStartTime)/1000;
				}
			else{
				printf("%012.3fms : p%d arrives, needs %d tokens, inter-arrival time=%.3fms\n",(packetEnters-emulationStartTime)/1000,packet->packetNo,packet->tokens,
				(packetEnters-prevPacketArr)/1000);
				packetInterArrivalTime+=(packetEnters-prevPacketArr)/1000;
			}
		prevPacketArr=packetEnters;
		//packetCount++;
	    return packet;
	    }
	    
	}
  



    
    void *packetarrival(void *arg)
    {   //int y=0;
	int i=1;
	My402ListElem *element=NULL;
	packetInfo *pi=NULL;

	for(i=1;i<=num;i++)
	{   
	    if(flag==1)
		readFile();
	   // y=(lambda)*1000;
	    usleep(lambda);
	    pthread_mutex_lock(&m);
	    packetInfo *packetArr = createPacket(i,P,mu);
//printf("Hello1\n");
	    if(packetArr==NULL){
//printf("Hello2\n");
		if(packetCount==num && My402ListLength(&q1)==0)
		{serverEnd=1;
		 //printf("broadcasting\n");
		 pthread_cond_broadcast(&cv);}
		 pthread_mutex_unlock(&m);
			continue;
		}
	    else
	    {
	    

	    gettimeofday(&tv,NULL);
	    double temp=tv.tv_sec*1000000+tv.tv_usec;
	    packetArr->q1_enterTime=temp;
	    My402ListAppend(&q1,packetArr);

	    printf("%012.3fms : p%d enters Q1\n",((packetArr->q1_enterTime)-emulationStartTime)/1000,packetArr->packetNo);
		if(packetCount==num && My402ListLength(&q1)==0)
		{serverEnd=1;
		 //printf("broadcasting\n");
		 pthread_cond_broadcast(&cv);}

		if(My402ListLength(&q1)>0)
		{		
		element=My402ListFirst(&q1);
		pi=element->obj;
	        if(My402ListLength(&bucket)>=(pi->tokens) && My402ListLength(&q1)>0)
	    	moveFromQ1ToQ2();
		}


	    pthread_mutex_unlock(&m);
	    }
	}

	//printf("Packet thread exited\n");
	return 0;

    }

    void *tokenarrival(void *arg)
    {	//int x=(1.000000/r)*1000000;
	int i=1;
	My402ListElem *element=NULL;
	packetInfo *pi=NULL;
	for(;;)
	{
		
		pthread_mutex_lock(&m);
		if(packetCount==num && My402ListLength(&q1)==0)
	        {
		//printf("Token thread exited\n");
		pthread_mutex_unlock(&m);
		pthread_exit(0);
		}
		pthread_mutex_unlock(&m);
		usleep(r);
		tokenCount++;
		gettimeofday(&tv,NULL);
		double tokenArrives = tv.tv_sec*1000000+tv.tv_usec;
		pthread_mutex_lock(&m);
		if(My402ListLength(&bucket)>=B)
		{
		  printf("%012.3fms : token t%d arrives, dropped\n",(tokenArrives-emulationStartTime)/1000,i);
		  droppedTokens++;
		  //tokenCount++;
		  
		//My402ListElem *element=My402ListFirst(&q1);
		//packetInfo *pi=element->obj;
	    	//if(My402ListLength(&bucket)>=(pi->tokens) && My402ListLength(&q1)>0)
	    	//moveFromQ1ToQ2();
		//pthread_mutex_unlock(&m);
		  //i++;
		  //continue;
		}
		else
		{
			My402ListAppend(&bucket,(void*)i);
			printf("%012.3fms : token t%d arrives, token bucket now has %d tokens\n",(tokenArrives-emulationStartTime)/1000,i,My402ListLength(&bucket));
			//tokenCount++;
		
		if(My402ListLength(&q1)>0)
		{		
		element=My402ListFirst(&q1);
		pi=element->obj;
		

	    	if(My402ListLength(&bucket)>=(pi->tokens) && My402ListLength(&q1)>0)
	    	moveFromQ1ToQ2();
		}
		}
		pthread_mutex_unlock(&m);
		i++;

	}
	
	return 0;
	}
    

    void *serverone(void *arg)
    {
	
	while(1)
       {
	//printf("In S1\n");	
	packetInfo *temp=NULL;
	pthread_mutex_lock(&m);
        while(((My402ListLength(&q2)==0)&&serverEnd==0)&&cancelFlag==0)
	pthread_cond_wait(&cv,&m);
	
	if(cancelFlag==1)
	{pthread_mutex_unlock(&m);
	 pthread_exit(0);}

	if(serverEnd==1)
	{pthread_mutex_unlock(&m);
	 pthread_exit(0);}

	if(My402ListLength(&q2)>0)
	{
	My402ListElem *elem= NULL;
	elem=My402ListFirst(&q2);
	temp=elem->obj;
	My402ListUnlink(&q2,elem);
	packetServed++;
	//printf("Packet Served: %d \n",packetServed);
	gettimeofday(&tv,NULL);
	double q2_exitTime=tv.tv_sec*1000000+tv.tv_usec;
	temp->q2_leaveTime=q2_exitTime;
	printf("%012.3fms : p%d leaves Q2, time in Q2=%.3fms\n",(temp->q2_leaveTime-emulationStartTime)/1000,temp->packetNo,(temp->q2_leaveTime-temp->q2_enterTime)/1000);
	avgQ2+=(temp->q2_leaveTime-temp->q2_enterTime)/1000;
	gettimeofday(&tv,NULL);
	double s1_serviceTime=tv.tv_sec*1000000+tv.tv_usec;
	temp->serverEnterTime=s1_serviceTime;
	printf("%012.3fms : p%d begins service at S1,requesting %.3fms of service time\n",(temp->serverEnterTime-emulationStartTime)/1000,temp->packetNo,temp->serverTime/1000);
	}
	pthread_mutex_unlock(&m);
	int a = (temp->serverTime);	
	usleep(a);
	gettimeofday(&tv,NULL);
	double s1_endserviceTime=tv.tv_sec*1000000+tv.tv_usec;
	
	pthread_mutex_lock(&m);
	temp->serverLeaveTime=s1_endserviceTime;
	printf("%012.3fms : p%d departs from S1, service time = %.3fms, time in system = %.3fms\n",(temp->serverLeaveTime-emulationStartTime)/1000,temp->packetNo,
		(temp->serverLeaveTime-temp->serverEnterTime)/1000,(temp->serverLeaveTime-temp->packetSysEnterTime)/1000);

	
	packetInfo *x = (packetInfo *)malloc(sizeof(packetInfo));
	x->packetSysEnterTime=(temp->serverLeaveTime-temp->packetSysEnterTime)/1000000;
	My402ListAppend(&q3,x);

	avgS1+=(temp->serverLeaveTime-temp->serverEnterTime)/1000;
	packetServiceTime+=(temp->serverLeaveTime-temp->serverEnterTime)/1000;
	avgPacketTimeInSys+=(temp->serverLeaveTime-temp->packetSysEnterTime)/1000;
	if(packetServed==packetCount&&packetCount==num)
	{
	serverEnd=1;
	pthread_cond_broadcast(&cv);
	//pthread_mutex_unlock(&m);
	 //printf("S1 exited\n");
	 //pthread_exit(0);
	}
	pthread_mutex_unlock(&m);
	}
	
	return 0;

    }



    void *servertwo(void *arg)
    {
	while(1)
       {
	//printf("In S2\n");
	packetInfo *temp=NULL;
	pthread_mutex_lock(&m);
	while(((My402ListLength(&q2)==0)&&serverEnd==0)&&cancelFlag==0)
	pthread_cond_wait(&cv,&m);
	
	if(cancelFlag==1)
	{pthread_mutex_unlock(&m);
	 pthread_exit(0);}

	if(serverEnd==1)
	{pthread_mutex_unlock(&m);
	 pthread_exit(0);}

	//int pac=0;
	if(My402ListLength(&q2)>0)
	{
	My402ListElem *elem= NULL;
	elem=My402ListFirst(&q2);
	temp=elem->obj;
	My402ListUnlink(&q2,elem);
	packetServed++;
	//printf("Packet Served: %d \n",packetServed);
	gettimeofday(&tv,NULL);
	double q2_exitTime=tv.tv_sec*1000000+tv.tv_usec;
	temp->q2_leaveTime=q2_exitTime;
	printf("%012.3fms : p%d leaves Q2, time in Q2=%.3fms\n",(temp->q2_leaveTime-emulationStartTime)/1000,temp->packetNo,(temp->q2_leaveTime-temp->q2_enterTime)/1000);
	avgQ2+=(temp->q2_leaveTime-temp->q2_enterTime)/1000;
	gettimeofday(&tv,NULL);
	double s1_serviceTime=tv.tv_sec*1000000+tv.tv_usec;
	temp->serverEnterTime=s1_serviceTime;
	printf("%012.3fms : p%d begins service at S2,requesting %.3fms of service time\n",(temp->serverEnterTime-emulationStartTime)/1000,temp->packetNo,temp->serverTime/1000);
	}
	pthread_mutex_unlock(&m);	
	int b = (temp->serverTime);	
	usleep(b);
	gettimeofday(&tv,NULL);
	double s1_endserviceTime=tv.tv_sec*1000000+tv.tv_usec;
	
	pthread_mutex_lock(&m);
	temp->serverLeaveTime=s1_endserviceTime;
	printf("%012.3fms : p%d departs from S2, service time = %.3fms, time in system = %.3fms\n",(temp->serverLeaveTime-emulationStartTime)/1000,temp->packetNo,
		(temp->serverLeaveTime-temp->serverEnterTime)/1000,(temp->serverLeaveTime-temp->packetSysEnterTime)/1000);

	packetInfo *x = (packetInfo *)malloc(sizeof(packetInfo));
	x->packetSysEnterTime=(temp->serverLeaveTime-temp->packetSysEnterTime)/1000000;
	My402ListAppend(&q3,x);

	avgS2+=(temp->serverLeaveTime-temp->serverEnterTime)/1000;
	packetServiceTime+=(temp->serverLeaveTime-temp->serverEnterTime)/1000;
	avgPacketTimeInSys+=(temp->serverLeaveTime-temp->packetSysEnterTime)/1000;
	if(packetServed==packetCount&&packetCount==num)
	{
	serverEnd=1;
	pthread_cond_broadcast(&cv);
	//pthread_mutex_unlock(&m);
	// printf("S1 exited\n");
	 //pthread_exit(0);
	}
	pthread_mutex_unlock(&m);
	}


	return 0;

    }



void * ctrlC(void *arg)
{
      
      My402ListElem * elem=NULL;
      packetInfo *temp =(packetInfo *)malloc(sizeof(packetInfo));
      while (1)
      {
              sigwait(&set);
              pthread_mutex_lock(&m);
              pthread_cancel(arrival);
              pthread_cancel(token);
              cancelFlag=1;
              pthread_cond_broadcast(&cv);
       //printf("length %d\n",My402ListLength(&q1));
        len=My402ListLength(&q1);
	remcount+=len;
              if(!My402ListEmpty(&q1))
              {
               elem=My402ListFirst(&q1);
               
               while(len>0)
                      {        
                              temp=elem->obj;
                              //My402ListUnlink(&q1,elem);
                       gettimeofday(&tv,NULL);
       printf("%012.3fms: p%d removed from Q1\n",(temp->packetSysEnterTime-emulationStartTime)/1000,temp->packetNo);
                             
                               My402ListUnlink(&q1,elem);
                               elem=My402ListNext(&q1, elem);
                               len--;
                      }
              }
       //printf("length %d\n",My402ListLength(&q2));
       len=My402ListLength(&q2);
	remcount+=len;
              if(!My402ListEmpty(&q2))
              {        
               elem=My402ListFirst(&q2);
                     while(len>0)
                      {        
                              temp=elem->obj;
          gettimeofday(&tv,NULL);
       printf("%012.3fms: p%d removed from Q2\n",(temp->packetSysEnterTime-emulationStartTime)/1000,temp->packetNo);
               elem=My402ListNext(&q1, elem);                  
       My402ListUnlink(&q2,elem);  
       len--;           
                      }
              }
              pthread_mutex_unlock(&m);
              return NULL;
      }
}
 


    int main(int argc, char *argv[])
    {

int argCounter=1;	
//int opt=0; 
char *argument=NULL;     

        memset(&q1, 0, sizeof(My402List));
	(void)My402ListInit(&q1);

	memset(&q2, 0, sizeof(My402List));
	(void)My402ListInit(&q2);

	memset(&bucket, 0, sizeof(My402List));
	(void)My402ListInit(&bucket);

	memset(&q3, 0, sizeof(My402List));
	(void)My402ListInit(&q3);



//check for malformed command check


int i=0;

/*
for (i = 1 ; i < argc  ; i= i + 1)
{
char line1[50];
if(strcmp(argv[i], "-t") == 0 )
fp=fopen(argv[++i],"r");
fgets(line1,sizeof line1,fp);
int qwnum=atoi(line1);
if(!qwnum)
{   fprintf(stderr,"input file is not in the right format\n");
exit(0);
}
}
*/


for (i=1;i<argc;i=i+2)
    {

        if(strcmp(argv[i], "-lambda") == 0 )
        {
            if(argv[i+1])
            continue;
            else
            {
                fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num]"
                "[-t tsfile]\n");
                exit(1);
            }
            continue ;
        }
        else if(strcmp(argv[i], "-B") == 0)
        {
            if(argv[i+1])
            continue;
            else
            {
                fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num]"
                "[-t tsfile]\n");
                exit(1);
            }

            if(B < 0 || B>2147483647)
			{
                fprintf(stderr, "ERROR- B must be a positive integer and smaller than 2147483647\n");
                exit(1);
            }
            continue ;
        }      
		else if(strcmp(argv[i], "-mu") == 0)
		{
            if(argv[i+1])
            continue;
            else
			{
			fprintf(stderr, "Malformed Command. \n Syntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num]"
			"[-t tsfile]\n");
			exit(1);
			}

            if(mu < 0)
			{
				fprintf(stderr, "ERROR- mu must be a positive real number\n");
				exit(1);
            }
			continue ;
        }

        else if(strcmp(argv[i], "-P") == 0)
        {
            if(argv[i+1])
            continue;
            else
            {
                fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num]"
                "[-t tsfile]\n");
                exit(1);
            }

            if(P < 0 || P > 2147483647)
			{
                fprintf(stderr, "ERROR- P must be a positive integer and smaller than 2147483647\n");
                exit(1);
            }
            continue ;
        }
		else if(strcmp(argv[i], "-n") == 0)
        {
            if(argv[i+1])
            continue;
            else
            {
                fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
                exit(1);
            }
			if(num < 0 || num > 2147483647)
			{
                fprintf(stderr, "ERROR- n must be a positive integer and smaller than 2147483647\n");
                exit(1);
            }
            continue ;
        }
               

        else if(strcmp(argv[i], "-r") == 0)
		{
            if(argv[i+1])
            continue;
            else
            {
                fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num]"
                "[-t tsfile]\n");
                exit(1);
            }

            if(r < 0)
			{
                fprintf(stderr, "ERROR- mu must be a positive real number\n");
                exit(1);
            }
                         
            continue ;
        }

        else if(strcmp(argv[i], "-t") == 0)
		{		if(argv[i+1])
				{                    
                    fileName = strdup(argv[i+1]) ;
                    if(fileName == NULL)
					{
                        fprintf(stderr, "ERROR-No file name given\n");
                        exit(1);
                    }
				}else
				{
                    fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
					exit(1);
				}
                struct stat s;
                if( stat(fileName,&s) == 0 )
                {
                    if( s.st_mode & S_IFDIR )
                    {
                    fprintf(stderr,"Malformed Command. \n%s is a directory\n", fileName);   
                    exit(0);
                    }
				}
					filep =fopen(fileName,"r");

                    if(access(argv[i+1],F_OK))
                    {
						fprintf(stderr,"Malformed Command. \nFile %s does not exist\n", argv[i+1]);
                          
                        exit(0);
                    }
					if(access(argv[i+1],R_OK))
                    {
						fprintf(stderr,"Malformed Command. \nUnable to read file %s - access denied\n", argv[i+1]);
                          
                        exit(0);
                    }
					char line1[50];
					fgets(line1,sizeof line1,filep);
					int ip=atoi(line1);
					if(!ip)
					{
						fprintf(stderr,"input file not in right format\n");
						exit(1);
					}
					else if( filep == NULL)
            		{
            		perror(fileName);   
            		}
					else
            continue ;

        }
                
        else
        {
               
            fprintf(stderr, "Malformed Command. \nSyntax is : \nwarmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num]"
                    "[-t tsfile]\n");
            exit(1);
        }       
	}




	printf("\nEmulation Parameters:\n");

	while(argCounter<argc)
	{
		argument=argv[argCounter];
		//printf("Argument is:%s\n",argument);
		if(strcmp(argument,"-lambda")==0)
		{
			double dval=(double)0;
				if (sscanf(argv[++argCounter], "%lf", &dval) != 1) {
				/* cannot parse argv[2] to get a double value */
				} else {
				lambda=dval;printf("\tLambda =%.6g\n",lambda);
				if((1/lambda)>10)
				lambda=10*1000000;
				else
				lambda=(1/lambda)*1000000;
				
				}
			
		}
		if(strcmp(argument,"-mu")==0)
		{
			double dval=(double)0;
				if (sscanf(argv[++argCounter], "%lf", &dval) != 1) {
				/* cannot parse argv[2] to get a double value */
				} else {
				mu=dval;printf("\tmu = %.6g\n",mu);
				if((1/mu)>10)
				mu=10*1000000;
				else
				mu=(1/mu)*1000000;
			
				}
			
		}
		if(strcmp(argument,"-r")==0)
		{
			double dval=(double)0;
				if (sscanf(argv[++argCounter], "%lf", &dval) != 1) {
				/* cannot parse argv[2] to get a double value */
				} else {
				r=dval;printf("\tr = %.6g\n",r);
				if((1/r)>10)
				r=10*1000000;
				else
				r=(1/r)*1000000;
			
				}
			
		}
		if(strcmp(argument,"-B")==0)
		{
			int ival=(int)0;
				if (sscanf(argv[++argCounter], "%d", &ival) != 1) {
				/* cannot parse argv[2] to get a double value */
				} else {
				B=ival;
			printf("\tB = %d\n",B);
				}
			
		}
		if(strcmp(argument,"-n")==0)
		{
			int ival=(int)0;
				if (sscanf(argv[++argCounter], "%d", &ival) != 1) {
				/* cannot parse argv[2] to get a double value */
				} else {
				num=ival;
			printf("\tnumber to arrive =%d\n",num);
				}
			
		}
		if(strcmp(argument,"-P")==0)
		{
			int ival=(int)0;
				if (sscanf(argv[++argCounter], "%d", &ival) != 1) {
				/* cannot parse argv[2] to get a double value */
				} else {
				P=ival;
			printf("\tP = %d\n",P);
				}
			
		}
		if(strcmp(argument,"-t")==0)
		{	flag=1;
			
			file=fopen(argv[++argCounter],"r");
			fileName=argv[argCounter];
			printf("\ttsfile = %s\n",argv[argCounter]);
				if (file==NULL) {
				perror(argv[argCounter]);
				} else {
				fgets(line,sizeof line,file);
	    			num=atoi(line);
				if(!num)
				{   fprintf(stderr,"input file is not in the right format\n");
				    exit(1);
				}
				else
				printf("\tnumber to arrive = %d\n",num);
				}
			
		}
	argCounter++;
	}

	gettimeofday(&tv,NULL);
	emulationStartTime = (tv.tv_sec*1000000)+tv.tv_usec;
	printf("\n%012.3fms : emulation begins\n",emulationStartTime-emulationStartTime);


	sigemptyset(&set);
        sigaddset(&set,SIGINT);
        sigprocmask(SIG_BLOCK,&set,0);

	pthread_create(&arrival,0,packetarrival,NULL);
	pthread_create(&token,0,tokenarrival,NULL);
	pthread_create(&s2,0,servertwo,NULL);
	pthread_create(&s1,0,serverone,NULL);
	pthread_create(&signalThread,0,ctrlC,NULL);


	pthread_join(arrival,0);
	pthread_join(token,0);
	pthread_join(s1,0);
	pthread_join(s2,0);
	gettimeofday(&tv,NULL);
	printf("%012.3fms : emulation ends\n",((tv.tv_sec*1000000+tv.tv_usec)-emulationStartTime)/1000);

	//PrintTestList(&q3,My402ListLength(&q3));

	

	double simulationTime=((tv.tv_sec*1000000+tv.tv_usec)-emulationStartTime)/1000;
 	printf("\nStatistics:\n\n");
	//double stat_interArrTime=(packetInterArrivalTime/(num*1000));
	if(packetInterArrivalTime==0)
	printf("\taverage packet inter-arrival time = %.6gs\n",(double)0);
	else
	printf("\taverage packet inter-arrival time = %.6gs\n",(packetInterArrivalTime/(num*1000)));
	
	if(packetServiceTime==0)
	printf("\taverage packet service time = %.6gs\n",(double)0);
	else
	printf("\taverage packet service time = %.6gs\n",(packetServiceTime/(packetServed*1000)));

	if(avgQ1==0)
	printf("\n\taverage number of packets in Q1 = %.6g\n",(double)0);
	else
	printf("\n\taverage number of packets in Q1 = %.6g\n",avgQ1/simulationTime);

	if(avgQ2==0)
	printf("\taverage number of packets in Q2 = %.6g\n",(double)0);
	else
	printf("\taverage number of packets in Q2 = %.6g\n",avgQ2/simulationTime);

	if(avgS1==0)
	printf("\taverage number of packets in S1 = %.6g\n",(double)0);
	else
	printf("\taverage number of packets in S1 = %.6g\n",avgS1/simulationTime);

	if(avgS2==0)
	printf("\taverage number of packets in S2 = %.6g\n",(double)0);
	else
	printf("\taverage number of packets in S2 = %.6g\n",avgS2/simulationTime);

	double mean=avgPacketTimeInSys/((num-remcount)*1000);
	if(mean>0)
	printf("\n\taverage time a packet spent in system = %.6g\n",mean);
	else
	printf("\n\taverage time a packet spent in system = %.6g\n",(double)0);


double ival=0;
double t=0;
double sum=0;
packetInfo *temp=NULL;

	//calculate standard deviation
My402ListElem *elem=NULL;
	for (elem=My402ListFirst(&q3); elem != NULL; elem=My402ListNext(&q3, elem)) {
	temp=elem->obj;
        ival=temp->packetSysEnterTime;
	t=ival-mean;
	t=t*t;
	sum=sum+t;
        //fprintf(stdout, "%012.3f \n", ival);
    	}
sum=sum/(num-remcount);
sum=sqrt(sum);

	if(sum>0)
	printf("\tstandard deviation for time spent in system = %.6g\n",sum);
	else
	printf("\tstandard deviation for time spent in system = %.6g\n",(double)0);
	double tokenDropProb=(droppedTokens/tokenCount);
	double packetDropProb=(droppedPackets/num);
	//printf("tokenCount=%.6g\n",tokenCount);
	//printf("droppedTokens=%.6g\n",droppedTokens);
	//printf("droppedPackets=%.6g\n",droppedPackets);
	//printf("Number of packets=%d\n",num);
	if(droppedTokens>0)
	printf("\n\ttoken drop probability = %.6g\n",tokenDropProb);
	else
	printf("\n\ttoken drop probability = %.6g\n",(double)0);

	if(droppedPackets>0)
	printf("\tpacket drop probability = %.6g\n",packetDropProb);
	else
	printf("\tpacket drop probability = %.6g\n",(double)0);

return 0;

    }
