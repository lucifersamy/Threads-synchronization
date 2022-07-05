#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <math.h>
#include <sys/time.h>

struct Complex{
    float img;
    float real;
};

static void * threadFunc(void *arg);
void printTimeStamp(char* message);
void killThreads();
void fatal(const char *format, ...);

int n,m;
int arrived=0;
int signalArrived=0;

pthread_t* thread_id = NULL;
int **A = NULL;
int **B= NULL;
int **C= NULL;
struct Complex ** dft;


pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutexattr_t mtxAttr;
pthread_mutex_t mtx;

void my_handler(){
    signalArrived++;
}


void freeArrays(){
    int i;
    for(i = 0; i < pow(2,n); i++)
        free(A[i]);
    free(A);
   
    for(i = 0; i < pow(2,n); i++)
        free(B[i]);
    free(B);
   
    
    for(i = 0; i < pow(2,n); i++)
        free(C[i]);
    free(C);

    for(i = 0; i < pow(2,n); i++)
        free(dft[i]);
    free(dft);

}

int main(int argc,char *argv[])
{
    int timeReturn;
    struct timeval beginTime;
    struct timeval endTime;
    timeReturn = gettimeofday(&beginTime, 0);
    if(timeReturn != 0){
        perror("gettimeofday");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    }

    int opt;
    char* filePath1=NULL;
    char* filePath2=NULL;
    char* outputPath=NULL;
    FILE *fp1 = NULL;
    FILE *fp2 = NULL;
    FILE *fpOutput = NULL;



    while((opt = getopt(argc, argv, ":i:j:o:n:m:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                filePath1 = optarg;
                break;
            case 'j':
                filePath2 = optarg;
                break;
            case 'o':
                outputPath = optarg;
                break;
            case 'n':
                n = atoi(optarg);
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case ':':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -i filePath1 -j filePath2 -o output -n 4 -m 2\n", argv[0]);
                exit(EXIT_FAILURE);     
                break;  
            case '?':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -i filePath1 -j filePath2 -o output -n 4 -m 2\n", argv[0]);
                exit(EXIT_FAILURE); 
                break; 
            case -1:
                break;
            default:
                abort (); 
        }
    }
    if(n<=2 | m<2){
        errno =EINVAL;
        fprintf(stderr, "Usage: %s -i filePath1 -j filePath2 -o output -n 4 -m 2\n", argv[0]);
        fprintf(stderr, "n must be greater than 2 and m must be greater than 1\n");
        exit(EXIT_FAILURE); 
    }
    if(m>pow(2,n)){
        errno=EINVAL;
        fprintf(stderr, "Usage: %s -i filePath1 -j filePath2 -o output -n 4 -m 2\n", argv[0]);
        fprintf(stderr, "m should not be greater than n.\n");
        exit(EXIT_FAILURE); 
    }
    if(m%2 != 0){
        errno=EINVAL;
        fprintf(stderr, "Usage: %s -i filePath1 -j filePath2 -o output -n 4 -m 2\n", argv[0]);
        fprintf(stderr, "2 mode of m must be 0.\n");
        exit(EXIT_FAILURE); 
    }
    
    if(optind!=11){
        errno=EINVAL;
        fprintf(stderr, "Wrong format.\n" );
        fprintf(stderr, "Usage: %s -i filePath1 -j filePath2 -o output -n 4 -m 2\n", argv[0]);
        exit(EXIT_FAILURE); 
    }

    struct sigaction newact;
    newact.sa_handler = &my_handler;
    newact.sa_flags = 0;

    if((sigemptyset(&newact.sa_mask) == -1) || (sigaction(SIGINT, &newact, NULL) == -1) ){
        perror("Failled to install SIGINT signal handler");
        exit(EXIT_FAILURE);
    }

    int i=0,j=0;
    char c;
    int k;
    
    A = calloc(pow(2,n), sizeof *A);
    for (k=0; k<pow(2,n); k++){
        A[k] = calloc(pow(2,n), sizeof *(A[k]));
    }

    B = calloc(pow(2,n), sizeof *B);
    for (k=0; k<pow(2,n); k++){
        B[k] = calloc(pow(2,n), sizeof *(B[k]));
    }

    C = calloc(pow(2,n), sizeof *C);
    for (k=0; k<pow(2,n); k++){
        C[k] = calloc(pow(2,n), sizeof *(C[k]));
    }

    dft = calloc(pow(2,n), sizeof *dft);
    for (k=0; k<pow(2,n); k++){
        dft[k] = calloc(pow(2,n), sizeof *(dft[k]));
    }

    if(signalArrived!=0){
        freeArrays();
        exit(EXIT_SUCCESS);
    }
    
    for(i=0;i<pow(2,n); ++i){
        for(j=0;j<pow(2,n); ++j){
            C[i][j] = 0;
        }
    }

    int s, type;
    s = pthread_mutexattr_init(&mtxAttr);
    if (s != 0){
        perror("pthread_mutexattr_init");
        freeArrays();
        exit(EXIT_FAILURE);
    }
    s = pthread_mutexattr_settype(&mtxAttr, PTHREAD_MUTEX_ERRORCHECK);
    if (s != 0){
        perror("pthread_mutexattr_settype");
        freeArrays();
        exit(EXIT_FAILURE);
    }
    s = pthread_mutex_init(&mtx, &mtxAttr);
    if (s != 0){
        perror("pthread_mutexattr_init");
        freeArrays();
        exit(EXIT_FAILURE);
    }
    s = pthread_mutexattr_destroy(&mtxAttr); 
    if (s != 0){
        perror("pthread_mutexattr_destroy");
        freeArrays();
        exit(EXIT_FAILURE);
    }

    if(signalArrived!=0){
        freeArrays();
        exit(EXIT_SUCCESS);
    }
    
    i=0,j=0;
    if ((fp1 = fopen (filePath1, "r")) == NULL) {
        printf("open error!\n");
        freeArrays();
        exit(EXIT_FAILURE);
    }

    int newChar=0;
    while(1){
        c = fgetc(fp1);
        if(c == EOF){
            break;
        }
        if(newChar == pow(2,n)*pow(2,n)){
            break;
        }
        A[i][j] = c;
        newChar++;
        j++;
        if(j == pow(2,n)){
            j=0;
            ++i;
        }
    }

    if(newChar != pow(2,n)*pow(2,n)){
        fclose (fp1);
        freeArrays();
        fatal("fatal\n");
    }
    fclose (fp1);


    char* message1 = calloc(300, sizeof *message1);
    sprintf(message1,"Two matrices of size %.fx%.f have been read. The number of threads is %d\n",pow(2,n),pow(2,n),m);
    printTimeStamp(message1);
    free(message1);

    if(signalArrived!=0){
        freeArrays();
        exit(EXIT_SUCCESS);
    }


    if ((fp2 = fopen (filePath2, "r")) == NULL) {
        printf("open error!\n");
        freeArrays();
        exit(EXIT_FAILURE);
    }

    i=0,j=0;
    newChar=0;

    while(1){
        c = fgetc(fp2);
        if(c == EOF){
            break;
        }   
        if(newChar == pow(2,n)*pow(2,n)){
            break;
        }
        B[i][j] = c;
        newChar++;
        j++;
        if(j == pow(2,n)){
            j=0;
            ++i;
        }
    }
    if(newChar != pow(2,n)*pow(2,n)){
        fclose (fp2);
        freeArrays();
        fatal("fatal\n"); 
    }
    fclose (fp2);

    if(signalArrived!=0){
        freeArrays();
        exit(EXIT_SUCCESS);
    }

    thread_id = (pthread_t*) calloc(m, sizeof(pthread_t));
    int threadNums[m];
    int returnValue;

    for(i=0; i<m; ++i){
        threadNums[i] = i;
        returnValue = pthread_create(&thread_id[i], NULL, threadFunc, &threadNums[i]);
        if (returnValue != 0){
            fprintf(stderr, "pthread_create supplier\n");
            freeArrays();
            exit(EXIT_FAILURE); 
        }
    }

    for(i=0; i<m; ++i){
        pthread_join(thread_id[i],NULL);
    }

    if(signalArrived!=0){
        freeArrays();
        free(thread_id);
        exit(EXIT_SUCCESS);
    }
    
    


    if ((fpOutput = fopen (outputPath, "w")) == NULL) {
        printf("open error!\n");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    }

    char* outputMessage = calloc(1000, sizeof *outputMessage);
    for (int i = 0; i < pow(2,n); ++i)
    {
        for (int j = 0; j < pow(2,n); ++j)
        {
            sprintf(outputMessage,"%3f+%3fi,",dft[i][j].real,dft[i][j].img);
            fputs(outputMessage, fpOutput);
        }
        putc('\n', fpOutput);
    }
    fclose(fpOutput);

    timeReturn = gettimeofday(&endTime, 0);
    if(timeReturn != 0){
        perror("gettimeofday");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    }

    long seconds = endTime.tv_sec - beginTime.tv_sec;
    long microseconds = endTime.tv_usec - beginTime.tv_usec;
    double elapsed = seconds + microseconds*1e-6;
    
    message1 = calloc(300, sizeof *message1);
    sprintf(message1,"The process has written the output file. The total time spent is %.4f seconds.\n",elapsed);
    printTimeStamp(message1);
    free(message1);

    free(outputMessage);
    free(thread_id);
    freeArrays();
}

static void * threadFunc(void *arg)
{   
    int timeReturn;
    struct timeval beginTime;
    struct timeval endTime;
    timeReturn = gettimeofday(&beginTime, 0);
    if(timeReturn != 0){
        perror("gettimeofday");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    }

    int i,j,k;
    int threadNum = *(int*) arg;
    int SIZE=pow(2,n);
    int start = (SIZE/m) * threadNum; 
    int end = start + (SIZE/m);
    if(threadNum == m-1){
        end =pow(2,n);
    }
    
    //multiplication part
    for(i = 0; i < SIZE; ++i) { //A matrix
        for( j = start; j < end; ++j) {
            for (k = 0; k < SIZE; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
            
        }
    }
    //senk barrier
    pthread_mutex_lock(&mtx);
    ++arrived;

    timeReturn = gettimeofday(&endTime, 0);
    if(timeReturn != 0){
        perror("gettimeofday");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    }
    while(arrived<m){
        pthread_cond_wait(&cond,&mtx);
    }

    long seconds = endTime.tv_sec - beginTime.tv_sec;
    long microseconds = endTime.tv_usec - beginTime.tv_usec;
    double elapsed = seconds + microseconds*1e-6;

    char* message = calloc(300, sizeof *message);
    sprintf(message,"Thread %d has reached the rendezvous point in %.4f seconds.\n",threadNum,elapsed);
    printTimeStamp(message);
    free(message);

    
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mtx);

    message = calloc(300, sizeof *message);
    sprintf(message,"Thread %d is advancing to the second part\n",threadNum);
    printTimeStamp(message);
    free(message);

    timeReturn = gettimeofday(&beginTime, 0);
    if(timeReturn != 0){
        perror("gettimeofday");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    } 

    //DFT part
    float x,y;
    int g,s;
    for(i=start;i<end;i++){
        for(j=0;j<SIZE;j++){
            dft[i][j].real=0; 
            dft[i][j].img=0;
            for(g=0;g<SIZE;g++){
                for(s=0;s<SIZE;s++){
                    x = (-2.0*M_PI*i*g/(float)SIZE);
                    y = (-2.0*M_PI*j*s/(float)SIZE);
                    dft[i][j].real += ((float)C[g][s])*1.0*cos(x+y);
                    dft[i][j].img += ((float)C[g][s])*1.0*sin(x+y);
                }
            }
        }
    }

    timeReturn = gettimeofday(&endTime, 0);
    if(timeReturn != 0){
        perror("gettimeofday");
        freeArrays();
        free(thread_id);
        exit(EXIT_FAILURE);
    }

    seconds = endTime.tv_sec - beginTime.tv_sec;
    microseconds = endTime.tv_usec - beginTime.tv_usec;
    elapsed = seconds + microseconds*1e-6;

    message = calloc(300, sizeof *message);
    sprintf(message,"Thread %d has has finished the second part in %.4f seconds.\n",threadNum,elapsed);
    printTimeStamp(message);
    free(message);

    return NULL;
}

void printTimeStamp(char* message){
    time_t     now;
    struct tm  ts;
    char       buf[40];
    time(&now);
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    printf("[%s]\t%s",buf,message);
}

void killThreads(){
    int i;
    for ( i = 0; i < m; ++i)
    {   
        if(pthread_self() != thread_id[i]){
            pthread_cancel(thread_id[i]);
        }
    }

    for(i=0; i<m; ++i){
        if(pthread_self() != thread_id[i]){
            pthread_join(thread_id[i],NULL);
        }
    }
    free(thread_id);
    exit(EXIT_SUCCESS);
}

void fatal(const char *format, ...)
{
    va_list argList;

    va_start(argList, format);
    perror("fatal");
    va_end(argList);

    exit(EXIT_FAILURE);
}